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

/** This file contains helper functions for analytic light source sampling.
    The code supports Falcor's analytic point, directional, and area lights,
    which are all defined in the scene description.

    Mesh lights (emissive geometry) and light probes are handled separately.

    This is work in progress. The code is not very well-tested.
*/

#include "Utils/Math/MathConstants.slangh"
#include "LightSolidAngleHelpers.slangh"

import Scene.Lights.LightData;
import Utils.Math.MathHelpers;
import Utils.Sampling.SampleGenerator;
import Utils.Sampling.SampleGeneratorInterface;

__exported import Utils.Sampling.SamplingSphericalCaps;
__exported import PSCMaps;

static const float kMinLightDistSqr = 1e-9f;
static const float kMaxLightDistance = FLT_MAX;

/** Describes a light sample for Falcor's analytic light sources.

    The struct contains a superset of what is normally needed for evaluating
    lighting integrals. Be careful not to access fields that are not needed,
    to make sure dead code elimination removes the computations.
*/
struct AnalyticLightSample {
    float3  posW;           ///< Sampled point on the light source in world space (for local lights only).
    float3  normalW;        ///< Normal of the sampled point on the light source in world space (normalized).
    float3  dir;            ///< Direction from the shading point to the sampled point on the light in world space (normalized).
    float3  shadowDir;      ///< Shadow ray direction. Used for artificial soft shadows.
    float   distance;       ///< Distance from the shading point to sampled point on the light.
    float3  Li;             ///< Classic incident radiance at the shading point (unshadowed). Note: Already divided by the pdf. For old workflow without separate contributions
    float   i;              ///< Incident irradiance multyplier.
    
    float   pdf;            ///< Probability density function with respect to solid angle at the shading point.
    float3  dbg;
};

/** Internal helper function to validate the shared computations for area light samples.
    The input sample must already have posW and normalW computed.
    \return True if the sample is valid, false otherwise.
*/
bool validateAreaLightSample(const float3 shadingPosW, const LightData light, inout AnalyticLightSample ls) {

    ls.i = 1.0f;
    ls.pdf = 0.0f;

    // Compute direction and distance to light.
    // The distance is clamped to a small epsilon to avoid div-by-zero below.
    float3 toLight = ls.posW - shadingPosW;
    float distSqr = max(dot(toLight, toLight), kMinLightDistSqr);
    ls.distance = sqrt(distSqr);
    ls.dir = ls.shadowDir = toLight / ls.distance;

    // Compute incident radiance at shading point.
    // The area lights are single-sided by default, so radiance is zero when seen from the back-facing side.
    float cosTheta = dot(ls.normalW, -ls.dir);

    if (cosTheta <= 0.f) {
        if (light.isSingleSided()) return false;
        ls.normalW *= -1.0f;
    }

    ls.Li = light.directDiffuseIntensity;
    
    // Compute the PDF with respect to solid angle. Note this may be +inf.
    ls.pdf = distSqr / (abs(cosTheta) * light.surfaceArea);
    return true;
}

/** Samples a rectangular area light source.
    \param[in] shadingPosW Shading point in world space.
    \param[in] light Light data.
    \param[in] float 2 random.
    \param[out] ls Light sample struct.
    \return True if a sample was generated, false otherwise.
*/
bool sampleRectAreaLight(const float3 shadingPosW, const LightData light, const float4 rnd, out AnalyticLightSample ls) {
    // Pick a random sample on the quad.
    // The quad is from (-1.0, -1.0, 0.0) to (1.0, 1.0, 0.0) in object space, but may be scaled by its transform matrix.
    float3 pos = float3(2.0f * (rnd.x - .5f), 2.0f * (rnd.y - .5f), 0.f);

    // Apply model to world transformation matrix.
    ls.posW = mul(float4(pos, 1.f), light.transMat).xyz;

    // Setup world space normal.
    // TODO: normalW is not correctly oriented for mesh instances that have flipped triangle winding.
    ls.normalW = normalize(mul(float4(0.f, 0.f, -1.f, 0.f), light.transMatIT).xyz);

    if (!validateAreaLightSample(shadingPosW, light, ls)) return false;

    return true;
}

