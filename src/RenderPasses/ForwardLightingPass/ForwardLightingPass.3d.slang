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
#define _USE_MULTIPLE_IMPORTANCE_SAMPLING

// UAV output channels
RasterizerOrderedTexture2D<float4> gOutPosition;
RasterizerOrderedTexture2D<float4> gOutAlbedo;
RasterizerOrderedTexture2D<float4> gOutShadows;
RasterizerOrderedTexture2D<float4> gOutNormals;
RasterizerOrderedTexture2D<float2> gOutMotionVecs;
RasterizerOrderedTexture2D<float>  gOutOcclusion;
RasterizerOrderedTexture2D<float>  gPrimID;
RasterizerOrderedTexture2D<float>  gOpID;

cbuffer PerFrameCB {
  uint2             gNoiseOffset;
  uint              gSampleNumber;                 // Interframe sample number.
  bool              gUseImportanceSampling = true; // Use BRDF importance sampling.
  int               usePdf = 0;                    // Use BRDF sampling pdf explicitly, otherwise the precomputed weight (for debugging).

  float2            gRenderTargetDim;              // frame dimension
  uint              gSamplesPerFrame;

  float4x4          gViewInvMat;

  EnvMapSampler     gEnvMapSampler;
};

#define is_valid(name) (is_valid_##name != 0)

EnvMapLighting gEnvMapLighting; 

SamplerState gNoiseSampler;
SamplerState gDepthSampler;

Texture2D gDepthTex;
Texture2D gNoiseTex;

static VSOut vsData;


VSOut vsMain(VSIn vIn) {
    VSOut vsOut;
    vsOut = defaultVS(vIn);
#ifdef _OUTPUT_MOTION_VECTORS
    vsOut.prevPosH.xy += vsOut.prevPosH.w * 2 * float2(gScene.camera.data.jitterX, gScene.camera.data.jitterY);
#endif
    return vsOut;
}

struct PsOut {
    float4 color : SV_TARGET0;
};


float _computeEnvironmentMapLOD(float pdf, Texture2D environmentMap) {
    uint txw, txh;
    environmentMap.GetDimensions(txw, txh);
    return log2(pdf * txh * M_1_PI);                                // From chapter 21 in Ray Tracing Gems.
}

float power_heuristic(float nf, float fPdf, float ng, float gPdf){
    float f = nf * fPdf;
    float g = ng * gPdf;
    return (f*f)/(f*f + g*g);
}

/** Evaluates the currently configured heuristic for multiple importance sampling (MIS).
    \param[in] n0 Number of samples taken from the first sampling strategy.
    \param[in] p0 Pdf for the first sampling strategy.
    \param[in] n1 Number of samples taken from the second sampling strategy.
    \param[in] p1 Pdf for the second sampling strategy.
    \return Weight for the contribution from the first strategy (p0).
*/
float evalMIS(float n0, float p0, float n1, float p1) {
    // Power two heuristic
    float q0 = (n0 * p0) * (n0 * p0);
    float q1 = (n1 * p1) * (n1 * p1);
    return q0 / (q0 + q1);
}



