/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
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

/** Whitted ray tracer.

    The purpose is to use it as a baseline for showing how to implement different
    texture level-of-detail filtering methods.

    The host sets the following defines:

    MAX_BOUNCES                 Maximum number of indirect bounces (0 means no indirect).
    USE_ANALYTIC_LIGHTS         Nonzero if Falcor's analytic lights should be used.
    USE_EMISSIVE_LIGHTS         Nonzero if emissive geometry should be used as lights.
    USE_ENV_LIGHT               Nonzero if env map is available and should be used as light source.
    USE_ENV_BACKGROUND          Nonzero if env map is available and should be used as background.
    TEX_LOD_MODE                Could be either of Mip0, RayCones, RayDiffs.
    RAY_CONE_MODE               Could be either Combo or Unified.
    VISUALIZE_SURFACE_SPREAD    True if we want to visualize the surface spread angle for the ray cone methods.
    USE_ROUGHNESS_TO_VARIANCE   True if we want to grow the size of the ray cones base on the BRDF roughness.
    USE_FRESNEL_AS_BRDF         True if we want to use Fresnel term as BRDF.
    is_valid_<name>     1 if optional I/O buffer with this name should be used.
*/

#include "Utils/Math/MathConstants.slangh"

import Scene.Raytracing;
import Scene.HitInfo;
import Utils.Math.MathHelpers;
import Utils.Sampling.SampleGenerator;
import Experimental.Scene.Materials.MaterialHelpers;
import Experimental.Scene.Materials.StandardMaterial;
import Experimental.Scene.Lights.LightHelpers;
import Experimental.Scene.Materials.TexLODHelpers;
import Experimental.Scene.Materials.TexLODTypes;
import Experimental.Scene.Materials.Fresnel;
import WhittedRayTracerTypes;

cbuffer CB
{
    uint        gFrameCount;                    // Frame count since scene was loaded.
    uint        gPRNGDimension;                 // First available PRNG dimension.
    float       gScreenSpacePixelSpreadAngle;   // The angle an "average" pixel spans from camera (texLOD).
}

static struct Data
{
    // Materials
    StandardMaterial standardMaterial;
} gData;

// Inputs
Texture2D<float4> gWorldPosition;
Texture2D<float4> gWorldShadingNormal;
Texture2D<float4> gWorldShadingTangent; // Optional
Texture2D<float4> gWorldFaceNormal;
Texture2D<float4> gMaterialDiffuseOpacity;
Texture2D<float4> gMaterialSpecularRoughness;
Texture2D<float4> gMaterialEmissive;
Texture2D<float4> gMaterialExtraParams;
Texture2D<PackedHitInfo> gVBuffer;

// Outputs
RWTexture2D<float4> gOutputColor;

// Static configuration based on defines set from the host.
#define is_valid(name) (is_valid_##name != 0)
static const uint kMaxBounces = MAX_BOUNCES;
static const TexLODMode kTexLODMode = TexLODMode(TEX_LOD_MODE);
static const RayConeMode kRayConeMode = RayConeMode(RAY_CONE_MODE);
static const RayFootprintFilterMode kRayConeFilterMode = RayFootprintFilterMode(RAY_CONE_FILTER_MODE);
static const RayFootprintFilterMode kRayDiffFilterMode = RayFootprintFilterMode(RAY_DIFF_FILTER_MODE);
static const bool kUseAnalyticLights = USE_ANALYTIC_LIGHTS;
static const bool kUseEmissiveLights = USE_EMISSIVE_LIGHTS;
static const bool kUseEnvLight = USE_ENV_LIGHT;
static const bool kUseEnvBackground = USE_ENV_BACKGROUND;
static const float3 kDefaultBackgroundColor = float3(0.25f, 0.25f, 0.25f);
static const float kRayTMax = FLT_MAX;
static const bool kUseRoughnessToVariance = USE_ROUGHNESS_TO_VARIANCE;
static const bool kVisualizeSurfaceSpread = VISUALIZE_SURFACE_SPREAD;
static const bool kUseFresnelAsBRDF = USE_FRESNEL_AS_BRDF;

/** Payload for shadow ray.
*/
struct ShadowRayData
{
    bool visible;
};

float roughnessToSpread(float linearRoughness)
{
    const float alpha = linearRoughness * linearRoughness;
    const float s = alpha * alpha;
    const float sigma2 =  (s / max(0.0001f, (1.0f - s))) * 0.5f;
    return 2.0f * sqrt(sigma2);
}