bool sampleRectAreaLightRIS(const float3 shadingPosW, const float3 shadingNormalW, const LightData light, const float4 rnd, out AnalyticLightSample ls) {
    // Rejection sample version

    // First check that all corners above surface plane
    // p[0](x)---p[1](y)
    //   |         |
    //   |         | <- light rect
    //   |         |
    // p[3](w)---p[2](z)

    ls.i = 1.0f;
    ls.Li = light.directDiffuseIntensity;

    const float3 localShadingPos = mul(float4(shadingPosW, 1.f), light.transMatInv).xyz;
    const float3 localNormal = normalize(mul(light.transMat, float4(shadingNormalW, 0.f)).xyz);

    // Unit size quad
    const float3 p[4] = {float3(-1.0f, 1.0f, 0.0f), float3(1.0f, 1.0f, 0.0f), float3(1.0f,-1.0f, 0.0f), float3(-1.0f,-1.0f, 0.0f)};

    // Double the quad as out weird houdini threats it. TODO: make all unit primitives proper sized!!!
    uint4 cosT = float4(
        dot(localNormal, normalize(p[0] - localShadingPos)) > 0.0f ? 1 : 0,
        dot(localNormal, normalize(p[1] - localShadingPos)) > 0.0f ? 1 : 0,
        dot(localNormal, normalize(p[2] - localShadingPos)) > 0.0f ? 1 : 0,
        dot(localNormal, normalize(p[3] - localShadingPos)) > 0.0f ? 1 : 0,
    );

    [branch]
    if( all(cosT == 1) ) {
        // All corners above shading surface. Return sample.
        return sampleRectAreaLight(shadingPosW, light, rnd, ls);
    } else if( all(cosT == 0) ) {
        // All corners below shading surface.
        return false;
    }

    // Some corners above, some below shading surface plane.
    // Case 1: One corner above
    // Case 2: Two corners above
    // Case 3: Three corners above

    uint _case = cosT[0] + cosT[1] + cosT[2] + cosT[3];
    uint  indices[3]; // point indices;

    // Find first 'above' corner
    const uint firstIdx = !cosT[0] ? !cosT[1] ? !cosT[2] ? !cosT[3] ? 0 : 3 : 2 : 1 : cosT[3] ? cosT[2] ? cosT[1] ? cosT[0] ? 0 : 1 : 2 : 3 : 0; 
    
    const float q = dot(localNormal, -localShadingPos);
    const float4 localSurfacePlane = float4(localNormal, q);

    float3 pp[8] = {p[3], p[0], p[1], p[2], p[3], p[0], p[1], p[2]}; // same 4 corners but wrapped around for easier indexing

    float local_visible_area = 1.0f;
    float3 pos = .0f;
    float3 b = sample_triangle(rnd.xy);

    switch (_case) {
        case 1: {
            // One corner above
            uint idx = firstIdx + 1; 
            float3 v1 = normalize(pp[idx-1] - pp[idx]);
            float3 v2 = normalize(pp[idx+1] - pp[idx]);
            float isect1 = point_to_plane_distance(pp[idx], v1, localSurfacePlane)*.5f;
            float isect2 = point_to_plane_distance(pp[idx], v2, localSurfacePlane)*.5f;
            local_visible_area = (isect1 * isect2) / 2.0f;
            pos = pp[idx] * b.x + (pp[idx] + v1*isect1*2.0) * b.y + (pp[idx] + v2*isect2*2.0) * b.z;
        } break;
        case 2:  {
            // Two corners above   
            uint idx1 = firstIdx + 1; 
            uint idx2 = firstIdx + 2;
            float3 v1 = normalize(pp[idx1-1] - pp[idx1]);
            float3 v2 = normalize(pp[idx2+1] - pp[idx2]); 
            float isect1 = point_to_plane_distance_unsigned(pp[idx1], v1, localSurfacePlane)*.5f;
            float isect2 = point_to_plane_distance_unsigned(pp[idx2], v2, localSurfacePlane)*.5f;
            local_visible_area = (isect1 + isect2) / 2.0f;

            float k = isect1 / (isect1 + isect2);
            
            [branch]
            if(rnd.z < k) {
                pos = pp[idx1] * b.x + pp[idx2] * b.y + (pp[idx1] + v1*isect1*2.0) * b.z;
            } else {
                pos = pp[idx2] * b.x + (pp[idx1] + v1*isect1*2.0) * b.y + (pp[idx2] + v2*isect2*2.0) * b.z;
            }
        } break;
        default: {
            // Three corners above
            const uint idx1 = firstIdx + 1, idx2 = firstIdx + 2, idx3 = firstIdx + 3; 
            float3 v1 = normalize(pp[idx1-1] - pp[idx1]);
            float3 v2 = normalize(pp[idx3+1] - pp[idx3]); 
            float isect1 = point_to_plane_distance(pp[idx1], v1, localSurfacePlane)*.5f;
            float isect2 = point_to_plane_distance(pp[idx3], v2, localSurfacePlane)*.5f;
            
            local_visible_area = 1.0f - ((1.0f - isect1) * (1.0f - isect2) * .5f);

            float s1 = isect1 * .5f, s3 = isect2 *.5f;
            float s2 = local_visible_area - s1 - s3;
            
            float k1 = s1 / local_visible_area, k2 = (s1 + s2) / local_visible_area; 

            [branch]
            if(rnd.z < k1) {
                pos = pp[idx2] * b.x + pp[idx1] * b.y + (pp[idx1] + v1*isect1*2.0) * b.z;
            } else if(rnd.z >= k1 && rnd.z < k2) {
                pos = pp[idx2] * b.x + (pp[idx1] + v1*isect1*2.0) * b.y + (pp[idx3] + v2*isect2*2.0) * b.z;
            } else {
                pos = pp[idx2] * b.x + pp[idx3] * b.y + (pp[idx3] + v2*isect2*2.0) * b.z;
            }
        } break;
    }

    
    // Apply model to world transformation matrix.
    ls.posW = mul(float4(pos, 1.f), light.transMat).xyz;

    // Setup world space normal.
    // TODO: normalW is not correctly oriented for mesh instances that have flipped triangle winding.
    ls.normalW = normalize(mul(float4(0.f, 0.f, -1.f, 0.f), light.transMatIT).xyz);
    
    float3 toLight = ls.posW - shadingPosW;
    float distSqr = max(dot(toLight, toLight), kMinLightDistSqr);
    ls.distance = sqrt(distSqr);
    ls.dir = ls.shadowDir = toLight / ls.distance;

    float cosTheta = dot(ls.normalW, -ls.dir);

    if (cosTheta <= 0.f) {
        if (light.isSingleSided()) return false;
        ls.normalW *= -1.0f;
    }

    ls.pdf = distSqr / (abs(cosTheta) * light.surfaceArea * local_visible_area);
    ls.i = 1.0f;

    return true;
}

