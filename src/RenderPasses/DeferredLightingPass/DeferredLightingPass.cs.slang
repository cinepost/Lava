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

import DeferredLightingHelpers;


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
  float             gBias;
  uint2             gFrameDim;
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

    TriangleHit triangleHit;
    ShadingData sd;

    let lod = createTextureSampler(ipos);
    bool isValidSurface = loadShadingData(ipos, gFrameDim, gScene.camera, gVbuffer, lod, sd, triangleHit);

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

    // Blue noise texture
    float4 bnoise = gNoiseTex.SampleLevel(gNoiseSampler, ((ipos + gNoiseOffset) / float2(64)), 0);

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
    float3 viewDir = normalize(gScene.camera.getPosition() - sd.posW);
    float3 geometryFaceNormalW = getGeometricFaceNormalW(triangleHit.instanceID, triangleHit.primitiveIndex);
    float3 ffNormalW = faceforward(geometryFaceNormalW, -viewDir, sd.faceN);

    // face forwaded interpolated normal
    if (gScene.doFixShadowTerminator(triangleHit.instanceID)) {
        rayOrigin = gScene.computeShadingPosition(triangleHit.instanceID, triangleHit.primitiveIndex, sd.posW, ffNormalW, triangleHit.getBarycentricWeights());
    }

    rayOrigin += viewDir * gBias;

    // Process lights
    
    [loop]
    for (int lightIndex = 0; lightIndex < gScene.getLightCount(); lightIndex++) {
        float3 shadowFactor = float3(1.0f);
        LightData light = gScene.getLight(lightIndex);
        AnalyticLightSample ls;

        // Sample light and reject sample if lower hemisphere.
        sd.mtl.setActiveLobes((uint)LobeType::SpecularReflection | (uint)LobeType::DiffuseReflection );
        
        if (sampleLight(sd.posW, sd.faceN, light, sampleNext2D(sg), ls) && dot(ls.dir, sd.N) > kMinCosTheta) {
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
                AOV_shadow *= shadowFactor;
            }

#ifdef _USE_MULTIPLE_IMPORTANCE_SAMPLING
            float misWeight = 1.0f;
            if (!light.isDeltaLight() && (ls.pdf > .0f)) {
                float brdfPdf = bsdf.evalPdf(sd, ls.dir);
                //misWeight = powerHeuristic(1.0f, ls.pdf, 1.0f, brdfPdf);
                misWeight = evalMIS(1.0f, ls.pdf, 1.0f, brdfPdf);
                misWeight /= ls.pdf;
            }
#else
            float misWeight = 1.0f;
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

    // Write main channel output color
    gInOutColor[ipos] = float4(surfaceColor, 1.0);
        
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

#if is_valid(gOutOcclusion)
        float pdf;

#ifdef USE_STBN_SAMPLING
        float3 dir = normalize(sample_cosine_hemisphere_oriented(bnoise.xy, sd.N, pdf));
#else
        float3 dir = normalize(sample_cosine_hemisphere_oriented(sampleNext2D(sg), sd.N, pdf));
#endif

        RayDesc ray;
        ray.Origin = rayOrigin;
        ray.Direction = dir;
        ray.TMin = .0f;

        ray.TMax = 1000.0f;

        rayShadowQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
        rayShadowQuery.Proceed();
        gOutOcclusion[ipos] = (rayShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? 0.0f : 1.0f;
#endif  // gOutOcclusion

}
