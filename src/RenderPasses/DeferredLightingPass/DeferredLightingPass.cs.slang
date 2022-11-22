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
#define _CACHE_NEXT_RAY

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
  float16_t3        gColorLimit = float3(10.0, 10.0, 10.0);
  float16_t3        gIndirectColorLimit = float3(3.0, 3.0, 3.0);
  uint              gRayDiffuseLimit = 0;
  uint              gRayReflectLimit = 0;
  uint              gRayRefractLimit = 0;
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


/** write AOVData
*/
void writeAOV(uint2 ipos, const AOVData d) {
#if is_valid(gOutAlbedo)
        gOutAlbedo[ipos] = float4(d.albedo, .0);
#endif  // gOutAlbedo

#if is_valid(gOutNormals)
        gOutNormals[ipos] = float4(d.normal, .0f);
#endif  // gOutNormals

#if is_valid(gOutShadows)
        gOutShadows[ipos] = float4(d.shadow, .0f);
#endif  // gOutShadows

#if is_valid(gOutPosition)
        gOutPosition[ipos] = float4(d.position, .0);
#endif  // gOutPosition

#if is_valid(gOutOcclusion)
        gOutOcclusion[ipos] = d.occlusion;
#endif  // gOutOcclusion
}

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

float3 calcIlluminance<S : ISampleGenerator, L : ITextureSampler>(inout float3 rayDir, inout HitInfo hit, inout float3 sampleWeight, inout S sg, inout AOVData aov_data, const float4 rnd, L lod, uint rayLevel, uint maxRayLevel){

    float3 surfaceColor = float3(0.0);
    
    TriangleHit triangleHit;
    ShadingData sd;

    ITextureSampler _lod = lod; //ExplicitLodTextureSampler(4.0);

    [branch]
    if ((rayLevel > maxRayLevel) || !calcShadingData(rayDir, hit, _lod, sd, triangleHit)) {
        hit = {};
        return surfaceColor;
    }

    float3 rayOrigin = sd.posW;
    float3 geometryFaceNormalW = getGeometricFaceNormalW(triangleHit.instanceID, triangleHit.primitiveIndex);

    // face forwaded interpolated normal
    float3 ffNormalW = faceforward(geometryFaceNormalW, -rayDir, sd.faceN);

    if (gScene.doFixShadowTerminator(triangleHit.instanceID)) {
        //rayOrigin = gScene.computeShadingPosition(triangleHit.instanceID, triangleHit.primitiveIndex, sd.posW, ffNormalW, triangleHit.getBarycentricWeights());
    }

    //rayOrigin -= rayDir * gRayBias;

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayShadowQuery;
    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    let bsdf = gScene.materials.getBSDF(sd, _lod);
    let bsdfProperties = bsdf.getProperties(sd);

    uint shadingLobes = (uint)LobeType::SpecularReflection | (uint)LobeType::DiffuseReflection;
    
    // Ray level common stuff
    if (rayLevel > 0) { 
        // Ray level 1+

        // Terminate specular caustics.
        shadingLobes = (uint)LobeType::DiffuseReflection | (uint)LobeType::SpecularTransmission;
    } else {
        // Ray level 0 (camera ray)
        aov_data.position = sd.posW;
        aov_data.normal = sd.N;
        aov_data.albedo = bsdfProperties.diffuseReflectionAlbedo;
    }

    HitInfo bsdfHit = {};  // We store bsdf hit here as a candidate for the next bounce
    float bsdfHitT = 0.0f; // We store hit distance to check agains potential light source to sample from the surface bsdf
    BSDFSample bsdfSample;

    sd.mtl.setActiveLobes(shadingLobes);
    bool validBsdfSample = bsdf.sample(sd, sg, bsdfSample, true);
    float3 nextRayDir = normalize(bsdfSample.wo);

    float3 diffuseColor = float3(0.0f);
    float3 specularColor = float3(0.0f);

    // Process lights
#if _SCENE_LIGHT_COUNT
    [unroll]
    for (int lightIndex = 0; lightIndex < _SCENE_LIGHT_COUNT; lightIndex++) {
#else
    [loop]
    for (int lightIndex = 0; lightIndex < gScene.getLightCount(); lightIndex++) {
#endif

        LightData light = gScene.getLight(lightIndex);
        float3 shadowFactor = float3(1.0f);
        if(rayLevel == 0) {
            if(!light.contributesDirectIllumination()) continue;
        } else {
            if(!light.contributesIndirectIllumination()) continue;
        }

        AnalyticLightSample ls;

        // Sample light and reject sample if lower hemisphere.
        sd.mtl.setActiveLobes(shadingLobes);

        [branch]
        if (sampleLight(sd.posW, sd.N, light, rnd.xy, ls) && dot(ls.dir, sd.N) > kMinCosTheta) {
            if ((light.shadowType == uint(LightShadowType.RayTraced)) && (gScene.isObjectShadowsReceiving(triangleHit.instanceID))) {
                
                // Test visibility by tracing a shadow ray.
                RayDesc ray;
                ray.Origin = rayOrigin;
                ray.Direction = ls.shadowDir;
                ray.TMin = .001f;
                ray.TMax = ls.distance;

                rayShadowQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
                rayShadowQuery.Proceed();
                shadowFactor = (rayShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? light.shadowColor : float3(1.0);
                //AOV_shadow *= shadowFactor;
            }

            float misDiffWeight = 1.0f;
            float misSpecWeight = 1.0f;

#ifdef _USE_MULTIPLE_IMPORTANCE_SAMPLING            
            if (!light.isDeltaLight() && (ls.pdf > .0f)) {
                sd.mtl.setActiveLobes((uint)LobeType::DiffuseReflection);
                misDiffWeight = evalMIS(1.0f, ls.pdf, 1.0f, bsdf.evalPdf(sd, ls.dir));
                misDiffWeight /= ls.pdf;

                sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection);
                misSpecWeight = evalMIS(1.0f, ls.pdf, 1.0f, bsdf.evalPdf(sd, ls.dir));
                misSpecWeight /= ls.pdf;

                //misSpecWeight = misDiffWeight;
            }
#endif //_USE_MULTIPLE_IMPORTANCE_SAMPLING

            sd.mtl.setActiveLobes((uint)LobeType::DiffuseReflection);
            diffuseColor += bsdf.eval(sd, ls.dir, sg) * ((rayLevel == 0) ? light.directDiffuseIntensity : light.indirectDiffuseIntensity ) * shadowFactor * misDiffWeight;

            sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection);
            specularColor += bsdf.eval(sd, ls.dir, sg) * ((rayLevel == 0) ? light.directSpecularIntensity : light.indirectSpecularIntensity ) * shadowFactor * misSpecWeight;
        }
    
#ifdef _USE_MULTIPLE_IMPORTANCE_SAMPLING

        // Sample bsdf for non singular light
        [branch]
        if(!light.isDeltaLight()) {
#ifdef _CACHE_NEXT_RAY
            RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE> rayShadowQuery;
            let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE;
#endif

            if(validBsdfSample && (bsdfSample.pdf > 0.0f)) {
                
                // Intersect light source and reject invalid light sample.
                if (intersectLight(sd.posW, bsdfSample.wo, light, ls) && dot(ls.dir, sd.N) > kMinCosTheta) {
                    float misWeight = evalMIS(1., bsdfSample.pdf, 1., ls.pdf);
                    
                    shadowFactor = float3(1.0f);

                    RayDesc ray;
                    ray.Origin = rayOrigin;
                    ray.Direction = nextRayDir;
                    ray.TMin = .001f;
                    ray.TMax = ls.distance;

                    rayShadowQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
                    rayShadowQuery.Proceed();

                    if(rayShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
                        shadowFactor = light.shadowColor;
#ifdef _CACHE_NEXT_RAY
                        bsdfHit = HitInfo(getCommittedTriangleHit(rayShadowQuery));
                        bsdfHitT = rayShadowQuery.CommittedRayT();
#endif
                    }

                    if(bsdfSample.isLobe(LobeType::DiffuseReflection)) {
                        sd.mtl.setActiveLobes((uint)LobeType::DiffuseReflection);
                        float misWeight = evalMIS(1., bsdf.evalPdf(sd, bsdfSample.wo), 1., ls.pdf);
                        diffuseColor += bsdfSample.weight * ((rayLevel == 0) ? light.directDiffuseIntensity : light.indirectDiffuseIntensity ) * shadowFactor * misWeight;
                    } else if(bsdfSample.isLobe(LobeType::SpecularReflection)) {
                        sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection);
                        float misWeight = evalMIS(1., bsdf.evalPdf(sd, bsdfSample.wo), 1., ls.pdf);
                        specularColor += bsdfSample.weight * ((rayLevel == 0) ? light.directSpecularIntensity : light.indirectSpecularIntensity ) * shadowFactor * misWeight;
                    }
                }
            }
        }
#endif  // _USE_MULTIPLE_IMPORTANCE_SAMPLING

    } // light loop

    float3 _surfaceColor = diffuseColor + specularColor;

    bool emissionIllumnatesObjects = false;
    _surfaceColor += ((rayLevel == 0) || emissionIllumnatesObjects) ? bsdf.getProperties(sd).emission : float3(0.0);
    _surfaceColor *= sampleWeight;
    
    float3 colorLimit = rayLevel == 0 ? (float3)gColorLimit : (float3)gIndirectColorLimit;
    surfaceColor = float3(min(_surfaceColor.r, colorLimit.r), min(_surfaceColor.g, colorLimit.g), min(_surfaceColor.b, colorLimit.b));
    
    [branch]
    if( rayLevel >= maxRayLevel) return surfaceColor;

    /// Next hit point
    hit = {};
    float hitT = 0.f;

    uint lobes = (uint)LobeType::None;

    if(rayLevel < gRayDiffuseLimit) lobes |= (uint)LobeType::DiffuseReflection;
    if(rayLevel < gRayReflectLimit) lobes |= (uint)LobeType::SpecularReflection;
    if(rayLevel < gRayRefractLimit) lobes |= (uint)LobeType::SpecularTransmission; //(uint)LobeType::DeltaTransmission;
    
    sd.mtl.setActiveLobes(lobes);

