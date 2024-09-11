#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
import Scene.Shading;
import Scene.RaytracingInline;

import Utils.Math.MathHelpers;
import Utils.Math.PackedFormats;
import Utils.Math.FormatConversion;
import Utils.Sampling.SampleGenerator;
import Utils.Sampling.VisibilitySamplesContainer;
import Experimental.Scene.Lights.LightHelpers;
import Experimental.Scene.Lights.EnvMapSampler;
import Experimental.Scene.Lights.EmissiveLightSampler;
import Experimental.Scene.Lights.EmissiveLightSamplerHelpers;

import Experimental.Scene.Materials.Fresnel;
import Experimental.Scene.Materials.Microfacet;
import Experimental.Scene.Materials.StandardMaterial;

#define is_valid(name) (is_valid_##name != 0)

import DeferredLightingHelpers;
import DeferredLightingAOV;

#ifndef _MAX_RAY_LEVEL
#define _MAX_RAY_LEVEL 0
#endif

#ifndef USE_DEPTH_OF_FIELD
#define USE_DEPTH_OF_FIELD false
#endif

#define _FIX_SHADOW_TERMINATOR
#define _USE_MULTIPLE_IMPORTANCE_SAMPLING
#define _CACHE_NEXT_RAY

#define LUMA_REC709(f3color) dot(f3color, float3(0.2126f, 0.7152f, 0.0722f))
#define LUMA_CCIR601(f3color) dot(f3color, float3(0.299f, 0.587f, 0.114f))

#define GREY(f3color) (LUMA_REC709(f3color) + 0.5f)

static const bool kUseAnalyticLights = USE_ANALYTIC_LIGHTS;
static const bool kUseEmissiveLights = USE_EMISSIVE_LIGHTS;

static const bool kAdjustShadingNormals = true;


// Extra input channels
Texture2D<float>            gDepth;
Texture2D<PackedHitInfo>    gVbuffer;
Texture2D<float4>           gTextureGrads;
Texture2D<uint4>            gNormW;
Texture2D<uint>             gOITStartOffset;

ParameterBlock<VisibilitySamplesContainer> gVisibilityContainer;

// Extra UAV output channels
RasterizerOrderedTexture2D<float4> gOutNormals;
RasterizerOrderedTexture2D<float4> gOutFaceNormals;
RasterizerOrderedTexture2D<float>  gOutPz;
RasterizerOrderedTexture2D<float4> gOutPosition;
RasterizerOrderedTexture2D<float4> gOutAlbedo;
RasterizerOrderedTexture2D<float4> gOutEmission;
RasterizerOrderedTexture2D<float>  gOutRoughness;
RasterizerOrderedTexture2D<float4> gOutShadows;
RasterizerOrderedTexture2D<float4> gOutTangentNormals;
RasterizerOrderedTexture2D<float2> gOutMotionVecs;
RasterizerOrderedTexture2D<float>  gOutOcclusion;
RasterizerOrderedTexture2D<float>  gOutFresnel;
RasterizerOrderedTexture2D<float>  gPrimID;
RasterizerOrderedTexture2D<float>  gOpID;
RasterizerOrderedTexture2D<float16_t>  gVariance;
RasterizerOrderedTexture2D<float2> gUV;

RasterizerOrderedTexture2D<float16_t4> gLastFrameSum;

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
  float             gRayContribThresh = 0.1;
  uint              gRussRouletteLevel = 2;
  uint2             gFrameDim;
  uint              gSamplesPerFrame;

  EnvMapSampler     gEnvMapSampler;
  EmissiveLightSampler emissiveSampler;           ///< Emissive light sampler. Only valid when kUseEmissiveLights == true.
};

SamplerState gNoiseSampler;
SamplerState gDepthSampler;
Texture2D gNoiseTex;


