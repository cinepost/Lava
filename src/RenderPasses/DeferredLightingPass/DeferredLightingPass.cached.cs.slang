#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
import Scene.Shading;
import Scene.RaytracingInline;

import Utils.Math.MathHelpers;
import Utils.Sampling.SampleGenerator;
import Experimental.Scene.Lights.LightHelpers;
import Experimental.Scene.Lights.EnvMapSampler;
import Experimental.Scene.Lights.EnvMapLighting;

import Experimental.Scene.Materials.Fresnel;
import Experimental.Scene.Materials.Microfacet;
import Experimental.Scene.Materials.StandardMaterial;
import Experimental.Scene.Materials.IBxDF;

#define is_valid(name) (is_valid_##name != 0)

import DeferredLightingHelpers;
import DeferredLightingAOV;

#ifndef _MAX_RAY_LEVEL
#define _MAX_RAY_LEVEL 0
#endif

#define _FIX_SHADOW_TERMINATOR
#define _USE_MULTIPLE_IMPORTANCE_SAMPLING

// Extra input channels
Texture2D<PackedHitInfo> gVbuffer;
Texture2D<float4> gTextureGrads;

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
  float3            gColorLimit = float3(10.0, 10.0, 10.0);
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

#ifdef _SHADING_RATE
struct ShadingDataCache {
    ShadingData sd;
    TriangleHit triangleHit;
    bool is_valid = false;
}
#endif


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

float3 calcIlluminance<S : ISampleGenerator, L : ITextureSampler>(inout ShadingData sd, inout float3 rayDir, inout HitInfo hit, const TriangleHit triangleHit, inout float3 sampleWeight, inout S sg, const float4 rnd, L lod, uint rayLevel, uint maxRayLevel) {

    float3 surfaceColor = float3(0.0);
    float3 rayOrigin = sd.posW;
    float3 geometryFaceNormalW = getGeometricFaceNormalW(triangleHit.instanceID, triangleHit.primitiveIndex);
    float3 ffNormalW = faceforward(geometryFaceNormalW, -rayDir, sd.faceN);

    // face forwaded interpolated normal
    if (gScene.doFixShadowTerminator(triangleHit.instanceID)) {
        //rayOrigin = gScene.computeShadingPosition(triangleHit.instanceID, triangleHit.primitiveIndex, sd.posW, ffNormalW, triangleHit.getBarycentricWeights());
    }

    rayOrigin -= rayDir * gRayBias;

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayShadowQuery;
    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

#ifdef _USE_MULTIPLE_IMPORTANCE_SAMPLING
    let bsdf = gScene.materials.getBSDF(sd, lod);
#endif

    // Process lights    
    [loop]
    for (int lightIndex = 0; lightIndex < gScene.getLightCount(); lightIndex++) {
        float3 shadowFactor = float3(1.0f);
        LightData light = gScene.getLight(lightIndex);
        AnalyticLightSample ls;

        // Sample light and reject sample if lower hemisphere.
        if( rayLevel == 0) {
            sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection | (uint)LobeType::DiffuseReflection );
        } else {
            sd.mtl.setActiveLobes((uint)LobeType::DiffuseReflection );
        }

        if (sampleLight(sd.posW, sd.faceN, light, rnd.xy, ls) && dot(ls.dir, sd.N) > kMinCosTheta) {
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
                //AOV_shadow *= shadowFactor;
            }

            float misWeight = 1.0f;

#ifdef _USE_MULTIPLE_IMPORTANCE_SAMPLING            
            if (!light.isDeltaLight() && (ls.pdf > .0f)) {
                float brdfPdf = bsdf.evalPdf(sd, ls.dir);
                misWeight = evalMIS(1.0f, ls.pdf, 1.0f, brdfPdf);
                misWeight /= ls.pdf;
            }
#endif //_USE_MULTIPLE_IMPORTANCE_SAMPLING

            surfaceColor += bsdf.eval(sd, ls.dir, sg) * ls.Li * shadowFactor * misWeight;
        }
    
#ifdef _USE_MULTIPLE_IMPORTANCE_SAMPLING

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

                    surfaceColor += bsdfSample.weight * ls.Li * shadowFactor * misWeight;
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

                    surfaceColor += bsdfSample.weight * ls.Li * shadowFactor * misWeight;
                }
            }
        }
#endif  // _USE_MULTIPLE_IMPORTANCE_SAMPLING

    }

    if(rayLevel < 1) {
        surfaceColor += bsdf.getProperties(sd).emission;
    }

    surfaceColor = float3(min(surfaceColor.r, gColorLimit.r), min(surfaceColor.g, gColorLimit.g), min(surfaceColor.b, gColorLimit.b));
    surfaceColor *= sampleWeight;

    if( rayLevel >= maxRayLevel) return surfaceColor;

    /// Next hit point
    hit = {};
    float hitT = 0.f;

    uint lobes = (uint)LobeType::None;

    if(rayLevel < gRayDiffuseLimit) lobes |= (uint)LobeType::DiffuseReflection;
    if(rayLevel < gRayReflectLimit) lobes |= (uint)LobeType::SpecularReflection;
    
    sd.mtl.setActiveLobes(lobes);
    //sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection | (uint)LobeType::DiffuseReflection );

    BSDFSample bsdfSample;
    if(bsdf.sample(sd, sg, bsdfSample, true) && (bsdfSample.pdf > 0.0f)) {

        sampleWeight *= bsdfSample.weight;

        RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE> rayQuery;
        let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE;

        bool useAlphaTest = false;
        rayDir = normalize(bsdfSample.wo);

        RayDesc ray;
        ray.Origin = rayOrigin;
        ray.Direction = rayDir;
        ray.TMin =  .0f;
        ray.TMax = 100000.0f;

        rayQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
        rayQuery.Proceed();

        if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
            hit = HitInfo(getCommittedTriangleHit(rayQuery));
            hitT = rayQuery.CommittedRayT();
            //p = p + rayDir * hitT;
        }

    }
    
