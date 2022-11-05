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
import DeferredLightingScreenTraceHelpers;
import DeferredLightingAOV;

#ifndef _MAX_RAY_LEVEL
#define _MAX_RAY_LEVEL 0
#endif

#define _FIX_SHADOW_TERMINATOR
#define _USE_MULTIPLE_IMPORTANCE_SAMPLING

// Extra input channels
Texture2D<PackedHitInfo> gVbuffer;
Texture2D<float4> gTextureGrads;

#ifdef _USE_DIRECT_BUFFER
Texture2D<float> gDepthBuffer;
Texture2D<float4> gDirectDiffuseBuffer;
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
  float3            gClipInfo;
  float4x4          gViewToTextureSpaceMatrix;

  EnvMapSampler     gEnvMapSampler;
};

EnvMapLighting gEnvMapLighting; 

SamplerState gDepthSampler;
SamplerState gNoiseSampler;
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

float3 calcIlluminance<S : ISampleGenerator, L : ITextureSampler>(inout float3 rayDir, inout HitInfo hit, inout float3 sampleWeight, inout S sg, const float4 rnd, L lod, uint rayLevel, uint maxRayLevel){

    float3 surfaceColor = float3(0.0);
    
    TriangleHit triangleHit;
    ShadingData sd;

    if ((rayLevel > maxRayLevel) || !calcShadingData(rayDir, hit, lod, sd, triangleHit)) {
        hit = {};
        return surfaceColor;
    }

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

    let bsdf = gScene.materials.getBSDF(sd, lod);

    float cPdf = 0.0f;
    float3 cDir = float3(0.0f);

    // Process lights    
    [loop]
    for (int lightIndex = 0; lightIndex < gScene.getLightCount(); lightIndex++) {
        float3 shadowFactor = float3(1.0f);
        LightData light = gScene.getLight(lightIndex);
        AnalyticLightSample ls;

        // Sample light and reject sample if lower hemisphere.
        sd.mtl.setActiveLobes((uint)LobeType::DiffuseReflection );       
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
                shadowFactor = (rayShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? light.shadowColor : float3(1.0f);
                //AOV_shadow *= shadowFactor;
            }

            float misWeight = 1.0f;

            if (!light.isDeltaLight() && (ls.pdf > .0f)) {
                float brdfPdf = bsdf.evalPdf(sd, ls.dir);
                misWeight = evalMIS(1.0f, ls.pdf, 1.0f, brdfPdf);
                misWeight /= ls.pdf;
            }

            surfaceColor += bsdf.eval(sd, ls.dir, sg) * ls.Li * shadowFactor * misWeight;
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
    }

    surfaceColor = float3(min(surfaceColor.r, gColorLimit.r), min(surfaceColor.g, gColorLimit.g), min(surfaceColor.b, gColorLimit.b)) * sampleWeight;

    if( rayLevel >= maxRayLevel) return surfaceColor;

    /// Next hit point
    hit = {};
    float hitT = 0.f;

    uint lobes = (uint)LobeType::None;

    if(rayLevel < gRayDiffuseLimit) lobes |= (uint)LobeType::DiffuseReflection;
    if(rayLevel < gRayReflectLimit) lobes |= (uint)LobeType::SpecularReflection;
    
    sd.mtl.setActiveLobes(lobes);
    
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
        }
    }
    return surfaceColor;
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 ipos = dispatchThreadId.xy;

    // early out
    if (any(ipos >= gFrameDim)) return;

    let lod = createTextureSampler(ipos);
    
    ShadingData sd;
    TriangleHit triangleHit;
    if(!loadShadingData(ipos, gFrameDim, gScene.camera, gVbuffer, lod, sd, triangleHit)) return;

    // Sample generator
    SampleGenerator sg = SampleGenerator.create(ipos, gSampleNumber);

    // AOV varibles
    float3 surfaceColor = float3(0.0);

    let bsdf = gScene.materials.getBSDF(sd, lod);
    const uint maxRayLevel = _MAX_RAY_LEVEL;

    //gInOutColor[ipos] = float4(0.0, 0.0, 0.0, 1.0);

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE> rayQuery;
    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE;

    float4x4 projectToPixelMatrix = gScene.camera.data.viewMat;


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
    
    sd.mtl.setActiveLobes((uint)LobeType::DiffuseReflection | (uint)LobeType::SpecularReflection);
    
    BSDFSample bsdfSample;
    if(bsdf.sample(sd, sg, bsdfSample, true) && (bsdfSample.pdf > 0.0f)) {
        float3 _sampleWeight = bsdfSample.weight;
        float3 _rayDir = normalize(bsdfSample.wo);

        float maxSteps = 100.0f;
        float maxRayTraceDistance = 10.0f;
        float csZThickness = 1.0;
        bool csZBufferIsHyperbolic = false;
        
        float3 csOrigin = mul(float4( sd.posW, 1.f), gScene.camera.data.viewMat).xyz;
        float3 csDirection = normalize(mul(float4( _rayDir, 1.f), gScene.camera.data.viewMat).xyz);

        //float3 csOrigin = mul(float4( sd.posW, 1.f), gScene.camera.getViewProj()).xyz;
        //float3 csDirection = normalize(mul(float4( _rayDir, 1.f), gScene.camera.getViewProj()).xyz);

        float stride = 1.0f;
        float jitterFraction = 0.0f; 

        float2 hitPixel;
        int which;
        float3 csHitPoint;
/*
        bool traced = traceScreenSpaceRay1(
            csOrigin, 
            csDirection,
            gScene.camera.data.projMat,
            gDepthBuffer,
            gDepthSampler,
            (float2)gFrameDim,
            csZThickness,
            csZBufferIsHyperbolic,
            gClipInfo,
            gScene.camera.data.nearZ,
            stride,
            jitterFraction,
            maxSteps,
            maxRayTraceDistance,
            hitPixel,
            which,
            csHitPoint);

        if( traced) {
            //surfaceColor += float3(0.0, 1.0, 0.0);
            //surfaceColor += gDirectDiffuseBuffer[int2(hitPixel)].rgb;
        }
*/
        float4 screenPt = mul(float4(csOrigin, 1.0), gViewToTextureSpaceMatrix);
        //surfaceColor += screenPt.xyz / screenPt.w;
        surfaceColor += csOrigin;

/*
        RayDesc ray;
        ray.Origin = sd.posW;
        ray.Direction = _rayDir;
        ray.TMin =  .0001f;
        ray.TMax = 100000.0f;

        rayQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
        rayQuery.Proceed();

        if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
            HitInfo _hit = HitInfo(getCommittedTriangleHit(rayQuery));
            //hitT = rayQuery.CommittedRayT();
            
            [loop]
            for(uint rayLevel = 1; rayLevel <= maxRayLevel; rayLevel++) {
                surfaceColor += calcIlluminance(_rayDir, _hit, _sampleWeight, sg, stbn, lod, rayLevel, maxRayLevel);
                //surfaceColor += float3(0.0, 1.0, 0.0);
            }
        }
*/
    }

#ifdef _SHADING_RATE
    }
    surfaceColor /= (float)_SHADING_RATE;
#endif

    // Write main channel output color
    float4 inColor = gInOutColor[ipos];
    //gInOutColor[ipos] = inColor + float4(min(surfaceColor.r, gColorLimit.r), min(surfaceColor.g, gColorLimit.g), min(surfaceColor.b, gColorLimit.b), 0.0);
    gInOutColor[ipos] = float4(min(surfaceColor.r, gColorLimit.r), min(surfaceColor.g, gColorLimit.g), min(surfaceColor.b, gColorLimit.b), 0.0);
    

    // Write available AOVs
}