/** write AOVData
*/
void writeAOV(uint2 ipos, const AOVData d) {
#if is_valid(gOutPz)
    gOutPz[ipos] = d.depth;
#endif  // gOutPz

#if is_valid(gOutAlbedo)
    gOutAlbedo[ipos] = float4(d.albedo, .0);
#endif  // gOutAlbedo

#if is_valid(gOutEmission)
    gOutEmission[ipos] = float4(d.emission, .0);
#endif  // gOutEmission

#if is_valid(gOutRoughness)
    gOutRoughness[ipos] = d.roughness;
#endif  // gOutRoughness

#if is_valid(gOutNormals)
    gOutNormals[ipos] = float4(d.normal, .0f);
#endif  // gOutNormals

#if is_valid(gOutFaceNormals)
    gOutFaceNormals[ipos] = float4(d.face_normal, .0f);
#endif  // gOutFaceNormals

#if is_valid(gOutTangentNormals)
    gOutTangentNormals[ipos] = float4(d.tangent_normal, .0f);
#endif  // gOutTangentNormals

#if is_valid(gOutShadows)
    gOutShadows[ipos] = float4(d.shadow, .0f);
#endif  // gOutShadows

#if is_valid(gOutPosition)
    gOutPosition[ipos] = float4(d.position, .0);
#endif  // gOutPosition

#if is_valid(gOutFresnel)
    gOutFresnel[ipos] = d.fresnel;
#endif  // gOutFresnel

#if is_valid(gPrimID)
    gPrimID[ipos] = d.primID;
#endif  // gOutFresnel

#if is_valid(gOpID)
    gOpID[ipos] = d.opID;
#endif  // gOpID

#if is_valid(gUV)
    gUV[ipos] = d.uv;
#endif  // gUV

}

enum class RayType {
    Primary                 = 0,    ///< Camera ray.
    Diffuse                 = 1,    ///< Diffuse reflection.
    Specular                = 2,    ///< Specular reflection (including delta).
    Transmission            = 3,    ///< Transmission (all kinds).
};

struct RayData {
    uint   level;   ///< Ray bounces counter
    float  length;  ///< Ray distance traveled
    float3 origin; 
    float3 dir;
    float3 weight;  ///< Ray throughput
    HitInfo hit;
    RayType type;
    
    static RayData create() {
        RayData r;
        r.level = 0;
        r.length = 0.0f;
        r.weight = float3(1.f);
        r.origin = r.dir = float3(0.0);
        r.type = RayType::Primary;
        r.hit = {};
        return r;
    }
};

/** Determine hints to use when creating the material instance.
*/
uint getMaterialInstanceHints(const HitInfo hit, const bool isPrimary) {
    uint hints = 0;
    if (hit.getType() == HitType::Triangle || hit.getType() == HitType::DisplacedTriangle) {

//#if GBUFFER_ADJUST_SHADING_NORMALS
        if (kAdjustShadingNormals || isPrimary) hints |= (uint)MaterialInstanceHints::AdjustShadingNormal;
//#else
        if (kAdjustShadingNormals && !isPrimary) hints |= (uint)MaterialInstanceHints::AdjustShadingNormal;
//#endif

    }
    return 0;//hints;
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
    #if is_valid(gTextureGrads)
        const float4 texGrads = gTextureGrads[pixel_pos];
        return ExplicitGradientTextureSampler(texGrads.xy, texGrads.zw);
    #else
        return ExplicitLodTextureSampler(0.f);
    #endif
}