#if is_valid(gOutOcclusion)
    if(rayLevel == 0) {
        float pdf;
        float3 dir = normalize(sample_cosine_hemisphere_oriented(rnd.xy, sd.N, pdf));

        RayDesc ray;
        ray.Origin = rayOrigin;
        ray.Direction = dir;
        ray.TMin = .0f;
        ray.TMax = 1.0f;

        rayShadowQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
        rayShadowQuery.Proceed();
        gOutOcclusion[ipos] = (rayShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? 0.0f : 1.0f;
    }
#endif  // gOutOcclusion

    return surfaceColor;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 ipos = dispatchThreadId.xy;

    // early out
    if (any(ipos >= gFrameDim)) return;

    const HitInfo hit = HitInfo(gVbuffer[ipos]);
    const float3 rayDir = normalize(gScene.camera.computeRayPinhole(ipos, gFrameDim).dir);

    let lod = createTextureSampler(ipos);
    bool isValidSurface = (hit.isValid() && hit.getType() == HitType::Triangle);

    if (!isValidSurface) {
#if is_valid(gOutAlbedo)
        gOutAlbedo[ipos] = float4(.0f);
#endif  // gOutAlbedo

#if is_valid(gOutNormals)
        gOutNormals[ipos] = float4(.0f);
#endif  // gOutNormals

#if is_valid(gOutShadows)
        gOutShadows[ipos] = float4(.0f);
#endif  // gOutShadows

#if is_valid(gOutPosition)
        gOutPosition[ipos] = float4(.0f);
#endif  // gOutPosition

#if is_valid(gOutOcclusion)
        gOutOcclusion[ipos] = .0f;
#endif  // gOutOcclusion
        
        return;
    }

    // Sample generator
    SampleGenerator sg = SampleGenerator.create(ipos, gSampleNumber);

    // AOV varibles
    float3 surfaceColor = float3(0.0f);
    float3 AOV_albedo = float3(0.0f, 0.0f, 0.0f);
    float3 AOV_shadow = float3(1.0f, 1.0f, 1.0f);
    float  AOV_occlusion = 1.0f;

    const uint maxRayLevel = _MAX_RAY_LEVEL;

#ifdef _SHADING_RATE
    ShadingDataCache shadingDataCache;
    shadingDataCache.is_valid = false;

    [loop]
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
    
    float3 _sampleWeight = float3(1.0);
    float3 _rayDir = rayDir;
    HitInfo _hit = hit;
    TriangleHit _triangleHit;
    ShadingData _sd;
    bool    _shadingDataIsValid = false;
    
#ifdef _SHADING_RATE
    if((shadingIter > 1) && shadingDataCache.is_valid) {
        _shadingDataIsValid = true;
        _sd = shadingDataCache.sd;
        _triangleHit = shadingDataCache.triangleHit;
    } else {
        shadingDataCache.is_valid = _shadingDataIsValid = calcShadingData(rayDir, _hit, lod, _sd, _triangleHit);
        shadingDataCache.sd = _sd;
        shadingDataCache.triangleHit = _triangleHit;
    }
#else
    _shadingDataIsValid = calcShadingData(_rayDir, _hit, lod, _sd, _triangleHit);
#endif

    
#if _MAX_RAY_LEVEL != 0
    [loop]
    for(uint rayLevel = 0; rayLevel <= maxRayLevel; rayLevel++) {
#else
    const uint rayLevel = 0;
#endif
        if(rayLevel > 0) _shadingDataIsValid = calcShadingData(_rayDir, _hit, lod, _sd, _triangleHit);
        
        if(_shadingDataIsValid) {
            surfaceColor += calcIlluminance(_sd, _rayDir, _hit, _triangleHit, _sampleWeight, sg, stbn, lod, rayLevel, maxRayLevel);
            if (all(_sampleWeight < float3(0.01f))) break;
        } else {
            break;
        }
#if _MAX_RAY_LEVEL != 0
    }
#endif  // _MAX_RAY_LEVEL != 0

#ifdef _SHADING_RATE
    }
    surfaceColor /= (float)_SHADING_RATE;
#endif

    // Write main channel output color
    gInOutColor[ipos] = float4(min(surfaceColor.r, gColorLimit.r), min(surfaceColor.g, gColorLimit.g), min(surfaceColor.b, gColorLimit.b), 1.0);
        
    // Write available AOVs
#if is_valid(gOutAlbedo)
        gOutAlbedo[ipos] = float4(AOV_albedo, 1.0f);
#endif  // gOutAlbedo

#if is_valid(gOutNormals)
#ifdef _AOV_NORMALS_FLOAT
        gOutNormals[ipos] = float4(sd.N, 1.0f);
#else
        gOutNormals[ipos] = float4(sd.N * 0.5f + 0.5f, 1.0f);
#endif
#endif  // gOutNormals

#if is_valid(gOutShadows)
        gOutShadows[ipos] = float4(AOV_shadow, 1.0f);
#endif  // gOutShadows

#if is_valid(gOutPosition)
        gOutPosition[ipos] = float4(sd.posW, 1.0f);
#endif  // gOutPosition

}