/** Payload for scatter ray (128B when ray cones are used, 164B when ray diffs are used).
*/
struct ScatterRayData
{
    float3  radiance;                   ///< Accumulated outgoing radiance from path.
    bool    terminated;                 ///< Set to true when path is terminated.
    bool    hasHitRefractiveMaterial;   ///> Whether the path has hit some refractive material on the way.
    float3  thp;                        ///< Current path throughput. This is updated at each path vertex.
    uint    pathLength;                 ///< Path length in number of path segments (0 at origin, 1 at first secondary hit, etc.). Max 2^31.
    float3  origin;                     ///< Next path segment origin.
    float3  direction;                  ///< Next path segment direction.
                                        //   only one (at most) of RayCone and RayDiff will be used (none, if we use Mip0).
    RayCone rayCone;                    ///< Ray cone (2 floats) for texture LOD.
    RayDiff rayDiff;                    ///< Ray differential (12 floats) for texture LOD.

    SampleGenerator sg;     ///< Per-ray state for the sample generator (up to 16B).

    /** Create ray payload with default parameters.
    */
    static ScatterRayData create(SampleGenerator sg)
    {
        ScatterRayData d;
        d.terminated = false;
        d.hasHitRefractiveMaterial = false;
        d.pathLength = 0;
        d.radiance = float3(0.0f, 0.0f, 0.0f);
        d.thp = float3(1.0f, 1.0f, 1.0f);
        d.origin = float3(0.0f, 0.0f, 0.0f);
        d.direction = float3(0.0f, 0.0f, 0.0f);
        d.sg = sg;
        d.rayCone = {};
        d.rayDiff = {};
        return d;
    }
};

/** Helper to load the material attributes.
    \param[in] pixelPos Pixel position.
*/
MaterialParams loadMaterialParams(uint2 pixelPos)
{
    MaterialParams matParams;
    matParams.diffuseOpacity = gMaterialDiffuseOpacity[pixelPos];
    matParams.specularRoughness = gMaterialSpecularRoughness[pixelPos];
    matParams.emissive = gMaterialEmissive[pixelPos];
    matParams.extraParams = gMaterialExtraParams[pixelPos];

    return matParams;
}

/** Traces a shadow ray towards a light source.
    \param[in] origin Ray origin for the shadow ray.
    \param[in] dir Direction from shading point towards the light source (normalized).
    \param[in] distance Distance to the light source.
    \return True if light is visible, false otherwise.
*/
bool traceShadowRay(float3 origin, float3 dir, float distance)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = 0.f;
    ray.TMax = distance;

    ShadowRayData rayData;
    rayData.visible = false;    // Set to true by miss shader if ray is not terminated before.
    TraceRay(gScene.rtAccel, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff /* instanceInclusionMask */, 1 /* hitIdx */, rayTypeCount, 1 /* missIdx */, ray, rayData);

    return rayData.visible;
}

/** Traces a scatter ray based on ray parameters stored in the ray payload.
    \param[in] rayData Describes the ray parameters. The struct is modified based on the result.
*/
void traceScatterRay(inout ScatterRayData rayData)
{
    RayDesc ray;
    ray.Origin = rayData.origin;
    ray.Direction = rayData.direction;
    ray.TMin = 0.f;
    ray.TMax = kRayTMax;

    uint rayFlags = 0;      // TODO: Set cull mode from the app.
    TraceRay(gScene.rtAccel, rayFlags, 0xff /* instanceInclusionMask */, 0 /* hitIdx */, rayTypeCount, 0 /* missIdx */, ray, rayData);
}