float3 calcIlluminance<S : ISampleGenerator, L : ITextureSampler>(inout RayData rayData, TriangleHit triangleHit, ShadingData sd, inout S sg, inout AOVData aov_data, const float4 rnd, L lod, const uint maxRayLevel){

    const float _tmin = .001f;

    float3 surfaceColor = float3(0.0);

    const float lumaWeight = LUMA_CCIR601(rayData.weight) / gRayContribThresh;
    const float rrThreshold = rnd.x;// * ((float)rayData.level / (float)min(1, maxRayLevel));  // range should be [0, 1]
    if(gRussRouletteLevel > 0 && rayData.level >= gRussRouletteLevel && lumaWeight < rrThreshold) {
        rayData.weight = float3(0.f);
        return surfaceColor;
    }

    float3 rayOrigin = sd.posW;
    float3 rayShadowOrigin = rayOrigin;
    float3 geometryFaceNormalW = getGeometricFaceNormalW(triangleHit.instanceID, triangleHit.primitiveIndex);

    [[branch ]] 
    if (gScene.doFixShadowTerminator(triangleHit.instanceID)) {
        rayShadowOrigin = gScene.computeShadingPosition(triangleHit.instanceID, triangleHit.primitiveIndex, sd.posW, sd.frame.N, triangleHit.getBarycentricWeights());
    } else {
        rayShadowOrigin = gScene.computeShadingPosition(triangleHit.instanceID, triangleHit.primitiveIndex, triangleHit.getBarycentricWeights());
    }

    //rayOrigin -= rayData.dir * gRayBias;

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayShadowOriginQuery;
    //let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE;

    const bool isPrimaryHit = (rayData.level == 0);

    // Create BSDF instance and query its properties.
    let hints = getMaterialInstanceHints(rayData.hit, isPrimaryHit);
    const IMaterialInstance mi = gScene.materials.getMaterialInstance(sd, lod, hints);

    BSDFProperties bsdfProperties = mi.getProperties(sd);
    const float3 N = bsdfProperties.guideNormal;
    //const float3 N = sd.frame.N;

    uint shadingLobes = (uint)LobeType::DiffuseReflection | (uint)LobeType::DeltaReflection | (uint)LobeType::SpecularReflection | (uint)LobeType::SpecularTransmission | (uint)LobeType::DeltaTransmission;
    
    // Ray level common stuff
    if (rayData.level > 0) { 
        // Ray level 1+

        // Terminate specular caustics.
    } else {
        // Ray level 0 (camera ray)
        aov_data.position = sd.posW;
        aov_data.normal = N;
        aov_data.uv = sd.uv;
        aov_data.tangent_normal = sd.frame.toLocal(N);
        aov_data.albedo = bsdfProperties.diffuseReflectionAlbedo;
    }

    HitInfo bsdfHit = {}; // We store bsdf hit here as a candidate for the next bounce
    float bsdfHitT = 0.f; // We store hit distance to check agains potential light source to sample from the surface bsdf
    BSDFSample bsdfSample;

    sd.mtl.setActiveLobes(shadingLobes);
    bool validBsdfSample = mi.sample(sd, sg, bsdfSample, true);
    float3 nextRayDir = normalize(bsdfSample.wo);

    float3 diffuseColor  = float3(0.f);
    float3 specularColor = float3(0.f);
    float3 shadowColor   = float3(1.f);

    const uint lightSetIndex = gScene.getLightSetIndex(triangleHit.instanceID);
    const uint lightCount = gScene.getLightCount(lightSetIndex);

    //if(lightSetIndex == 0) surfaceColor = float3(lightCount, 0, 0);
    //if(lightSetIndex == 1) surfaceColor = float3(0, lightCount, 0);
    //if(lightSetIndex == 2) surfaceColor = float3(0, 0, lightCount);

    //return surfaceColor;

    // Process lights
    [loop]
    for (int lightIndex = 0; lightIndex < lightCount; ++lightIndex) {

        LightData light = gScene.getLight(lightSetIndex, lightIndex);
        float3 shadowFactor = float3(1.0f);
        if(rayData.level == 0) {
            if(!light.contributesDirectIllumination()) continue;
        } else {
            if(!light.contributesIndirectIllumination()) continue;
        }

        AnalyticLightSample ls;

        // Sample light and reject sample if lower hemisphere.
        sd.mtl.setActiveLobes(shadingLobes);

        if (sampleLight(sd.posW, N, light, rnd, ls) && (dot(ls.dir, N) > kMinCosTheta)) {
            [[branch]]if ((light.getShadowType() == LightShadowType.RayTraced) && (gScene.isObjectShadowsReceiving(triangleHit.instanceID))) {

                SceneRayQuery<1> rayQuery;
                const Ray ray = Ray(rayShadowOrigin, ls.shadowDir, _tmin, ls.distance);

                [[branch]] if(gScene.isObjectSelfShadowsReceiving(triangleHit.instanceID)) {
                    shadowFactor = rayQuery.traceVisibilityRay(ray, rayFlags, 0x02) ? float3(1.0) : light.shadowColor;
                } else {
                    shadowFactor = rayQuery.traceVisibilityRay(ray, rayFlags, 0x02, triangleHit.instanceID.index) ? float3(1.0) : light.shadowColor;
                }
            }

            float misDiffWeight = 1.0f;
            float misSpecWeight = 1.0f;
            float misRefrWeight = 1.0f;

#ifdef _USE_MULTIPLE_IMPORTANCE_SAMPLING            
            [[branch ]] if (!light.isDeltaLight() && (ls.pdf > .0f)) {
                sd.mtl.setActiveLobes((uint)LobeType::DiffuseReflection);
                misDiffWeight = evalMIS(1.0f, ls.pdf, 1.0f, mi.evalPdf(sd, ls.dir));
                misDiffWeight /= ls.pdf;

                sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection);
                misSpecWeight = evalMIS(1.0f, ls.pdf, 1.0f, mi.evalPdf(sd, ls.dir));
                misSpecWeight /= ls.pdf;

                sd.mtl.setActiveLobes((uint)LobeType::SpecularTransmission);
                misRefrWeight = evalMIS(1.0f, ls.pdf, 1.0f, mi.evalPdf(sd, ls.dir));
                misRefrWeight /= ls.pdf;
            }
#endif //_USE_MULTIPLE_IMPORTANCE_SAMPLING

            [[branch ]] if ((rayData.level == 0 && light.contributesDirectDiffuseIllumination()) || (rayData.level > 0 && light.contributesIndirectDiffuseIllumination())) {
                sd.mtl.setActiveLobes((uint)LobeType::DiffuseReflection);
                diffuseColor += mi.eval(sd, ls.dir, sg) * ls.Li *((rayData.level == 0) ? light.directDiffuseIntensityMultiplier : light.indirectDiffuseIntensityMultiplier ) * shadowFactor * misDiffWeight;
            }

            [[branch ]] if ((rayData.level == 0 && light.contributesDirectSpecularIllumination()) || (rayData.level > 0 && light.contributesIndirectSpecularIllumination())) {
                sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection);
                specularColor += mi.eval(sd, ls.dir, sg) * ls.Li * ((rayData.level == 0) ? light.directSpecularIntensityMultiplier : light.indirectSpecularIntensityMultiplier ) * shadowFactor * misSpecWeight;

                sd.mtl.setActiveLobes((uint)LobeType::SpecularTransmission);
                specularColor += mi.eval(sd, ls.dir, sg) * ls.Li * ((rayData.level == 0) ? light.directSpecularIntensityMultiplier : light.indirectSpecularIntensityMultiplier ) * shadowFactor * misRefrWeight;
            }

            shadowColor *= shadowFactor;
        }

