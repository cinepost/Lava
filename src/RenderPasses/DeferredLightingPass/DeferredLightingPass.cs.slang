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

float3 calcIlluminance<S : ISampleGenerator, L : ITextureSampler>(inout float3 p, inout float3 rayDir, inout HitInfo hit, inout float3 sampleWeight, inout S sg, const float4 rnd, L lod, uint rayLevel, uint maxRayLevel) {
    float3 surfaceColor = float3(0.0);
    
    TriangleHit triangleHit;
    ShadingData sd;
    if ((rayLevel > maxRayLevel) || !calcShadingData(rayDir, hit, lod, sd, triangleHit)) {
        hit = {};
        return surfaceColor;
    }

    //sd.posW = p;
    float3 rayOrigin = sd.posW; //rayLevel == 0 ? sd.posW : p;

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
            p = p + rayDir * hitT;
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

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 ipos = dispatchThreadId.xy;

    // early out
    if (any(ipos >= gFrameDim)) return;

    const HitInfo hit = HitInfo(gVbuffer[ipos]);
    TriangleHit triangleHit;
    ShadingData sd;

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

    // Create BSDF instance and query its properties.
    let bsdf = gScene.materials.getBSDF(sd, lod);
    let bsdfProperties = bsdf.getProperties(sd);

    // AOV varibles
    float3 surfaceColor = float3(0.0f);
    float3 AOV_albedo = bsdfProperties.diffuseReflectionAlbedo;
    float3 AOV_shadow = float3(1.0f, 1.0f, 1.0f);
    float  AOV_occlusion = 1.0f;

    // Lighting ray query
    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayShadowQuery;
    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    float3 rayOrigin = sd.posW;
    
    float3 geometryFaceNormalW = getGeometricFaceNormalW(triangleHit.instanceID, triangleHit.primitiveIndex);
    float3 ffNormalW = faceforward(geometryFaceNormalW, -rayDir, sd.faceN);

    // face forwaded interpolated normal
    if (gScene.doFixShadowTerminator(triangleHit.instanceID)) {
        //rayOrigin = gScene.computeShadingPosition(triangleHit.instanceID, triangleHit.primitiveIndex, sd.posW, ffNormalW, triangleHit.getBarycentricWeights());
    }

    //rayOrigin -= rayDir * gRayBias;

    const uint maxRayLevel = _MAX_RAY_LEVEL;

#ifdef _SHADING_RATE
    [unroll]
    for(int _sr = 0; _sr < _SHADING_RATE; _sr++) {
#endif

    // Spatio temporal blue noise
#ifdef _USE_STBN_SAMPLING
    float4 stbn = gNoiseTex.SampleLevel(gNoiseSampler, ((ipos + gNoiseOffset) / float2(64)), 0);
#else
    float4 stbn = float4(sampleNext2D(sg), sampleNext2D(sg));
#endif
    
    float3 _sampleWeight = float3(1.0);
    float3 _p = rayOrigin;
    float3 _rayDir = rayDir;
    HitInfo _hit = hit;
    
#if _MAX_RAY_LEVEL != 0
    [loop]
    for(uint rayLevel = 0; rayLevel <= maxRayLevel; rayLevel++) {
#else
    const float rayLevel = 0;
#endif
        surfaceColor += calcIlluminance(_p, _rayDir, _hit, _sampleWeight, sg, stbn, lod, rayLevel, maxRayLevel);
#if _MAX_RAY_LEVEL != 0
    }
#endif

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