/** Evaluates the direct illumination from all analytic lights.
    This function samples Falcor's light list uniformly with one shadow ray.
    \param[in] sd Shading data.
    \param[in] mtl The material at the shading point.
    \param[in] rayOrigin Ray origin for the shadow ray.
    \param[in] sg SampleGenerator object.
    \return Outgoing radiance in view direction.
*/
float3 evalDirectAnalytic<Material : IMaterial>(const ShadingData sd, const Material mtl, const float3 rayOrigin, inout SampleGenerator sg)
{
    // Use all analytic light sources.
    const uint lightCount = gScene.getLightCount();
    float invPdf = 1.0f / lightCount; // Light selection pdf = 1.0f / lightCount.

    float3 contribution = float3(0.0f);
    for (uint lightIndex = 0; lightIndex < lightCount; lightIndex++)
    {
        // Sample local light source.
        AnalyticLightSample ls;
        bool valid = sampleLight(rayOrigin, gScene.getLight(lightIndex), sg, ls);

        // Reject sample if lower hemisphere.
        if (valid && dot(ls.dir, sd.N) > kMinCosTheta)
        {
            // Test visibility by tracing a shadow ray.
            bool V = traceShadowRay(rayOrigin, ls.dir, ls.distance);
            contribution +=  V ? mtl.eval(sd, ls.dir) * (1.0f - sd.specularTransmission) * ls.Li * invPdf : float3(0);
        }
    }
    return contribution;
}

/** Processes a hit point to generate a scatter ray or terminate.
    This function generates a cosine-weighted direction over the hemisphere.
    \param[in] sd Shading data.
    \param[in] rayOrigin Ray origin for the new ray.
    \param[in] rayData Ray payload.
*/
void generateReflectionRay(const ShadingData sd, const float3 rayOrigin, const float3 rayDir, const float3 normal, bool tir, inout ScatterRayData rayData)
{
    float3 wi = reflect(rayDir, normal);
    rayData.origin = rayOrigin;
    rayData.direction = wi;

    if (kUseFresnelAsBRDF)
        rayData.thp *= tir ? 1.0f : evalFresnelSchlick(sd.specular, 1.0f, max(0.00001f, dot(normal, wi)));
    else
        rayData.thp *= tir ? 1.0f : (sd.specular.x + sd.specular.y + sd.specular.z) / 3.0f;    // TODO: better proposals here are welcome...
}

/*********************** Ray index 0: Scatter ray ************************ */
[shader("miss")]
void scatterMiss(inout ScatterRayData rayData : SV_RayPayload)
{
    // Ray missed the scene. Mark the ray as terminated.
    rayData.terminated = true;

    // Add contribution from distant light (env map) in this direction.
    if (kUseEnvLight)
    {
        float lod = 0.0f;    // TexLODMode::Mip0
        if (kTexLODMode == TexLODMode::RayCones)
        {
            lod = computeEnvironmentMapLOD(rayData.rayCone.getSpreadAngle(), gScene.envMap.envMap);
        }
        else if (kTexLODMode == TexLODMode::RayDiffs)
        {
            lod = computeEnvironmentMapLOD(rayData.rayDiff.getdDdx(), rayData.rayDiff.getdDdy(), gScene.envMap.envMap);
        }

        float3 Le = gScene.envMap.eval(WorldRayDirection(), lod);
        rayData.radiance += rayData.thp * Le;
    }
}

[shader("anyhit")]
void scatterAnyHit(inout ScatterRayData rayData : SV_RayPayload, BuiltInTriangleIntersectionAttributes attribs : SV_IntersectionAttributes)
{
    // Alpha test for non-opaque geometry.
    GeometryInstanceID instanceID = getGeometryInstanceID();
    VertexData v = getVertexData(instanceID, PrimitiveIndex(), attribs);
    const uint materialID = gScene.getMaterialID(instanceID);
    if (alphaTest(v, gScene.materials[materialID], gScene.materialResources[materialID], 0.f)) IgnoreHit();
}