#ifdef _USE_MULTIPLE_IMPORTANCE_SAMPLING

        // Sample bsdf for non singular light
        if(!light.isDeltaLight()) {
#ifdef _CACHE_NEXT_RAY
            RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayShadowOriginQuery;
            let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
#endif

            uint _shadingLobes = 0;
            [[branch ]] if ((rayData.level == 0 && light.contributesDirectDiffuseIllumination()) || (rayData.level > 0 && light.contributesIndirectDiffuseIllumination())) {
                _shadingLobes |= (uint)LobeType::DiffuseReflection;
            }

            [[branch ]] if ((rayData.level == 0 && light.contributesDirectSpecularIllumination()) || (rayData.level > 0 && light.contributesIndirectSpecularIllumination())) {
                _shadingLobes |= (uint)LobeType::SpecularReflection | (uint)LobeType::SpecularTransmission;
            }
            sd.mtl.setActiveLobes(_shadingLobes);
            
            if(mi.sample(sd, sg, bsdfSample, true)) {
                
                // Intersect light source and reject invalid light sample.
                if (intersectLight(sd.posW, bsdfSample.wo, light, ls) && dot(ls.dir, N) > kMinCosTheta) {
                    float misWeight = evalMIS(1., bsdfSample.pdf, 1., ls.pdf);
                    
                    shadowFactor = float3(1.0f);

                    RayDesc ray;
                    ray.Origin = rayShadowOrigin;
                    ray.Direction = bsdfSample.wo;
                    ray.TMin = _tmin;
                    ray.TMax = ls.distance;

                    rayShadowOriginQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
                    rayShadowOriginQuery.Proceed();

                    if(rayShadowOriginQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
                        shadowFactor = light.shadowColor;
#ifdef _CACHE_NEXT_RAY
                        bsdfHit = HitInfo(getCommittedTriangleHit(rayShadowOriginQuery));
                        bsdfHitT = rayShadowOriginQuery.CommittedRayT();
#endif
                    }

                    [branch]
                    if(bsdfSample.isLobe(LobeType::DiffuseReflection)) {
                        sd.mtl.setActiveLobes((uint)LobeType::DiffuseReflection);
                        float misWeight = evalMIS(1., mi.evalPdf(sd, bsdfSample.wo), 1., ls.pdf);
                        diffuseColor += bsdfSample.weight * ls.Li * ((rayData.level == 0) ? light.directDiffuseIntensityMultiplier : light.indirectDiffuseIntensityMultiplier ) * shadowFactor * misWeight;
                    } else if(bsdfSample.isLobe(LobeType::SpecularReflection)) {
                        sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection);
                        float misWeight = evalMIS(1., mi.evalPdf(sd, bsdfSample.wo), 1., ls.pdf);
                        specularColor += bsdfSample.weight * ls.Li * ((rayData.level == 0) ? light.directSpecularIntensityMultiplier : light.indirectSpecularIntensityMultiplier ) * shadowFactor * misWeight;
                    } else if(bsdfSample.isLobe(LobeType::SpecularTransmission) || bsdfSample.isLobe(LobeType::DeltaTransmission)) {
                        sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection);
                        float misWeight = evalMIS(1., mi.evalPdf(sd, bsdfSample.wo), 1., ls.pdf);
                        specularColor += bsdfSample.weight * ls.Li * ((rayData.level == 0) ? light.directSpecularIntensityMultiplier : light.indirectSpecularIntensityMultiplier ) * shadowFactor * misWeight;
                    }
                }
            }
        }
