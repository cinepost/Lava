/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
import Scene.Shading;

import Utils.Math.MathHelpers;
import Utils.Sampling.SampleGenerator;
import Experimental.Scene.Lights.LightHelpers;
import Experimental.Scene.Lights.EnvMapSampler;
import Experimental.Scene.Lights.EnvMapLighting;


import Experimental.Scene.Material.Fresnel;
import Experimental.Scene.Material.Microfacet;
import Experimental.Scene.Material.StandardMaterial;
import Experimental.Scene.Material.IBxDF;

#define _FIX_SHADOW_TERMINATOR

cbuffer PerFrameCB {
  uint2             gNoiseOffset;
  uint              gSampleNumber;        // Interframe sample number.
  int               useBrdfSampling = 1;  // Use BRDF importance sampling.
  int               usePdf = 0;           // Use BRDF sampling pdf explicitly, otherwise the precomputed weight (for debugging).

  float2            gRenderTargetDim; // frame dimension
  uint              gSamplesPerFrame;

  float4x4          gViewInvMat;

  EnvMapSampler     gEnvMapSampler;
};

EnvMapLighting gEnvMapLighting; 

SamplerState gSampler;
SamplerState gNoiseSampler;
SamplerState gDepthSampler;

Texture2D gDepthTex;
Texture2D gNoiseTex;

static VSOut vsData;


VSOut vsMain(VSIn vIn)
{
    VSOut vsOut;
    vsOut = defaultVS(vIn);
#ifdef _OUTPUT_MOTION_VECTORS
    vsOut.prevPosH.xy += vsOut.prevPosH.w * 2 * float2(gScene.camera.data.jitterX, gScene.camera.data.jitterY);
#endif
    return vsOut;
}

struct PsOut
{
    float4 color : SV_TARGET0;

#ifdef _OUTPUT_NORMALS
    float4 normal : SV_TARGET1;
#endif

#ifdef _OUTPUT_MOTION_VECTORS
    float2 motion : SV_TARGET2;
#endif
};


float _computeEnvironmentMapLOD(float pdf, Texture2D environmentMap) {
    uint txw, txh;
    environmentMap.GetDimensions(txw, txh);
    return log2(pdf * txh * M_1_PI);                                // From chapter 21 in Ray Tracing Gems.
}


[earlydepthstencil]
PsOut psMain(VSOut vsOut, uint triangleIndex : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics) {
    PsOut psOut;

    if(gScene.isObjectMatteShaded(vsOut.instanceID)) discard;

    let lod = ImplicitLodTextureSampler();
    if (alphaTest(vsOut, triangleIndex, lod)) discard;

    // pixel position
    int2 pixel = int2(vsOut.posH.xy);

    // pseudo-random sample generator
    SampleGenerator sg = SampleGenerator.create(pixel, gSampleNumber);

    // screen space texture coordinates gradient
    float2 gradX = ddx_fine(vsOut.texC) * 0.5; ///< Gradient of texture coordinate in the screen-space X direction
    float2 gradY = ddy_fine(vsOut.texC) * 0.5; ///< Gradient of texture coordiante in teh screen-space Y direction
    
    float3 viewDir = normalize(gScene.camera.getPosition() - vsOut.posW);

    //ShadingData sd = prepareShadingData(vsOut, triangleIndex, viewDir, gradX, gradY);
    ShadingData sd = prepareShadingData(vsOut, triangleIndex, viewDir, lod);

//#ifdef _ADJUST_SHADING_POSITION
//    sd.posW = posWA;
//#endif

    // Create BSDF instance and query its properties.
    let bsdf = gScene.materials.getBSDF(sd, lod);
    let bsdfProperties = bsdf.getProperties(sd);


    float3 surfaceColor = float3(0, 0, 0);
    float  surfaceAlpha = 0.0;

    
    // Direct lighting from analytic light sources
    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayShadowQuery;
    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    float3 rayOrigin = sd.posW;

#ifdef _FIX_SHADOW_TERMINATOR
    if (gScene.doFixShadowTerminator(vsOut.instanceID)) {
        rayOrigin = gScene.computeShadingPosition(vsOut.instanceID, triangleIndex, vsOut.posW, normalize(vsOut.normalW), barycentrics);
    }
#endif
    
    for (int lightIndex = 0; lightIndex < gScene.getLightCount(); lightIndex++) {
        float3 shadowFactor = float3(1.0, 1.0, 1.0);

        LightData light = gScene.getLight(lightIndex);

        AnalyticLightSample ls;
        bool valid = sampleLight(sd.posW, light, sampleNext2D(sg), ls);

        // Reject sample if lower hemisphere.
        if (!valid || dot(ls.dir, sd.N) <= kMinCosTheta) continue;
        

        if ((light.shadowType == uint(LightShadowType.RayTraced)) && (gScene.isObjectShadowsReceiving(vsOut.instanceID))) {
            
            // Test visibility by tracing a shadow ray.
            RayDesc ray;
            ray.Origin = rayOrigin;// + viewDir * 0.0001f;
            ray.Direction = ls.dir;
            ray.TMin = .01f;

            switch (light.type) {
                case LightType::Distant:
                case LightType::Directional:
                    ray.TMax = 1e4f;
                    break;
                default:
                    ray.TMax = length(ls.posW - sd.posW);
                    break;
            }

            rayShadowQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
            rayShadowQuery.Proceed();
            shadowFactor = (rayShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? light.shadowColor : float3(1.0, 1.0, 1.0);
        }

        surfaceColor += bsdf.eval(sd, ls.dir, sg) * ls.Li * shadowFactor;
    }

    // Add the emissive component
    surfaceColor += bsdf.getProperties(sd).emission;
    
    // Surface alpha
    surfaceAlpha = max(psOut.color.a, sd.opacity);

    // Final pixel color
    psOut.color.rgb = surfaceColor; //lerp(psOut.color.rgb, surfaceColor, surfaceAlpha);
    psOut.color.a = 1.f; //surfaceAlpha;

    //psOut.color.rg = vsOut.texC;

#ifdef _OUTPUT_NORMALS
    psOut.normal = float4(sd.N * 0.5f + 0.5f, 1.0f);
#endif

#ifdef _OUTPUT_MOTION_VECTORS
    // Using vsOut.posH.xy as pixel coordinate since it has the SV_Position semantic.
    psOut.motion = calcMotionVector(vsOut.posH.xy, vsOut.prevPosH, gRenderTargetDim);
#endif

    return psOut;
}