[shader("closesthit")]
void scatterClosestHit(inout ScatterRayData rayData : SV_RayPayload, BuiltInTriangleIntersectionAttributes attribs : SV_IntersectionAttributes)
{
    // Evaluate Falcor's material parameters at the hit point.
    // Note we pass hitPos-rayDir as "camera position" to avoid zero-length rays causing NaNs
    // in the view direction. It'd been cleaner if prepareShadingData() took ray dir directly.
    const float3 rayDir = WorldRayDirection();
    const float3 rayOrg = WorldRayOrigin();
    float3 refractedRayDir;
    bool tir = false;

    float hitT = RayTCurrent();
    const GeometryInstanceID instanceID = getGeometryInstanceID();
    const uint materialID = gScene.getMaterialID(instanceID);
    ShadingData sd;
    VertexData v;

    if (kTexLODMode == TexLODMode::Mip0)
    {
        v = getVertexData(instanceID, PrimitiveIndex(), attribs);
        sd = prepareShadingData(v, materialID, gScene.materials[materialID], gScene.materialResources[materialID], -rayDir, 0.0f);
        if (sd.eta != 1.0f)
        {
            if (!refractWithTIR(rayDir, sd.N, sd.eta, refractedRayDir))
            {
                tir = true;
            }
        }
    }
    else if (kTexLODMode == TexLODMode::RayCones)
    {
        v = getVertexDataRayCones(instanceID, PrimitiveIndex(), attribs);
        float curvature = gScene.computeCurvatureIsotropic(instanceID, PrimitiveIndex());

        // Compute texture LOD for prepareShadingData().
        float surfaceSpreadAngle = 0.0f;
        if (kRayConeMode == RayConeMode::Combo || kRayConeMode == RayConeMode::Unified)
        {
            float rayConeWidth = rayData.rayCone.getSpreadAngle() * hitT + rayData.rayCone.getWidth();
            surfaceSpreadAngle = computeSpreadAngleFromCurvatureIso(curvature, rayConeWidth, rayDir, v.normalW);
        }

        rayData.rayCone = rayData.rayCone.propagateDistance(hitT);                     // Propagate (only) to hit point (not setting any surfaceSpreadAngle -- this is cone in the next propagate call, a few lines down).

        if (kRayConeFilterMode == RayFootprintFilterMode::Isotropic || (kRayConeFilterMode == RayFootprintFilterMode::AnisotropicWhenRefraction && !rayData.hasHitRefractiveMaterial))
        {
            float lambda = rayData.rayCone.computeLOD(v.coneTexLODValue, rayDir, v.normalW);
            sd = prepareShadingDataUsingRayConesLOD(v, materialID, gScene.materials[materialID], gScene.materialResources[materialID], -rayDir, lambda);
        }
        else
        {
            float3 positions[3];
            float2 txcoords[3];
            gScene.getVertexPositionsW(instanceID, PrimitiveIndex(), positions);
            gScene.getVertexTexCoords(instanceID, PrimitiveIndex(), txcoords);

            float coneRadiusAtHitPoint = rayData.rayCone.getWidth();
            float2 ddx = float2(0), ddy = float2(0);
            computeAnisotropicEllipseAxes(v.posW, v.faceNormalW, rayDir, coneRadiusAtHitPoint, positions, txcoords, v.texC, ddx, ddy);
            sd = prepareShadingData(v, materialID, gScene.materials[materialID], gScene.materialResources[materialID], -rayDir, ddx, ddy);
        }

        // Bounce on hit surface.
#if USE_ROUGHNESS_TO_VARIANCE
        surfaceSpreadAngle += roughnessToSpread(sd.linearRoughness);
#endif
        if (sd.eta == 1.0f)   // reflection
        {
            rayData.rayCone = rayData.rayCone.addToSpreadAngle(2.0f * surfaceSpreadAngle);       // Reflection needs a factor of 2.0 (see RTG1-article on texture LOD).
        }
        else                // refraction
        {

            float3 hitPoint = rayOrg + rayDir * hitT;
            if (!refractRayCone(rayData.rayCone, rayOrg, rayDir, hitPoint, sd.N, surfaceSpreadAngle, sd.eta, refractedRayDir))
            {
                rayData.rayCone = rayData.rayCone.addToSpreadAngle(surfaceSpreadAngle);
                tir = true;
            }
        }
    }
    else if (kTexLODMode == TexLODMode::RayDiffs)
    {
        float2 dUVdx, dUVdy;    // Ray differential variables for the texture lookup.
        float2 dBarydx, dBarydy;
        float3 geometricNormal = gScene.getFaceNormalW(instanceID, PrimitiveIndex());
        RayDiff newRayDiff = rayData.rayDiff.propagate(rayOrg, rayDir, hitT, geometricNormal);              // Propagate the ray differential to the current hit point.

        float3 barycentrics;
        StaticVertexData triangleVertices[3];
        v = getVertexData(instanceID, PrimitiveIndex(), attribs, barycentrics, triangleVertices);

        float3 unnormalizedN;   // Non-normalized interpolated normal for ray differential scatter.
        float3 normals[3];      // Non-normalized normals for ray differential scatter.

        prepareRayDiffAtHitPoint(v, triangleVertices, barycentrics, rayDir, gScene.getWorldMatrix(instanceID), gScene.getInverseTransposeWorldMatrix(instanceID),
            newRayDiff, unnormalizedN, normals, dBarydx, dBarydy, dUVdx, dUVdy);

        if (kRayDiffFilterMode == RayFootprintFilterMode::Isotropic || (kRayDiffFilterMode == RayFootprintFilterMode::AnisotropicWhenRefraction && !rayData.hasHitRefractiveMaterial))
        {
            // When using prepareShadingDataUsingRayDiffsIsotropic(), the texture sampler will compute a single lambda for texture LOD
            // using the dUVdx and dUVdy.
            sd = prepareShadingDataUsingRayDiffsIsotropic(v, materialID, gScene.materials[materialID], gScene.materialResources[materialID], -rayDir, dUVdx, dUVdy);
        }
        else        // kRayDiffFilterMode == RayFootprintFilterMode::Anisotropic
        {
            // Use the dUVdx and dUVdy directly --> gives you anisotropic filtering.
            sd = prepareShadingData(v, materialID, gScene.materials[materialID], gScene.materialResources[materialID], -rayDir, dUVdx, dUVdy);
        }

        if (sd.eta == 1.0f)
        {
            reflectRayDifferential(newRayDiff, rayDir, unnormalizedN, v.normalW, dBarydx, dBarydy, normals);
        }
        else
        {
            if (!refractRayDifferential(newRayDiff, rayDir, unnormalizedN, normalize(unnormalizedN), dBarydx, dBarydy, normals, sd.eta, refractedRayDir))
            {
                reflectRayDifferential(newRayDiff, rayDir, unnormalizedN, v.normalW, dBarydx, dBarydy, normals);
                tir = true;
            }
        }
        rayData.rayDiff = newRayDiff;
    }

    // Add emitted light.
    if (kUseEmissiveLights)
    {
        rayData.radiance += rayData.thp * sd.emissive;
    }

    // Check whether to terminate based on max depth.
    if (rayData.pathLength >= kMaxBounces)
    {
        rayData.terminated = true;
        return;
    }

    // Compute ray origin for new rays spawned from the hit.
    float3 rayOriginAtHitPoint = sd.computeNewRayOrigin();

    // Add contribution of direct light from analytic lights.
    if (kUseAnalyticLights)
    {
        float3 Lr = evalDirectAnalytic(sd, gData.standardMaterial, rayOriginAtHitPoint, rayData.sg);
        rayData.radiance += rayData.thp * Lr;
    }

    // Generate scatter ray for the next path segment.
    // The raygen shader will continue the path based on the returned payload.
    if (sd.eta == 1.0f || tir)
    {
        generateReflectionRay(sd, rayOriginAtHitPoint, rayDir, v.normalW, tir, rayData);
    }
    else
    {
        rayData.direction = refractedRayDir;
        rayData.origin = sd.computeNewRayOrigin(false);

        if (kUseFresnelAsBRDF)
        {
            rayData.thp *= 1.0f - evalFresnelSchlick(sd.specular, 1.0f, max(0.00001f, dot(sd.N, -rayDir)));
            // There's convinient 'evalFresnelDielectric' we could use here, but it's somewhat darker (worse for development)
            //rayData.thp *= 1.0f - evalFresnelDielectric(sd.eta, dot(sd.N, -rayDir));
        }
        else
        {
            rayData.thp *= (1.0f - sd.linearRoughness);
        }
        rayData.hasHitRefractiveMaterial = true;
    }
    rayData.pathLength++;

    if (max(max(rayData.thp.x, rayData.thp.y), rayData.thp.z) < 0.01f)  // TODO: this is a hack to terminate rays
    {
        rayData.terminated = true;
        return;
    }
}