#endif  // _USE_MULTIPLE_IMPORTANCE_SAMPLING

    } // light loop

    aov_data.shadow += float3(1.f) - shadowColor;

    float3 _surfaceColor = diffuseColor + specularColor;

    bool emissionIllumnatesObjects = false;
    _surfaceColor += ((rayData.type == RayType::Primary) || (rayData.type == RayType::Specular) || ((rayData.type == RayType::Diffuse) && emissionIllumnatesObjects)) ? 
        mi.getProperties(sd).emission : float3(0.0);
    
    _surfaceColor *= rayData.weight;
    
    float3 colorLimit = rayData.level == 0 ? (float3)gColorLimit : (float3)gIndirectColorLimit;
    surfaceColor = float3(min(_surfaceColor.r, colorLimit.r), min(_surfaceColor.g, colorLimit.g), min(_surfaceColor.b, colorLimit.b));
    
    [[branch ]] if( rayData.level >= maxRayLevel) return surfaceColor;

    /// Next hit point
    rayData.hit = {};
    
    uint lobes = (uint)LobeType::None;

    if(rayData.level < gRayDiffuseLimit) lobes |= (uint)LobeType::DiffuseReflection;
    if(rayData.level < gRayReflectLimit) lobes |= (uint)LobeType::SpecularReflection | (uint)LobeType::DeltaReflection;
    if(rayData.level < gRayRefractLimit) lobes |= (uint)LobeType::SpecularTransmission | (uint)LobeType::DeltaTransmission;
    
    sd.mtl.setActiveLobes(lobes);

#ifdef _CACHE_NEXT_RAY    
    //Invalid bsdf sample. Terminate here
    [[branch ]] if(!validBsdfSample) {
        return surfaceColor;
    }

    if(bsdfHitT > 0.0 && (1 == 2)) {
        // Reuse found surface hit
        
        rayData.weight *= bsdfSample.weight;
        rayData.dir = normalize(bsdfSample.wo);   
        rayData.hit = bsdfHit;
        rayData.length += bsdfHitT;

    } else {
        // Find next hit
        
        if(mi.sample(sd, sg, bsdfSample, true)) {
            RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE> rayQuery;
            let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE;

            bool useAlphaTest = false;
            
            rayData.weight *= bsdfSample.weight;
            rayData.dir = normalize(bsdfSample.wo);

            RayDesc ray;
            ray.Origin = sd.posW;
            ray.Direction = rayData.dir;
            ray.TMin =  .001f;
            ray.TMax = 100000.0f;

            rayQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
            rayQuery.Proceed();

            if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
                rayData.hit = HitInfo(getCommittedTriangleHit(rayQuery));
                rayData.length += rayQuery.CommittedRayT();

            } else {
                if(bsdfSample.isLobe(LobeType::SpecularTransmission) || bsdfSample.isLobe(LobeType::DeltaTransmission) ||
                   bsdfSample.isLobe(LobeType::DeltaTransmission) || bsdfSample.isLobe(LobeType::DeltaReflection)) {

                    [loop]
                    for (int lightIndex = 0; lightIndex < gScene.getLightCount(); lightIndex++) {
                        LightData light = gScene.getLight(lightIndex);
                        if(!light.isDeltaLight()) {
                            AnalyticLightSample ls;
                            if (intersectLight(sd.posW, rayData.dir, light, ls)) {
                                surfaceColor += rayData.weight * ls.Li * ((rayData.level == 0) ? light.directSpecularIntensityMultiplier : light.indirectSpecularIntensityMultiplier );
                            }
                        }
                    }
                }
            }   
        }
    }
