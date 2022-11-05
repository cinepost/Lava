#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
import Scene.Shading;
import Scene.RaytracingInline;

import Utils.Math.MathHelpers;
import Utils.Sampling.SampleGenerator;
import Experimental.Scene.Lights.LightHelpers;
import Experimental.Scene.Lights.EnvMapSampler;
import Experimental.Scene.Lights.EnvMapLighting;

import Experimental.Scene.Material.Fresnel;
import Experimental.Scene.Material.Microfacet;
import Experimental.Scene.Material.StandardMaterial;
import Experimental.Scene.Material.IBxDF;

#define is_valid(name) (is_valid_##name != 0)

import DeferredLightingHelpers;
import DeferredLightingAOV;

#define _FIX_SHADOW_TERMINATOR
#define _USE_MULTIPLE_IMPORTANCE_SAMPLING

// Extra input channels
Texture2D<float> gDepthBuffer;
Texture2D<PackedHitInfo> gVbuffer;
Texture2D<float4> gTextureGrads;

// Extra output channels
#ifdef _USE_DIRECT_BUFFER
RWTexture2D<float4> gDirectDiffuseBuffer;
#endif

// Extra UAV output channels
RasterizerOrderedTexture2D<float4> gOutPosition;
RasterizerOrderedTexture2D<float4> gOutAlbedo;
RasterizerOrderedTexture2D<float4> gOutShadows;
RasterizerOrderedTexture2D<float4> gOutNormals;
RasterizerOrderedTexture2D<float2> gOutMotionVecs;
RasterizerOrderedTexture2D<float>  gOutOcclusion;
RasterizerOrderedTexture2D<float>  gPrimID;
RasterizerOrderedTexture2D<float>  gOpID;

// Main output channel
RWTexture2D<float4> gInOutColor;

cbuffer PerFrameCB {
  uint2             gNoiseOffset;
  uint              gSampleNumber;                 // Interframe sample number.
  bool              gUseImportanceSampling = true; // Use BRDF importance sampling.
  int               usePdf = 0;                    // Use BRDF sampling pdf explicitly, otherwise the precomputed weight (for debugging).
  float16_t3        gColorLimit = float3(10.0, 10.0, 10.0);
  float16_t3        gIndirectColorLimit = float3(3.0, 3.0, 3.0);
  uint              gRayDiffuseLimit = 0;
  uint              gRayReflectLimit = 0;
  float             gRayBias = 0.0001;
  uint2             gFrameDim;
  uint              gSamplesPerFrame;

  EnvMapSampler     gEnvMapSampler;
};

EnvMapLighting gEnvMapLighting; 

SamplerState gNoiseSampler;
SamplerState gDepthSampler;
Texture2D gDepthTex;
Texture2D gNoiseTex;


float _computeEnvironmentMapLOD(float pdf, Texture2D environmentMap) {
    uint txw, txh;
    environmentMap.GetDimensions(txw, txh);
    return log2(pdf * txh * M_1_PI);                                // From chapter 21 in Ray Tracing Gems.
}