bool intersectRectAreaLight(const float3 shadingPosW, const float3 dir, const LightData light, out AnalyticLightSample ls) {
    
    float3 localPos = mul(float4(shadingPosW, 1.f), light.transMatInv).xyz;
    float3 localDir = normalize(mul(float4(dir, 0.f), light.transMatInv).xyz);

    float isect = intersect_plane(localPos, localDir);
    
    localPos += isect * localDir;
    if (localPos.x > 1.f || localPos.x < -1.f || localPos.y > 1.f || localPos.y < -1.f) return false;
    ls.posW = mul(float4(localPos, 1.f), light.transMat).xyz;

    ls.normalW = normalize(mul(float4(0.f, 0.f, -1.f, 0.f), light.transMatIT).xyz);

    if (!validateAreaLightSample(shadingPosW, light, ls)) return false;

    return true;
}

/** Samples a spherical area light source.
    \param[in] shadingPosW Shading point in world space.
    \param[in] light Light data.
    \param[in] float 2 random.
    \param[out] ls Light sample struct.
    \return True if a sample was generated, false otherwise.
*/
bool sampleSphereAreaLight(const float3 shadingPosW, const LightData light, const float4 rnd, out AnalyticLightSample ls) {
    float3 localPos = mul(float4(shadingPosW, 1.f), light.transMatInv).xyz;
    
    float3 w = -localPos;
    float localDistSqr = max(dot(w, w), kMinLightDistSqr);     //squared local distance to light center
    float dc = sqrt(localDistSqr);      //distance to light center

    float radius = 1.0f;  // because transform matrix does twice scale
    float sph_r2 = 1.0f;

    float sinThetaMax2 = sph_r2 / localDistSqr;
    float cosThetaMax = sqrt(max(0.0f, 1.0f - sinThetaMax2));
    float cosTheta = (1 - rnd.x) + rnd.x * cosThetaMax;
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = rnd.y * M_2PI;

    float ds = dc * cosTheta - sqrt(max(0.0f, sph_r2 - dc * dc * sinTheta * sinTheta));
    float cosAlpha = (dc * dc + sph_r2 - ds * ds) / (2 * dc * radius);
    float sinAlpha = sqrt(max(0.0f, 1.0f - cosAlpha * cosAlpha));

    float3 wc = normalize(w);
    float3 wcX, wcY;

    if (abs(wc.x) > abs(wc.y)) {
        wcX = float3(-wc.z, 0, wc.x) / sqrt(wc.x * wc.x + wc.z * wc.z);
    } else {
        wcX = float3(0, wc.z, -wc.y) / sqrt(wc.y * wc.y + wc.z * wc.z);
    }
    
    wcY = cross(wc, wcX);

    float3 pos = sphericalDirection(sinAlpha, cosAlpha, phi, -wcX, -wcY, -wc);

    // Apply model to world transformation matrix.
    ls.posW = mul(float4(pos, 1.f), light.transMat).xyz;

    // Setup world space normal.
    ls.normalW = normalize(mul(float4(pos, 0.f), light.transMatIT).xyz);

    if (!validateAreaLightSample(shadingPosW, light, ls)) return false;

    // Compute spherical light pdf
    ls.pdf = uniformConePdf(cosThetaMax);

    return true;
}