#ifdef _CACHE_NEXT_RAY
    
    //Invalid bsdf sample. Terminate here
    if(!validBsdfSample || (bsdfSample.pdf <= 0.0f)) {
        return surfaceColor;
    }

    if(bsdfHitT > 0.0) {
        // Reuse found surface hit
        
        sampleWeight *= bsdfSample.weight;
        rayDir = normalize(bsdfSample.wo);   
        hit = bsdfHit;
        hitT = bsdfHitT;

    } else {
        // Find next hit

        if(bsdf.sample(sd, sg, bsdfSample, true) && (bsdfSample.pdf > 0.0f)) {
            RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE> rayQuery;
            let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE;

            bool useAlphaTest = false;
            
            sampleWeight *= bsdfSample.weight;
            rayDir = normalize(bsdfSample.wo);

            RayDesc ray;
            ray.Origin = sd.posW;
            ray.Direction = rayDir;
            ray.TMin =  .001f;
            ray.TMax = 100000.0f;

            rayQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
            rayQuery.Proceed();

            if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
                hit = HitInfo(getCommittedTriangleHit(rayQuery));
                hitT = rayQuery.CommittedRayT();
            }
        }
    }
#else
    [branch]
    if(bsdf.sample(sd, sg, bsdfSample, true) && (bsdfSample.pdf > 0.0f)) {

        RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE> rayQuery;
        let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE;

        bool useAlphaTest = false;

        sampleWeight *= bsdfSample.weight;
        rayDir = normalize(bsdfSample.wo);

        RayDesc ray;
        ray.Origin = sd.posW;
        ray.Direction = rayDir;
        ray.TMin =  .001f;
        ray.TMax = 100000.0f;

        rayQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
        rayQuery.Proceed();

        if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
            hit = HitInfo(getCommittedTriangleHit(rayQuery));
            hitT = rayQuery.CommittedRayT();
            //p = p + rayDir * hitT;
        }
    }