/** Helper to create a texture sampler instance.
    The method for computing texture level-of-detail depends on the configuration.
    \param[in] pixel Current pixel coordinates.
    \return Texture sampler instance.
*/
ITextureSampler createTextureSampler(const uint2 pixel_pos) {
    if (is_valid(gTextureGrads)) {
        float4 texGrads = gTextureGrads[pixel_pos];
        return ExplicitGradientTextureSampler(texGrads.xy, texGrads.zw);
    } else {
        return ExplicitLodTextureSampler(0.f);
    }
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 ipos = dispatchThreadId.xy;

    // early out
    if (any(ipos >= gFrameDim)) return;

    const HitInfo hit = HitInfo(gVbuffer[ipos]);
    
    if (!(hit.isValid() && hit.getType() == HitType::Triangle)) return;

    let lod = createTextureSampler(ipos);

    TriangleHit triangleHit;
    ShadingData sd;
    if(!loadShadingData(ipos, gFrameDim, gScene.camera, gVbuffer, lod, sd, triangleHit)) return;

    const float3 rayDir = normalize(gScene.camera.computeRayPinhole(ipos, gFrameDim).dir);
    float3 rayOrigin = sd.posW;
    float3 geometryFaceNormalW = getGeometricFaceNormalW(triangleHit.instanceID, triangleHit.primitiveIndex);
    float3 ffNormalW = faceforward(geometryFaceNormalW, -rayDir, sd.faceN);

    // face forwaded interpolated normal
    if (gScene.doFixShadowTerminator(triangleHit.instanceID)) {
        //rayOrigin = gScene.computeShadingPosition(triangleHit.instanceID, triangleHit.primitiveIndex, sd.posW, ffNormalW, triangleHit.getBarycentricWeights());
    }

    rayOrigin -= rayDir * gRayBias;

    // Sample generator
    SampleGenerator sg = SampleGenerator.create(ipos, gSampleNumber);

    // AOV varibles
    float3 surfaceColor = float3(0.0);
#ifdef _USE_DIRECT_BUFFER
    float4 directSurfaceBufferColor = float4(0.0, 0.0, 0.0, -1.0);
#endif

#ifdef _SHADING_RATE
    [unroll]
    for(int shadingIter = 0; shadingIter < _SHADING_RATE; shadingIter++) {
#else
    const int shadingIter = 0;
#endif

    // Spatio temporal blue noise
#ifdef _USE_STBN_SAMPLING
    float4 stbn = gNoiseTex.SampleLevel(gNoiseSampler, ((ipos + gNoiseOffset) / float2(64)), 0);
#else
    float4 stbn = float4(sampleNext2D(sg), sampleNext2D(sg));
#endif

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayShadowQuery;
    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    let bsdf = gScene.materials.getBSDF(sd, lod);

    // Process lights
    float3 diffuseColor = float3(0.0f);
    float3 specularColor = float3(0.0f);    
    [loop]
    for (int lightIndex = 0; lightIndex < gScene.getLightCount(); lightIndex++) {

        float3 shadowFactor = float3(1.0f);
        LightData light = gScene.getLight(lightIndex);
        AnalyticLightSample ls;

        // Sample light and reject sample if lower hemisphere.
        sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection | (uint)LobeType::DiffuseReflection );
        
        if (sampleLight(sd.posW, sd.faceN, light, stbn.xy, ls) && dot(ls.dir, sd.N) > kMinCosTheta) {
            if ((light.shadowType == uint(LightShadowType.RayTraced)) && (gScene.isObjectShadowsReceiving(triangleHit.instanceID))) {
                
                // Test visibility by tracing a shadow ray.
                RayDesc ray;
                ray.Origin = rayOrigin;
                ray.Direction = ls.shadowDir;
                ray.TMin = .0f;
                ray.TMax = ls.distance;

                rayShadowQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
                rayShadowQuery.Proceed();
                shadowFactor = (rayShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? light.shadowColor : float3(1.0);
            }

            float misWeight = 1.0f;

            if (!light.isDeltaLight() && (ls.pdf > .0f)) {
                float brdfPdf = bsdf.evalPdf(sd, ls.dir);
                misWeight = evalMIS(1.0f, ls.pdf, 1.0f, brdfPdf);
                misWeight /= ls.pdf;
            }

            sd.mtl.setActiveLobes((uint)LobeType::DiffuseReflection);
            diffuseColor += bsdf.eval(sd, ls.dir, sg) * ls.Li * shadowFactor * misWeight;

            sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection);
            specularColor += bsdf.eval(sd, ls.dir, sg) * ls.specularLi * shadowFactor * misWeight;
        }
    
        // Sample bsdf for non singular light
        if(!light.isDeltaLight()) {
            BSDFSample bsdfSample;

            // Disffuse Component
            sd.mtl.setActiveLobes((uint)LobeType::DiffuseReflection);
            if(bsdf.sample(sd, sg, bsdfSample, true) && (bsdfSample.pdf > 0.0f)) {
                
                // Intersect light source and reject invalid light sample.
                if (intersectLight(sd.posW, bsdfSample.wo, light, ls) && dot(ls.dir, sd.N) > kMinCosTheta) {
                    float misWeight = evalMIS(1., bsdfSample.pdf, 1., ls.pdf);
                    
                    shadowFactor = float3(1.0f);

                    RayDesc ray;
                    ray.Origin = rayOrigin;
                    ray.Direction = ls.shadowDir;
                    ray.TMin =  .0f;
                    ray.TMax = ls.distance;

                    rayShadowQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
                    rayShadowQuery.Proceed();
                    shadowFactor = (rayShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? light.shadowColor : float3(1.0f);

                    diffuseColor += bsdfSample.weight * ls.Li * shadowFactor * misWeight;
                }
            }

            // Specular Component
            sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection);
            if(bsdf.sample(sd, sg, bsdfSample, true) && (bsdfSample.pdf > 0.0f)) {
                
                // Intersect light source and reject invalid light sample.
                if (intersectLight(sd.posW, bsdfSample.wo, light, ls) && dot(ls.dir, sd.N) > kMinCosTheta) {
                    float misWeight = evalMIS(1., bsdfSample.pdf, 1., ls.pdf);
                    
                    shadowFactor = float3(1.0f);

                    RayDesc ray;
                    ray.Origin = rayOrigin;
                    ray.Direction = ls.shadowDir;
                    ray.TMin =  .0f;
                    ray.TMax = ls.distance;

                    rayShadowQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
                    rayShadowQuery.Proceed();
                    shadowFactor = (rayShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? light.shadowColor : float3(1.0f);

                    specularColor += bsdfSample.weight * ls.specularLi * shadowFactor * misWeight;
                }
            }
        }
    }

    float3 _surfaceColor = diffuseColor + specularColor;

    _surfaceColor += bsdf.getProperties(sd).emission;

    bool emissionIllumnatesObjects = false;
    if (emissionIllumnatesObjects) {
        diffuseColor += bsdf.getProperties(sd).emission;
    }

#ifdef _USE_DIRECT_BUFFER
    directSurfaceBufferColor += float4(diffuseColor, 1.0); // alpha value higher than 0 indicates valid diffuse cache sample
#endif
    
    surfaceColor += float3(min(_surfaceColor.r, (float)gColorLimit.r), min(_surfaceColor.g, (float)gColorLimit.g), min(_surfaceColor.b, (float)gColorLimit.b));

#ifdef _SHADING_RATE

    }
    surfaceColor /= (float)_SHADING_RATE;
#ifdef _USE_DIRECT_BUFFER
    directSurfaceBufferColor /= (float)_SHADING_RATE;
#endif

#endif

    // Write direct diffuse buffer
#ifdef _USE_DIRECT_BUFFER
    gDirectDiffuseBuffer[ipos] = directSurfaceBufferColor;
#endif

    // Write main channel output color
    gInOutColor[ipos] = float4(surfaceColor, 1.0);
}