bool sampleSphereAreaLightSAU(const float3 shadingPosW, const float3 shadingNormalW, const LightData light, const float4 rnd, out AnalyticLightSample ls) {

    const float3 localShadingPos = mul(float4(shadingPosW, 1.f), light.transMatInv).xyz;

    //float3 localNormal = normalize(mul(float4(shadingNormalW, 0.f), light.transMatInv).xyz);
    const float3 localNormal = normalize(mul(light.transMat, float4(shadingNormalW, 0.f)).xyz);
    
    ProjectedSphericalCap cap;
    prepareProjectedSphericalCapSampling(cap, localNormal, -localShadingPos, 1.0f);

    const float3 lightSampleDirLocal = normalize(sampleProjectedSphericalCap(ls.pdf, cap, rnd.xy));

    float isect = intersect_sphere(localShadingPos, lightSampleDirLocal).x;
    if(isect <= .0f || ls.pdf <= .0f) return false;

    float3 pos = localShadingPos + isect * lightSampleDirLocal;

    // Apply model to world transformation matrix.
    ls.posW = mul(float4(pos, 1.f), light.transMat).xyz;

    // Setup world space normal.
    ls.normalW = normalize(mul(float4(pos, 0.f), light.transMatIT).xyz);

    float3 toLight = ls.posW - shadingPosW;
    float distSqr = max(dot(toLight, toLight), kMinLightDistSqr);
    ls.distance = sqrt(distSqr);

    ls.dir = ls.shadowDir = normalize(mul(float4(lightSampleDirLocal, 0.f), light.transMat).xyz);

    float cosTheta = dot(ls.normalW, -ls.dir);

    if (cosTheta <= 0.f) {
        if (light.isSingleSided()) return false;
        ls.normalW *= -1.0f;
    }

    ls.pdf *= cosTheta;

    ls.i = 1.0f;
    ls.Li = light.directDiffuseIntensity;

    return true;
}