#endif

    return surfaceColor;
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 ipos = dispatchThreadId.xy;

    // early out
    if (any(ipos >= gFrameDim)) return;

    #if is_valid(gTextureGrads)
        const float4 texGrads = gTextureGrads[ipos];
    #endif

#if defined(USE_VISIBILITY_CONTAINER)
    const HitInfo hit = gVisibilityContainer.readOpaqueHit(ipos);
#else
    const HitInfo hit = HitInfo(gVbuffer[ipos]);
#endif

    AOVData aov_data = AOVData.create();
    
    if (!hit.isValid() || hit.getType() != HitType::Triangle) {
        writeAOV(ipos, aov_data);   
        return;
    }

    // Sample generator
    SampleGenerator sg = SampleGenerator(ipos, gSampleNumber);

#if USE_DEPTH_OF_FIELD
    const float3 rayDir = normalize(gScene.camera.computeRayThinlens(ipos, gFrameDim, sampleNext2D(sg)).dir);
#else
    const float3 rayDir = normalize(gScene.camera.computeRayPinhole(ipos, gFrameDim).dir);
#endif

    ITextureSampler primaryLod = createTextureSampler(ipos);
    //ITextureSampler primaryLod = TailTextureSampler();
    //ITextureSampler primaryLod = ExplicitLodTextureSampler(4.f);
    
    // Primary ray surface data
    TriangleHit primaryTriangleHit = hit.getTriangleHit();
    ShadingData primaryShadingData;

    if(gScene.isObjectMatteShaded(primaryTriangleHit.instanceID)) return;
    
    VertexData v = gScene.getVertexData(primaryTriangleHit);

#if is_valid(gNormW)
    if(gScene.isObjectSubdividable(primaryTriangleHit.instanceID)) {

        uint3 packedNormal = gNormW[ipos].xyz;

        v.normalW = {
            unpackSnorm16(packedNormal.x >> 16),
            unpackSnorm16(packedNormal.y >> 16),
            unpackSnorm16(packedNormal.z >> 16)
        };

        v.faceNormalW = {
            unpackSnorm16(packedNormal.x & 0x0000ffff),
            unpackSnorm16(packedNormal.y & 0x0000ffff),
            unpackSnorm16(packedNormal.z & 0x0000ffff)
        };

        const uint materialID = gScene.getMaterialID(primaryTriangleHit);
        primaryShadingData = gScene.materials.prepareShadingData(v, materialID, -rayDir, primaryLod);
    } else {
        calcPrimaryShadingData(rayDir, primaryTriangleHit, primaryLod, primaryShadingData);
    }
#else
    calcPrimaryShadingData(rayDir, primaryTriangleHit, primaryLod, primaryShadingData);
#endif

    aov_data.face_normal = v.faceNormalW;

    float2 screenUV = float2(ipos) / float2(gFrameDim); 

    // Beauty pass color
    float3 surfaceColor = float3(0.0f);

    const uint maxRayLevel = _MAX_RAY_LEVEL;
    float16_t variance = 1.f;

#if is_valid(gLastFrameSum) && _USE_VARIANCE
    const float16_t4 _lastFrameSum = gLastFrameSum[ipos];
    const float16_t3 lastFrameSum = _lastFrameSum.rgb;
    const float16_t lastVariance = _lastFrameSum.a;
#else
    const float16_t3 lastFrameSum = float16_t3(0.0);
    const float16_t lastVariance = 1.0;
#endif

#if _USE_VARIANCE
    if(gSampleNumber > 16 && lastVariance < 0.0005f) {
        gInOutColor[ipos] = float4(lastFrameSum, 1.f);
        return;
    }
#endif