#endif
    
#if is_valid(gOutOcclusion)
    if(rayLevel == 0) {
        float pdf;
        float3 dir = normalize(sample_cosine_hemisphere_oriented(rnd.xy, sd.N, pdf));

        RayDesc ray;
        ray.Origin = rayOrigin;
        ray.Direction = dir;
        ray.TMin = .001f;
        ray.TMax = 1.0f;

        rayShadowQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
        rayShadowQuery.Proceed();
        aov_data.occlusion = (rayShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? 0.0f : 1.0f;
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
    AOVData aov_data = AOVData.create();
    
    if (!(hit.isValid() && hit.getType() == HitType::Triangle)) {
        writeAOV(ipos, aov_data);        
        return;
    }

    const float3 rayDir = normalize(gScene.camera.computeRayPinhole(ipos, gFrameDim).dir);
    let lod = createTextureSampler(ipos);

    // Sample generator
    SampleGenerator sg = SampleGenerator.create(ipos, gSampleNumber);

    // Beauty pass color
    float3 surfaceColor = float3(0.0f);

    const uint maxRayLevel = _MAX_RAY_LEVEL;

#ifdef _SHADING_RATE
    [loop]
    //[unroll]
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
    
#if _MAX_RAY_LEVEL != 0
    [loop]
    for(uint rayLevel = 0; rayLevel <= maxRayLevel; rayLevel++) {
#else
    const float rayLevel = 0;
#endif
        surfaceColor += calcIlluminance(_rayDir, _hit, _sampleWeight, sg, aov_data, stbn, lod, rayLevel, maxRayLevel);
#if _MAX_RAY_LEVEL != 0
        if(!_hit.isValid()) break;
    }
#endif

#ifdef _SHADING_RATE
    }
    surfaceColor /= (float)_SHADING_RATE;
#endif

    // Write main channel output color
    gInOutColor[ipos] = float4(min(surfaceColor.r, gColorLimit.r), min(surfaceColor.g, gColorLimit.g), min(surfaceColor.b, gColorLimit.b), 1.0);
        
    // Write available AOVs
    writeAOV(ipos, aov_data);
}