bool intersectSphereAreaLight(const float3 shadingPosW, const float3 dir, const LightData light, out AnalyticLightSample ls) {
    
    float3 localPos = mul(float4(shadingPosW, 1.f), light.transMatInv).xyz;
    float3 localDir = normalize(mul(float4(dir, 0.f), light.transMatInv).xyz);

    float isect = intersect_sphere(localPos, localDir).x;
    if(isect == -1.0f) return false;

    float3 pos = localPos + isect * localDir;
    ls.posW = mul(float4(pos, 1.f), light.transMat).xyz;

    ls.normalW = normalize(mul(float4(pos, 0.f), light.transMatIT).xyz);

    if (!validateAreaLightSample(shadingPosW, light, ls)) return false;

    // Compute spherical light pdf
    float sph_r2 = 1.0f;
    float loacalDistSqr = max(dot(-localPos, -localPos), kMinLightDistSqr);
    float sinThetaMax2 = sph_r2 / loacalDistSqr;
    float cosThetaMax = sqrt(max(0.0f, 1.0f - sinThetaMax2));
    ls.pdf = uniformConePdf(cosThetaMax);

    return true;
}

/** Samples disc area light source.
    \param[in] shadingPosW Shading point in world space.
    \param[in] light Light data.
    \param[in] float 2 random.
    \param[out] ls Light sample struct.
    \return True if a sample was generated, false otherwise.
*/
bool sampleDiscAreaLight(const float3 shadingPosW, const LightData light, const float4 rnd, out AnalyticLightSample ls) {
    // Sample a random point on the disk.
    // TODO: Fix spelling disagreement between disc vs disk.
    float3 pos = float3(sample_disk_concentric(rnd.xy), 0.f);

    // Transform to world space.
    ls.posW = mul(float4(pos, 1.f), light.transMat).xyz;

    // Setup world space normal.
    ls.normalW = normalize(mul(float4(0.f, 0.f, -1.f, 0.f), light.transMatIT).xyz);

    if (! validateAreaLightSample(shadingPosW, light, ls)) return false;

    return true;
}

bool intersectDiscAreaLight(const float3 shadingPosW, const float3 dir, const LightData light, out AnalyticLightSample ls) {
    
    float3 localPos = mul(float4(shadingPosW, 1.f), light.transMatInv).xyz;
    float3 localDir = normalize(mul(float4(dir, 0.f), light.transMatInv).xyz);

    float isect = intersect_plane(localPos, localDir);
    
    localPos += isect * localDir;
    if ((localPos.x * localPos.x + localPos.y * localPos.y) > 1.f) return false;
    ls.posW = mul(float4(localPos, 1.f), light.transMat).xyz;

    ls.normalW = normalize(mul(float4(0.f, 0.f, -1.f, 0.f), light.transMatIT).xyz);

    if (!validateAreaLightSample(shadingPosW, light, ls)) return false;

    return true;
}

/** Samples a distant light source.
    \param[in] shadingPosW Shading point in world space.
    \param[in] light Light data.
    \param[in] float2 random.
    \param[out] ls Light sample struct.
    \return True if a sample was generated, false otherwise.
*/
bool sampleDistantLight(const float3 shadingPosW, const LightData light, const float4 rnd, out AnalyticLightSample ls) {
    // A distant light doesn't have a position. Just clear to zero.
    ls.posW = float3(0.f, 0.f, 0.f);

    float3 dir = sample_cone(rnd.xy, light.cosSubtendedAngle);

    // Transform sampled direction to world space
    ls.dir = ls.shadowDir = normalize(mul(dir, (float3x3)light.transMat));
    ls.normalW = -ls.dir;
    ls.distance = kMaxLightDistance;

    // Compute incident radiance at shading point.
    // A DistantLight's angle defines the solid angle it subtends.
    // But because the angle is intended to affect penumbra size, but not
    // perceived brigthness, we treat intensity as radiance.
    ls.i = 1.0f;
    ls.Li = light.directDiffuseIntensity;
    
    ls.pdf = 1.f / ((float)M_2PI * (1.f - light.cosSubtendedAngle));
    return true;
}