/************************** Ray index 1: Shadow ray ************************ */

[shader("miss")]
void shadowMiss(inout ShadowRayData rayData : SV_RayPayload)
{
    // The miss shader is executed if the ray misses all geometry. Mark as visible.
    rayData.visible = true;
}

[shader("anyhit")]
void shadowAnyHit(inout ShadowRayData rayData : SV_RayPayload, BuiltInTriangleIntersectionAttributes attribs : SV_IntersectionAttributes)
{
    // Alpha test for non-opaque geometry.
    GeometryInstanceID instanceID = getGeometryInstanceID();
    VertexData v = getVertexData(instanceID, PrimitiveIndex(), attribs);
    const uint materialID = gScene.getMaterialID(instanceID);
    if (alphaTest(v, gScene.materials[materialID], gScene.materialResources[materialID], 0.f)) IgnoreHit();
}

/** ******************************** RayGen ******************************** */

/** This is the entry point for the minimal path tracer.

    One path per pixel is generated, which  is traced into the scene.
    The path tracer is written as a for-loop over path segments.

    Built-in light sources (point, directional, ..) are sampled explicitly at each
    path vertex. The contributions from area lights (env map and mesh lights)
    are explicitly added by the scatter ray hit/miss shaders.
*/
[shader("raygeneration")]
void rayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    float3 outColor = float3(0.0f);

    const float3 nonNormalizedRayDir = gScene.camera.computeNonNormalizedRayDirPinhole(launchIndex, launchDim);    // Used by ray diffs.
    const float3 rayDir = normalize(nonNormalizedRayDir);
    const float4 worldPos = gWorldPosition[launchIndex];

    if (worldPos.w != 0.f)  // Using w to indicate valid geometry for now.
    {
        // Pixel represents a valid primary hit. Compute its contribution.

        // Load geometry parameters from G-buffer.
        float3 normal = gWorldShadingNormal[launchIndex].xyz;
        float4 tangent = is_valid(gWorldShadingTangent) ? gWorldShadingTangent[launchIndex] : float4(perp_stark(normal), 1.f);
        float3 faceNormal = gWorldFaceNormal[launchIndex].xyz;
        GeometryParams geoParams = prepareGeometryParams(worldPos.xyz, -rayDir, normal, tangent, faceNormal);

        // Load material parameters from G-buffer.
        MaterialParams matParams = loadMaterialParams(launchIndex);

        // Prepare ShadingData struct.
        ShadingData sd = prepareShadingData(geoParams, matParams);

        // Create sample generator.
        SampleGenerator sg = SampleGenerator.create(launchIndex, gFrameCount);

        // Advance the generator to the first available dimension.
        // TODO: This is potentially expensive. We may want to store/restore the state from memory if it becomes a problem.
        for (uint i = 0; i < gPRNGDimension; i++) sampleNext1D(sg);

        // Compute ray origin for new rays spawned from the G-buffer.
        const float3 rayOriginAtHitPoint = sd.computeNewRayOrigin();

        // Always output directly emitted light, independent of whether emissive materials are treated as light sources or not.
        outColor += sd.emissive;

        // Add contribution of direct light from analytic lights.
        // Light probe and mesh lights are handled by the scatter ray hit/miss shaders.
        outColor += kUseAnalyticLights ? evalDirectAnalytic(sd, gData.standardMaterial, rayOriginAtHitPoint, sg) : float3(0, 0, 0);

        // Prepare ray payload.
        ScatterRayData rayData = ScatterRayData.create(sg);

        float3 refractedRayDir = float3(0.0f);
        bool tir = false;

        if (kTexLODMode == TexLODMode::RayCones)
        {
            // Set up ray cone (texLOD) at the hit point: first create ray cone at camera, the propagate to hit point.
            float surfaceSpreadAngle = 0.0f;
            const float3 rayOrg = gScene.camera.getPosition();
            const float hitT = length(rayOrg - worldPos.xyz);

            const HitInfo hit = HitInfo(gVBuffer[launchIndex]);
            if (hit.isValid() && hit.getType() == HitType::Triangle)
            {
                const TriangleHit triangleHit = hit.getTriangleHit();
                const float4x4 worldMat = gScene.getWorldMatrix(triangleHit.instanceID);
                const float3x3 worldInvTransposeMat = gScene.getInverseTransposeWorldMatrix(triangleHit.instanceID);
                const uint3 vertexIndices = gScene.getIndices(triangleHit.instanceID, triangleHit.primitiveIndex);
                const float3 barycentrics = triangleHit.getBarycentricWeights();
                float2 txcoords[3], dBarydx, dBarydy, dUVdx, dUVdy;

                StaticVertexData vertices[3] = { gScene.getVertex(vertexIndices[0]), gScene.getVertex(vertexIndices[1]), gScene.getVertex(vertexIndices[2]) };

                float curvature = gScene.computeCurvatureIsotropicFirstHit(triangleHit.instanceID, triangleHit.primitiveIndex, rayDir);

                if (kRayConeMode == RayConeMode::Combo)
                {
                    RayDiff rayDiff;
                    float3 dDdx, dDdy, unnormalizedN, normals[3], dNdx, dNdy, edge1, edge2;
                    prepareVerticesForRayDiffs(rayDir, vertices, worldMat, worldInvTransposeMat, barycentrics, edge1, edge2, normals, unnormalizedN, txcoords);
                    computeRayDirectionDifferentials(nonNormalizedRayDir, gScene.camera.data.cameraU, gScene.camera.data.cameraV, launchDim, dDdx, dDdy);
                    RayDiff rd = RayDiff.create(float3(0.0f), float3(0.0f), dDdx, dDdy);    // Init ray diff. dOdx = 0 , dOdy = 0, and the directions are from above.
                    rayDiff = rd.propagate(worldPos.xyz, rayDir, hitT, sd.N);               // Propagate the ray differential to the current hit point.
                    computeBarycentricDifferentials(rayDiff, rayDir, edge1, edge2, sd.faceN, dBarydx, dBarydy);
                    computeNormalDifferentials(rayDiff, unnormalizedN, dBarydx, dBarydy, normals, dNdx, dNdy);
                    float3 right = normalize(gScene.camera.computeNonNormalizedRayDirPinhole(launchIndex + uint2(1, 0), launchDim)) - rayDir;
                    float3 up = normalize(gScene.camera.computeNonNormalizedRayDirPinhole(launchIndex + uint2(0, 1), launchDim)) - rayDir;
                    surfaceSpreadAngle = computeScreenSpaceSurfaceSpreadAngle(right, up, dNdx, dNdy);
                }
                else // kRayConeMode == RayConeMode::Unified
                {
                    float curvature = gScene.computeCurvatureIsotropicFirstHit(triangleHit.instanceID, triangleHit.primitiveIndex, rayDir);
                    float rayConeWidth = hitT * gScreenSpacePixelSpreadAngle;
                    surfaceSpreadAngle = computeSpreadAngleFromCurvatureIso(curvature, hitT * gScreenSpacePixelSpreadAngle, rayDir, sd.N);
                }

#if USE_ROUGHNESS_TO_VARIANCE
                surfaceSpreadAngle += roughnessToSpread(sd.linearRoughness);
#endif
                if (kVisualizeSurfaceSpread)
                {
                    const float eps = 1.0e-4;
                    float r = surfaceSpreadAngle > eps ? surfaceSpreadAngle : 0.0f;
                    float g = surfaceSpreadAngle <= -eps ? -surfaceSpreadAngle : 0.0f;
                    float b = abs(surfaceSpreadAngle) < eps ? 1.0f : 0.0f;
                    gOutputColor[launchIndex] = float4(r, g, b, 1.0f) * 10.0f;
                    return;
                }

                RayCone rc = RayCone.create(0.0f, gScreenSpacePixelSpreadAngle);
                rayData.rayCone = rc.propagateDistance(hitT);
                if (sd.eta == 1.0f)   // reflection
                {
                    rayData.rayCone = rayData.rayCone.addToSpreadAngle(2.0f * surfaceSpreadAngle);   // Reflection needs a factor of 2.0 (see RTG1 article on texture LOD).
                }
                else                // refraction
                {
                    float3 hitPoint = rayOrg + rayDir * hitT;
                    if (!refractRayCone(rayData.rayCone, rayOrg, rayDir, hitPoint, sd.N, surfaceSpreadAngle, sd.eta, refractedRayDir))
                    {
                        rayData.rayCone = rayData.rayCone.addToSpreadAngle(surfaceSpreadAngle);
                        tir = true;
                    }
                }
            }
        }
        else if (kTexLODMode == TexLODMode::RayDiffs)
        {
            RayDiff rd;
            float3 dDdx, dDdy;
            const float3 rayOrg = gScene.camera.getPosition();
            const float hitT = length(rayOrg - worldPos.xyz);

            computeRayDirectionDifferentials(nonNormalizedRayDir, gScene.camera.data.cameraU, gScene.camera.data.cameraV, launchDim, dDdx, dDdy);

            rayData.rayDiff = RayDiff.create(float3(0.0f), float3(0.0f), dDdx, dDdy); // Init ray diff. dOdx = 0 , dOdy = 0, and the directions are from above.

            rayData.rayDiff = rayData.rayDiff.propagate(worldPos.xyz, rayDir, hitT, faceNormal); // Propagate the ray differential to the current hit point.

            const HitInfo hit = HitInfo(gVBuffer[launchIndex]);
            if (hit.isValid() && hit.getType() == HitType::Triangle)
            {
                const TriangleHit triangleHit = hit.getTriangleHit();
                const float4x4 worldMat = gScene.getWorldMatrix(triangleHit.instanceID);
                const float3x3 worldInvTransposeMat = gScene.getInverseTransposeWorldMatrix(triangleHit.instanceID);
                const uint3 vertexIndices = gScene.getIndices(triangleHit.instanceID, triangleHit.primitiveIndex);
                const float3 barycentrics = triangleHit.getBarycentricWeights();
                float3 unnormalizedN, normals[3], dNdx, dNdy, edge1, edge2;
                float2 txcoords[3], dBarydx, dBarydy, dUVdx, dUVdy;

                StaticVertexData vertices[3] = { gScene.getVertex(vertexIndices[0]), gScene.getVertex(vertexIndices[1]), gScene.getVertex(vertexIndices[2]) };
                prepareVerticesForRayDiffs(rayDir, vertices, worldMat, worldInvTransposeMat, barycentrics, edge1, edge2, normals, unnormalizedN, txcoords);

                computeBarycentricDifferentials(rayData.rayDiff, rayDir, edge1, edge2, sd.faceN, dBarydx, dBarydy);

                if (sd.eta == 1.0f)
                {
                    reflectRayDifferential(rayData.rayDiff, rayDir, unnormalizedN, normalize(unnormalizedN), dBarydx, dBarydy, normals);
                }
                else
                {
                    if (!refractRayDifferential(rayData.rayDiff, rayDir, unnormalizedN, normalize(unnormalizedN), dBarydx, dBarydy, normals, sd.eta, refractedRayDir))
                    {
                        reflectRayDifferential(rayData.rayDiff, rayDir, unnormalizedN, normalize(unnormalizedN), dBarydx, dBarydy, normals);
                        tir = true;
                    }
                }
            }
        }
        else // Mip0.
        {
            if (!refractWithTIR(rayDir, sd.N, sd.eta, refractedRayDir))
            {
                tir = true;
            }
        }

        // Generate scatter ray.
        if (sd.eta == 1.0f || tir)
        {
            generateReflectionRay(sd, rayOriginAtHitPoint, rayDir, normal, tir, rayData);
        }
        else
        {
            rayData.direction = refractedRayDir;
            rayData.origin = sd.computeNewRayOrigin(false);

            if (kUseFresnelAsBRDF)
            {
                rayData.thp *= 1.0f - evalFresnelSchlick(sd.specular, 1.0f, max(0.00001f, dot(sd.N, -rayDir)));

                // There's convenient 'evalFresnelDielectric' we could use here, but it's somewhat darker (worse for development)
                //rayData.thp *= 1.0f - evalFresnelDielectric(sd.eta, dot(sd.N, -rayDir));
            }
            else
            {
                rayData.thp *= (1.0f - sd.linearRoughness);
            }
        }

        if(sd.linearRoughness <= 0.19f)      // Only trace reflection rays when material is highly reflective.
        {
            // Follow path into the scene and compute its total contribution.
            for (uint depth = 0; depth <= kMaxBounces && !rayData.terminated; depth++)
            {
                // Trace scatter ray. If it hits geometry, the closest hit shader samples
                // direct illumination and generates the next scatter ray.
                traceScatterRay(rayData);
            }
        }
        // Store contribution from scatter ray.
        outColor += rayData.radiance;
    }
    else
    {
        // Background pixel.
        if (kUseEnvBackground)
        {
            float lod = 0.0f;    // TexLODMode::Mip0.
            if (kTexLODMode == TexLODMode::RayCones)
            {
                lod = computeEnvironmentMapLOD(gScreenSpacePixelSpreadAngle, gScene.envMap.envMap);
            }
            else if (kTexLODMode == TexLODMode::RayDiffs)
            {
                float3 dDdx, dDdy;
                computeRayDirectionDifferentials(nonNormalizedRayDir, gScene.camera.data.cameraU, gScene.camera.data.cameraV, launchDim, dDdx, dDdy);
                lod = computeEnvironmentMapLOD(dDdx, dDdy, gScene.envMap.envMap);
            }
            outColor = gScene.envMap.eval(rayDir, lod);
        }
        else
        {
            outColor = kDefaultBackgroundColor;
        }
    }

    gOutputColor[launchIndex] = float4(outColor, 1.0f);
}