#ifdef _SHADING_RATE
    [loop]
    for(int shadingIter = 0; shadingIter < _SHADING_RATE; shadingIter++) {
#else
    const int shadingIter = 0;
#endif

    // Spatio temporal blue noise
#ifdef _USE_STBN_SAMPLING
    const float4 rnd = gNoiseTex.SampleLevel(gNoiseSampler, float2((ipos + gNoiseOffset + uint2(sampleNext2D(sg) * uint2(64))) / float2(64)), 0);
#else
    const float4 rnd = float4(sampleNext2D(sg), sampleNext2D(sg));
#endif
    
    RayData rayData = RayData.create();
    rayData.dir = rayDir;
    rayData.hit = hit;
    
    TriangleHit _triangleHit = primaryTriangleHit;
    ShadingData _shadingData = primaryShadingData;
    ITextureSampler _lod = primaryLod;
    
#if _MAX_RAY_LEVEL != 0
    [loop]
    for(uint rayLevel = 0; rayLevel <= maxRayLevel; rayLevel++) {
        if(rayLevel > 0) {
            //_lod = ExplicitLodTextureSampler(5.0);
            _lod = TailTextureSampler();
            if(!calcShadingData(rayData.dir, rayData.hit, _lod, _shadingData, _triangleHit)) break;
        }
#else
        const uint rayLevel = 0;
#endif
        rayData.level = rayLevel;
        surfaceColor += calcIlluminance(rayData, _triangleHit, _shadingData, sg, aov_data, rnd, _lod, maxRayLevel);

#if _MAX_RAY_LEVEL != 0
        if(!rayData.hit.isValid()) break;
    }
#endif

#ifdef _SHADING_RATE
    }
    surfaceColor /= (float)(_SHADING_RATE);
#endif

// Compute variance if needed
#if _USE_VARIANCE
    if(gSampleNumber > 16) {
        float lumaPrev = LUMA_CCIR601(lastFrameSum / float(gSampleNumber));
        float lumaCurr = LUMA_CCIR601((lastFrameSum + surfaceColor) / float(gSampleNumber + 1));
        variance = 1.f - (lumaPrev > lumaCurr ? (lumaCurr/lumaPrev) : (lumaPrev/lumaCurr));
    }
#endif

    // Write main channel output color
    gInOutColor[ipos] = float4(min(surfaceColor.r, gColorLimit.r), min(surfaceColor.g, gColorLimit.g), min(surfaceColor.b, gColorLimit.b), 1.0);

    // Calc aov variables
#if is_valid(gOutPz)
    {
    #if is_valid(gDepth)
        float2 _screenUV = screenUV * 2.0 - float2(1.f);
        float4 screenP = float4(_screenUV.x, _screenUV.y, gDepth[ipos], 1.0);
    #else
        float4 screenP = mul(float4(primaryShadingData.posW, 1.f), gScene.camera.getViewProjMat());
    #endif  // gDepth
        float4 viewP = mul(screenP, gScene.camera.data.invProjMat);
        aov_data.depth = -(viewP.z / viewP.w);
    }
#endif  // gOutPz

#if is_valid(gOutFresnel)
    {
        aov_data.fresnel = evalFresnelSchlick(0.f, 1.f, dot(primaryShadingData.V, primaryShadingData.frame.N));
    }
#endif

#if is_valid(gOutRoughness)
    {
        const IMaterialInstance mi = gScene.materials.getMaterialInstance(primaryShadingData, primaryLod);
        aov_data.roughness = mi.getProperties(primaryShadingData).roughness;
    }
#endif  // gOutEmission

#if is_valid(gOutEmission)
    {
        const IMaterialInstance mi = gScene.materials.getMaterialInstance(primaryShadingData, primaryLod);
        aov_data.emission = mi.getProperties(primaryShadingData).emission;
    }
#endif  // gOutEmission

#if is_valid(gOutShadows)
    #ifdef _SHADING_RATE
        aov_data.shadow /= (float)_SHADING_RATE;
    #endif
#endif  // gOutShadows

#if is_valid(gPrimID)
    {
        aov_data.primID = primaryTriangleHit.primitiveIndex;
    }
#endif

#if is_valid(gOpID)
    {
        aov_data.opID = primaryTriangleHit.instanceID.index;
    }
#endif

    // Write available AOVs
    writeAOV(ipos, aov_data);

#if is_valid(gLastFrameSum) && _USE_VARIANCE
    {
        gLastFrameSum[ipos] = float16_t4(lastFrameSum + surfaceColor, variance);
    }
#endif

#if is_valid(gVariance) && _USE_VARIANCE
    {
        gVariance[ipos] = variance;
    }
#endif

}