bool intersectDistantLight(const float3 shadingPosW, const float3 dir, const LightData light, out AnalyticLightSample ls) {
    // A distant light doesn't have a position. Just clear to zero.
    ls.posW = float3(0.f, 0.f, 0.f);

    // Transform sampled direction to world space
    ls.dir = ls.shadowDir = normalize(dir);
    ls.normalW = light.dirW;

    if(dot(light.dirW, -ls.dir) < light.cosSubtendedAngle) return false;

    ls.distance = kMaxLightDistance;

    // Compute incident radiance at shading point.
    // A DistantLight's angle defines the solid angle it subtends.
    // But because the angle is intended to affect penumbra size, but not
    // perceived brigthness, we treat intensity as radiance.
    ls.i = 1.0f;
    ls.Li = light.directDiffuseIntensity;
    
    ls.pdf = 1.f / ((float)M_2PI * (1.f - light.cosSubtendedAngle));
    return true;
}

/** Samples a directional light source.
    \param[in] shadingPosW Shading point in world space.
    \param[in] light Light data.
    \param[out] ls Light sample struct.
    \return True if a sample was generated, false otherwise.
*/
bool sampleDirectionalLight(const float3 shadingPosW, const LightData light, out AnalyticLightSample ls) {
    // A directional light doesn't have a position. Just clear to zero.
    ls.posW = float3(0, 0, 0);

    // For a directional light, the normal is always along its light direction.
    ls.normalW = light.dirW;

    // Setup direction and distance to light.
    ls.distance = kMaxLightDistance;
    ls.dir = ls.shadowDir = -light.dirW;

    // Setup incident radiance. For directional lights there is no falloff or cosine term.
    ls.i = 1.0f;
    ls.Li = light.directDiffuseIntensity;
    
    // For a directional light, the PDF with respect to solid angle is a Dirac function. Set to zero.
    ls.pdf = 0.f;

    return true;
}

/** Samples a point (spot) light.
    \param[in] shadingPosW Shading point in world space.
    \param[in] light Light data.
    \param[out] ls Light sample struct.
    \return True if a sample was generated, false otherwise.
*/
bool samplePointLight(const float3 shadingPosW, const LightData light, const float4 rnd, out AnalyticLightSample ls) {
    // Get the position and normal.
    ls.posW = light.posW;
    ls.normalW = light.dirW;

    // Compute direction and distance to light.
    // The distance is clamped to a small epsilon to avoid div-by-zero below.
    float3 toLight = ls.posW - shadingPosW;
    float distSqr = max(dot(toLight, toLight), kMinLightDistSqr);
    ls.distance = sqrt(distSqr);
    ls.dir = ls.shadowDir = toLight / ls.distance;

    // Calculate the falloff for spot-lights.
    float cosTheta = -dot(ls.dir, light.dirW);
    float falloff = 1.f;
    
    if (cosTheta < light.cosOpeningAngle) {
        // Outside spot light cone. Sample is invalid
        return false;
    } else if (light.penumbraAngle > 0.f) {
        // Inside spol light cone
        float3 shadowDir = float3(sample_disk(rnd.xy) * light.radius, .0f);

        // Transform sampled shadow direction to world space
        ls.shadowDir = normalize(ls.posW + mul(shadowDir, (float3x3)light.transMat) - shadingPosW);
        
        float deltaAngle = light.openingAngle - acos(cosTheta);
        falloff = smoothstep(0.f, light.penumbraAngle, deltaAngle);
    } else {
        // Point light shadows
        ls.shadowDir = normalize(ls.posW + mul(sample_sphere(rnd.xy) * light.radius, (float3x3)light.transMat) - shadingPosW);
    }

    // Compute incident radiance at shading point.
    float _k = falloff / distSqr;
    ls.i = _k;
    ls.Li = light.directDiffuseIntensity * _k;
    
    // For a point light, the PDF with respect to solid angle is a Dirac function. Set to zero.
    ls.pdf = 0.f;

    return true;
}

bool sampleEnvironmentLight(const float3 shadingPosW, const float3 shadingNormalW, const LightData light, const float4 rnd, out AnalyticLightSample ls) {
    // An environmentl light doesn't have a position. Just clear to zero.
    ls.posW = float3(0, 0, 0);

    ls.distance = kMaxLightDistance;
    ls.dir = ls.shadowDir = sample_hemisphere_oriented(rnd.xy, shadingNormalW); 

    ls.normalW = -normalize(ls.dir);

    ls.i = 1.0f;
    ls.Li = light.directDiffuseIntensity;

    ls.pdf = 1.0f;
    
    return true;
}