[earlydepthstencil]
PsOut psMain(VSOut vsOut, uint triangleIndex : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics) {
    PsOut psOut;

    if(gScene.isObjectMatteShaded(vsOut.instanceID)) discard;

    let lod = ImplicitLodTextureSampler();
    if (alphaTest(vsOut, triangleIndex, lod)) discard;

    // pixel position
    int2 ipos = int2(vsOut.posH.xy);

    // pseudo-random sample generator
    SampleGenerator sg = SampleGenerator.create(ipos, gSampleNumber);

    // screen space texture coordinates gradient
    float2 gradX = ddx_fine(vsOut.texC) * 0.5f; ///< Gradient of texture coordinate in the screen-space X direction
    float2 gradY = ddy_fine(vsOut.texC) * 0.5f; ///< Gradient of texture coordiante in teh screen-space Y direction
    
    float3 viewDir = normalize(gScene.camera.getPosition() - vsOut.posW);
    
    //ShadingData sd = prepareShadingData(vsOut, triangleIndex, viewDir, gradX, gradY);
    ShadingData sd = prepareShadingData(vsOut, triangleIndex, viewDir, lod);

    // Create BSDF instance and query its properties.
    let bsdf = gScene.materials.getBSDF(sd, lod);
    let bsdfProperties = bsdf.getProperties(sd);


    // Main output variables
    float3 surfaceColor = float3(0.0f);
    float  surfaceAlpha = 0.0f;

    // AOV varibles
    float3 AOV_albedo = bsdfProperties.diffuseReflectionAlbedo;
    float3 AOV_shadow = float3(1.0f, 1.0f, 1.0f);
    float  AOV_occlusion = 1.0f;

    // Direct lighting from analytic light sources
    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayShadowQuery;
    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    float3 rayOrigin = sd.posW;

#ifdef _FIX_SHADOW_TERMINATOR
    // face forwaded interpolated normal
    float3 ffNormalW = faceforward(normalize(vsOut.normalW), -viewDir, sd.faceN);
    if (gScene.doFixShadowTerminator(vsOut.instanceID)) {
        rayOrigin = gScene.computeShadingPosition(vsOut.instanceID, triangleIndex, vsOut.posW, ffNormalW, barycentrics);
    }
#endif

    rayOrigin += viewDir * 0.001f;

    [loop]
    for (int lightIndex = 0; lightIndex < gScene.getLightCount(); lightIndex++) {
        float3 shadowFactor = float3(1.0f);
        float misWeight = 1.f;

        LightData light = gScene.getLight(lightIndex);

        AnalyticLightSample ls;
        
        // Sample light and reject sample if lower hemisphere.
        if (sampleLight(sd.posW, sd.faceN, light, sampleNext2D(sg), ls) && dot(ls.dir, sd.N) > kMinCosTheta) {
        
            if ((light.shadowType == uint(LightShadowType.RayTraced)) && (gScene.isObjectShadowsReceiving(vsOut.instanceID))) {
                
                // Test visibility by tracing a shadow ray.
                RayDesc ray;
                ray.Origin = rayOrigin;
                ray.Direction = ls.shadowDir;
                ray.TMin = .00001f;

                ray.TMax = ls.distance;

                rayShadowQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
                rayShadowQuery.Proceed();
                shadowFactor = (rayShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? light.shadowColor : float3(1.0f);
            }

#ifdef _USE_MULTIPLE_IMPORTANCE_SAMPLING
            misWeight = 1.0f;
            if (!light.isDeltaLight() && (ls.pdf > .0f)) {
                float brdfPdf = bsdf.evalPdf(sd, ls.dir);
                misWeight = power_heuristic(1.0f, ls.pdf, 1.0f, brdfPdf);
                misWeight /= ls.pdf;
            }
#else
            misWeight = 1.0f;
#endif //_USE_MULTIPLE_IMPORTANCE_SAMPLING

            surfaceColor += bsdf.eval(sd, ls.dir, sg) * ls.Li * shadowFactor * misWeight;// * float3(1.0, 0.0, 0.0);
        }

        
#ifdef _USE_MULTIPLE_IMPORTANCE_SAMPLING

        // Sample bsdf for non singular light
        if(!light.isDeltaLight()) {
            BSDFSample bsdfSample;
            if(bsdf.sample(sd, sg, bsdfSample, true) && (bsdfSample.pdf > 0.0f)) {
                
                // Intersect light source and reject invalid light sample.
                if (intersectLight(sd.posW, bsdfSample.wo, light, ls) && dot(ls.dir, sd.N) > kMinCosTheta) {
                    misWeight = power_heuristic(1., bsdfSample.pdf, 1., ls.pdf);

                    shadowFactor = float3(1.0f);

                    RayDesc ray;
                    ray.Origin = rayOrigin;
                    ray.Direction = ls.shadowDir;
                    ray.TMin = .00001f;

                    ray.TMax = ls.distance;

                    rayShadowQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
                    rayShadowQuery.Proceed();
                    shadowFactor = (rayShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? light.shadowColor : float3(1.0f);

                    surfaceColor += bsdfSample.weight * ls.Li * shadowFactor * misWeight;// * float3(0.0, 1.0, 0.0);
                }
            }
        }
#endif  // _USE_MULTIPLE_IMPORTANCE_SAMPLING

        AOV_shadow *= shadowFactor;
    }

    // Add the emissive component
    surfaceColor += bsdf.getProperties(sd).emission;
    
    // Surface alpha
    surfaceAlpha = max(psOut.color.a, sd.opacity);

    // Final pixel color
    psOut.color.rgb = surfaceColor; //lerp(psOut.color.rgb, surfaceColor, surfaceAlpha);
    psOut.color.a = 1.f; //surfaceAlpha;

    // Write available AOVs
    if (is_valid(gOutAlbedo)){
        gOutAlbedo[ipos] = float4(AOV_albedo, 1.0f);
    }

    if (is_valid(gOutNormals)){
        gOutNormals[ipos] = float4(sd.N * 0.5f + 0.5f, 1.0f);
    }

    if (is_valid(gOutShadows)){
        gOutShadows[ipos] = float4(AOV_shadow, 1.0f);
    }

    if (is_valid(gOutPosition)){
        gOutPosition[ipos] = float4(sd.posW, 1.0f);
    }

    if (is_valid(gOutOcclusion)){
        gOutOcclusion[ipos] = AOV_occlusion;
    }

    if (is_valid(gPrimID)){
        gPrimID[ipos] = 0.0f;
    }

    if (is_valid(gOpID)){
        gOpID[ipos] = (float)vsOut.instanceID.index;
    }

    if (is_valid(gOutMotionVecs)){
        // Using vsOut.posH.xy as pixel coordinate since it has the SV_Position semantic.
        gOutMotionVecs[ipos] = calcMotionVector(vsOut.posH.xy, vsOut.prevPosH, gRenderTargetDim);
    }

    return psOut;
}
