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

import Experimental.Scene.Material.Fresnel;
import Experimental.Scene.Material.Microfacet;
import Experimental.Scene.Material.StandardMaterial;
import Experimental.Scene.Material.IBxDF;


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

//RaytracingAccelerationStructure gTlas;

//EnvMapLighting gEnvMapLighting; 

SamplerState gSampler;
SamplerState gNoiseSampler;
SamplerState gDepthSampler;

Texture2D gDepthTex;
Texture2D gNoiseTex;

static VSOut vsData;


VSOut vs(VSIn vIn)
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
    float4 normal : SV_TARGET1;
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
PsOut ps(VSOut vsOut, uint triangleIndex : SV_PrimitiveID) {
    PsOut psOut;

    // pixel position
    int2 ipos = int2(vsOut.posH.xy);

    // pseudo-random sample generator
    SampleGenerator sg = SampleGenerator.create(ipos, gSampleNumber);

    // blue noise
    float4 noise = gNoiseTex.SampleLevel(gNoiseSampler, ((ipos + gNoiseOffset) / float2(64)), 0);

    // screen space texture coordinates gradient
    float2 gradX = ddx_fine(vsOut.texC) * 0.5; ///< Gradient of texture coordinate in the screen-space X direction
    float2 gradY = ddy_fine(vsOut.texC) * 0.5; ///< Gradient of texture coordiante in teh screen-space Y direction
    
    float3 viewDir = normalize(gScene.camera.getPosition() - vsOut.posW);
    
    ShadingData sd = prepareShadingData(vsOut, triangleIndex, viewDir, gradX, gradY);

    // Compute GGX alpha.
    float alpha = sd.linearRoughness * sd.linearRoughness;

    StandardMaterial mtl;

    float4 finalColor = float4(0, 0, 0, 1);

    // Direct lighting from analytic light sources
    
    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayQuery;
    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    float3 rayOrigin = sd.posW;
    float3 shadowFactor;

    for (int lightIndex = 0; lightIndex < gScene.getLightCount(); lightIndex++) {
        shadowFactor = float3(1.0, 1.0, 1.0);

        LightData light = gScene.getLight(lightIndex);

        AnalyticLightSample ls;
        bool valid = sampleLight(sd.posW, light, sampleNext2D(sg), ls);

        // Reject sample if lower hemisphere.
        if (!valid || dot(ls.dir, sd.N) <= kMinCosTheta) continue;

        if (light.shadowType == uint(LightShadowType.RayTraced) ) {
            
            // Test visibility by tracing a shadow ray.
            RayDesc ray;
            ray.Origin = rayOrigin;// + viewDir * 0.0001f;
            ray.Direction = ls.dir;
            ray.TMin = 0.0001f;
            ray.TMax = 1e4f;

            rayQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0xff, ray);
            rayQuery.Proceed();
            shadowFactor = (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? light.shadowColor : float3(1.0, 1.0, 1.0);
        }

        finalColor.rgb += mtl.eval(sd, ls.dir) * ls.Li * shadowFactor;
    }

    // Add the emissive component
    finalColor.rgb += sd.emissive;
    finalColor.a = sd.opacity;

#ifdef _USE_ENV_MAP
    // Add lighting from environment map

    #ifdef _USE_SIMPLIFIED_ENV_LIGHTING
        finalColor.rgb += gEnvMapLighting.evalDiffuse(sd);
        finalColor.rgb += gEnvMapLighting.evalSpecular(sd, reflect(-sd.V, sd.N));
    #else


        // Environment diffuse
        float envSampleWeight = 1.0;
        EnvMapSample envSample;
        gEnvMapSampler.sample(sampleNext2D(sg), envSample);

/*
        if ( dot(envSample.dir, sd.N) <= kMinCosTheta) {
            gEnvMapSampler.sample(sampleNext2D(sg), envSample);
            envSampleWeight = 0.5;
        }
        if ( dot(envSample.dir, sd.N) <= kMinCosTheta) {
            gEnvMapSampler.sample(sampleNext2D(sg), envSample);
            envSampleWeight = 0.333333;
        }
        if ( dot(envSample.dir, sd.N) <= kMinCosTheta) {
            gEnvMapSampler.sample(sampleNext2D(sg), envSample);
            envSampleWeight = 0.25;
        }
        if ( dot(envSample.dir, sd.N) <= kMinCosTheta) {
            gEnvMapSampler.sample(sampleNext2D(sg), envSample);
            envSampleWeight = 0.2;
        }
*/

        if ( dot(envSample.dir, sd.N) <= kMinCosTheta) {
            DiffuseReflectionFrostbite diffuseReflection;
            diffuseReflection.albedo = sd.diffuse;
            diffuseReflection.linearRoughness = sd.linearRoughness;

            float pdf = 0.0;
            float3 weight = float3(1.0, 1.0, 1.0);
            uint lobe;
            float3 wi;
            float3 wo = sd.toLocal(sd.V);

            bool result = diffuseReflection.sample(wo, wi, pdf, weight, lobe, sg);

            float3 diffDir = normalize(sd.fromLocal(wi));

            shadowFactor = 1.0;

            {
                RayDesc ray;
                ray.Origin = rayOrigin;
                ray.Direction = diffDir;
                ray.TMin = 0.001f;
                ray.TMax = 1e4f;
            
                rayQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0xff, ray);
                rayQuery.Proceed();
                shadowFactor = (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? 0.0 : 1.0;
            }

            float diffLod = 6.0; // TODO: calculate based on pdf
            float3 Le = gScene.envMap.eval(diffDir, diffLod);
            finalColor.rgb += (diffuseReflection.eval(wo, wi) * Le * shadowFactor);

        } else {

            shadowFactor = 1.0;

            {
                RayDesc ray;
                ray.Origin = rayOrigin;
                ray.Direction = envSample.dir;
                ray.TMin = 0.001f;
                ray.TMax = 1e4f;
            
                rayQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0xff, ray);
                rayQuery.Proceed();
                shadowFactor = (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? 0.0 : 1.0;
            }
            
            if ( envSample.pdf > 0) {
                DiffuseReflectionFrostbite diffuseReflection;
                diffuseReflection.albedo = sd.diffuse;
                diffuseReflection.linearRoughness = sd.linearRoughness;

                float3 wi = sd.toLocal(envSample.dir);
                float3 wo = sd.toLocal(sd.V);
                float3 diffuse = diffuseReflection.eval(wo, wi);

                float envLod = _computeEnvironmentMapLOD(envSample.pdf, gScene.envMap.envMap);
                float3 Le = envSample.Le; //gEnvMapSampler.eval(envSample.dir, 4.0); //envSample.Le

                finalColor.rgb += diffuse * (Le / envSample.pdf) * shadowFactor * envSampleWeight;
            }
        }


        // Environment specular
        SpecularReflectionMicrofacet specularReflection;

        const uint activeLobes = sd.getActiveLobes();

        specularReflection.albedo = sd.specular;
        specularReflection.alpha = alpha;
        specularReflection.activeLobes = (uint)LobeType::SpecularReflection; //activeLobes;

        float pdf = 0.0;
        float3 weight = float3(1.0, 1.0, 1.0);
        uint lobe;
        float3 wi;
        float3 wo = sd.toLocal(sd.V);
        
        float3 reflDir;
        bool valid = true;

        if (alpha == 0.0) {
            pdf = 0.0;
            reflDir = normalize(sd.fromLocal(float3(-wo.x, -wo.y, wo.z)));
            weight = evalFresnelSchlick(sd.specular, 1.f, wo.z);
        } else {
            specularReflection.sample(wo, wi,  pdf, weight, lobe, sg);
            reflDir = normalize(sd.fromLocal(wi));
        }


        //finalColor.rgb = float3(0,0,0);

        if ( valid && dot(reflDir, sd.N) > kMinCosTheta) {
            shadowFactor = 1.0;

            {
                RayDesc ray;
                ray.Origin = rayOrigin;
                ray.Direction = reflDir;
                ray.TMin = 0.001f;
                ray.TMax = 1e4f;
            
                rayQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0xff, ray);
                rayQuery.Proceed();
                shadowFactor = (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? 0.0 : 1.0;
            }

            //float envReflPdf = gEnvMapSampler.evalPdf(reflDir);

            float envLod = 1.0; //_computeEnvironmentMapLOD(envReflPdf, gScene.envMap.envMap);
            finalColor.rgb += gScene.envMap.eval(reflDir, envLod) * weight * shadowFactor;

            //finalColor.rgb = float3(1.0 / pdf, 0.1 / pdf, 0.01 / pdf);
        }

    #endif
#endif

    psOut.color = finalColor;
    psOut.normal = float4(sd.N * 0.5f + 0.5f, 1.0f);

#ifdef _OUTPUT_MOTION_VECTORS
    // Using vsOut.posH.xy as pixel coordinate since it has the SV_Position semantic.
    psOut.motion = calcMotionVector(vsOut.posH.xy, vsOut.prevPosH, gRenderTargetDim);
#endif

//#if defined(_VISUALIZE_CASCADES) && defined(_ENABLE_SHADOWS)
//    float3 cascadeColor = visibilityBuffer.Load(int3(vsOut.posH.xy, 0)).gba;
//    psOut.color.rgb *= cascadeColor;
//#endif
    return psOut;
}