bool intersectEnvironmentLight(const float3 shadingPosW, const float3 dir, const LightData light, out AnalyticLightSample ls) {
    // An environmentl light doesn't have a position. Just clear to zero.
    ls.posW = float3(0, 0, 0);

    ls.distance = kMaxLightDistance;
    ls.dir = ls.shadowDir = normalize(dir);

    ls.normalW = -normalize(ls.dir);

    ls.i = 1.0f;
    ls.Li = light.directDiffuseIntensity;

    ls.pdf = 0.0f;

    return true;
}

/** Samples an analytic light source.
    This function calls the correct sampling function depending on the type of light.
    \param[in] shadingPosW Shading point in world space.
    \param[in] light Light data.
    \param[in] 2d random number.
    \param[out] ls Sampled point on the light and associated sample data, only valid if true is returned.
    \return True if a sample was generated, false otherwise.
*/
bool sampleLight(const float3 shadingPosW, const float3 shadingNormalW, const LightData light, const float4 rnd, out AnalyticLightSample ls) {
    // Sample the light based on its type: point, directional, or area.
    switch (light.type) {
        case LightType::Point:
            return samplePointLight(shadingPosW, light, rnd, ls);
        case LightType::Directional:
            return sampleDirectionalLight(shadingPosW, light, ls);
        case LightType::Rect:
#if AREA_LIGHTS_SAMPLING_SPHERICAL_SOLID_ANGLE_URENA
            //return sampleRectAreaLightSAU(shadingPosW, shadingNormalW, light, rnd, ls);
            return sampleRectAreaLightRIS(shadingPosW, shadingNormalW, light, rnd, ls);
#else
            return sampleRectAreaLight(shadingPosW, light, rnd, ls);
#endif
        case LightType::Sphere:
#if AREA_LIGHTS_SAMPLING_SPHERICAL_SOLID_ANGLE_URENA
            return sampleSphereAreaLightSAU(shadingPosW, shadingNormalW, light, rnd, ls);
#else
            return sampleSphereAreaLight(shadingPosW, light, rnd, ls);
#endif
        case LightType::Disc:
            return sampleDiscAreaLight(shadingPosW, light, rnd, ls);
        case LightType::Distant:
            return sampleDistantLight(shadingPosW, light, rnd, ls);
        case LightType::Env:
            return sampleEnvironmentLight(shadingPosW, shadingNormalW, light, rnd, ls);
        default:
            ls = {};
            return false; // Should not happen
    }
}

/** Evaluates a light approximately. This is useful for raster passes that don't use stochastic integration.
    For now only point and directional light sources are supported.
    \param[in] shadingPosW Shading point in world space.
    \param[in] light Light data.
    \param[out] ls Sampled point on the light and associated sample data (zero initialized if no sample is generated).
    \return True if a sample was generated, false otherwise.
*/
bool evalLightApproximate(const float3 shadingPosW, const LightData light, out AnalyticLightSample ls) {
    switch (light.type) {
        case LightType::Point:
            return samplePointLight(shadingPosW, light, float4(.0f), ls);
        case LightType::Directional:
            return sampleDirectionalLight(shadingPosW, light, ls);
        default:
            ls = {};
            return false;
    }

    // All other light types are not supported.
    return false;
}

bool intersectLight(const float3 shadingPosW, const float3 dir, const LightData light, out AnalyticLightSample ls) {
    switch (light.type) {
        case LightType::Sphere:
            return intersectSphereAreaLight(shadingPosW, dir, light, ls);
        case LightType::Rect:
            return intersectRectAreaLight(shadingPosW, dir, light, ls);
        case LightType::Disc:
            return intersectDiscAreaLight(shadingPosW, dir, light, ls);
        case LightType::Distant:
            return intersectDistantLight(shadingPosW, dir, light, ls);
        case LightType::Env:
            return intersectEnvironmentLight(shadingPosW, dir, light, ls);
        default:
            ls = {};
            return false;
    }
}