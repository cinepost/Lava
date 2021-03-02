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

/** This file contains legacy light evaluation functions used by rasterization.
    It should be merged/replaced with the BSDF code for ray tracing.
*/

#include "Utils/Math/MathConstants.slangh"

import Utils.Helpers;
import Scene.ShadingData;

//import Scene.Material.BRDFCommon;
__exported import Scene.Lights.LightData;
__exported import Scene.Lights.LightProbeData;

struct LightSample
{
    float3 diffuse;   // The light intensity at the surface location used for the diffuse term
    float3 specular;  // The light intensity at the surface location used for the specular term. For light probes, the diffuse and specular components are different
    float3 L;         // The direction from the surface to the light source
    float3 posW;      // The world-space position of the light
    
    float NdotH;      // Unclamped, can be negative
    float NdotL;      // Unclamped, can be negative
    float LdotH;      // Unclamped, can be negative
    float distance;   // Distance from the light-source to the surface
};

void calcCommonLightProperties(ShadingData sd, inout LightSample ls)
{
    float3 H = normalize(sd.V + ls.L);
    ls.NdotH = dot(sd.N, H);
    ls.NdotL = dot(sd.N, ls.L);
    ls.LdotH = dot(ls.L, H);
};

float getDistanceFalloff(float distSquared)
{
    float falloff = 1 / ((0.01 * 0.01) + distSquared); // The 0.01 is to avoid infs when the light source is close to the shading point
    return falloff;
}

/** Evaluate a directional light source intensity/direction at a shading point
*/
LightSample evalDirectionalLight(in LightData light, in float3 surfacePosW)
{
    LightSample ls;
    ls.diffuse = light.intensity;
    ls.specular = light.intensity;

    ls.L = -normalize(light.dirW);
    float dist = 1.0; //length(surfacePosW - light.posW);
    ls.posW = surfacePosW - light.dirW * dist;
    return ls;
}

/** Evaluate a point light source intensity/direction at a shading point
*/
LightSample evalPointLight(in LightData light, in float3 surfacePosW)
{
    LightSample ls;
    ls.posW = light.posW;
    ls.L = light.posW - surfacePosW;
    // Avoid NaN
    float distSquared = dot(ls.L, ls.L);
    ls.distance = (distSquared > 1e-5f) ? length(ls.L) : 0;
    ls.L = (distSquared > 1e-5f) ? normalize(ls.L) : 0;

    // Calculate the falloff
    float falloff = getDistanceFalloff(distSquared);

    // Calculate the falloff for spot-lights
    float cosTheta = -dot(ls.L, light.dirW); // cos of angle of light orientation
    if (cosTheta < light.cosOpeningAngle)
    {
        falloff = 0;
    }
    else if (light.penumbraAngle > 0)
    {
        float deltaAngle = light.openingAngle - acos(cosTheta);
        falloff *= saturate((deltaAngle - light.penumbraAngle) / light.penumbraAngle);
    }
    ls.diffuse = light.intensity * falloff;
    ls.specular = ls.diffuse;
    return ls;
}

float linearRoughnessToLod(float linearRoughness, float mipCount)
{
    return sqrt(linearRoughness) * (mipCount - 1);
}

/** Evaluate a light source intensity/direction at a shading point
*/
LightSample evalLight(LightData light, ShadingData sd)
{
    LightSample ls;
    if (light.type == uint(LightType::Directional) || light.type == uint(LightType::Distant)) {
      ls = evalDirectionalLight(light, sd.posW);
    } else if (light.type == uint(LightType::Point)) {
      ls = evalPointLight(light, sd.posW);
    }
    calcCommonLightProperties(sd, ls);
    return ls;
};

float3 getDiffuseDominantDir(float3 N, float3 V, float ggxAlpha)
{
    float a = 1.02341 * ggxAlpha - 1.51174;
    float b = -0.511705 * ggxAlpha + 0.755868;
    float factor = saturate((saturate(dot(N, V)) * a + b) * ggxAlpha);
    return normalize(lerp(N, V, factor));
}

float3 getSpecularDominantDir(float3 N, float3 R, float ggxAlpha)
{
    float smoothness = 1 - ggxAlpha;
    float factor = smoothness * (sqrt(smoothness) + ggxAlpha);
    return normalize(lerp(N, R, factor));
}

float3 evalLightProbeDiffuse(LightProbeData probe, ShadingData sd) {
    if(probe.doSolidColor) {
        return float3(1);
    }
    
    float3 N = getDiffuseDominantDir(sd.N, sd.V, sd.ggxAlpha);

    // Interpret negative radius as global light probe with infinite distance
    // Otherwise simulate the light probe as covering a finite spherical area
    float2 uv;
    if(probe.radius < 0.0f)
    {
        uv = dirToSphericalCrd(N);
    }
    else
    {
        float3 intersectPosW;
        intersectRaySphere(sd.posW, N, probe.posW, probe.radius, intersectPosW);
        uv = dirToSphericalCrd(normalize(intersectPosW - probe.posW));
    }

    float width, height, mipCount;
    probe.resources.diffuseTexture.GetDimensions(0, width, height, mipCount);

    float3 diffuseLighting = probe.resources.diffuseTexture.SampleLevel(probe.resources.sampler, uv, 0).rgb;
    return diffuseLighting; 
}

float3 evalLightProbeDiffuseOld(LightProbeData probe, ShadingData sd) {
    float3 N = getDiffuseDominantDir(sd.N, sd.V, sd.ggxAlpha);

    // Interpret negative radius as global light probe with infinite distance
    // Otherwise simulate the light probe as covering a finite spherical area
    float2 uv;
    if(probe.radius < 0.0f)
    {
        uv = dirToSphericalCrd(N);
    } else {
        float3 intersectPosW;
        intersectRaySphere(sd.posW, N, probe.posW, probe.radius, intersectPosW);
        uv = dirToSphericalCrd(normalize(intersectPosW - probe.posW));
    }

    float width, height, mipCount;
    probe.resources.diffuseTexture.GetDimensions(0, width, height, mipCount);

    float3 diffuseLighting = probe.resources.diffuseTexture.SampleLevel(probe.resources.sampler, uv, 0).rgb;
    float preintegratedDisneyBRDF = probe.sharedResources.dfgTexture.SampleLevel(probe.sharedResources.dfgSampler, float2(sd.NdotV, sd.ggxAlpha), 0).z;

    return diffuseLighting * preintegratedDisneyBRDF * sd.diffuse.rgb;
}


float3 evalLightProbeSpecular(LightProbeData probe, ShadingData sd, float3 L) {
    if(probe.doSolidColor) {
        return float3(1);
    }

    float dfgWidth, dfgHeight;
    probe.sharedResources.dfgTexture.GetDimensions(dfgWidth, dfgHeight);

    float width, height, mipCount;
    probe.resources.specularTexture.GetDimensions(0, width, height, mipCount);

    float3 dominantDir = getSpecularDominantDir(sd.N, L, sd.ggxAlpha);
    
    float ggxAlpha = sd.ggxAlpha;
    float mipLevel = linearRoughnessToLod(ggxAlpha, mipCount);

    float2 uv;
    if (probe.radius < 0.0f)
    {
        uv = dirToSphericalCrd(dominantDir);
    } else {
        float3 intersectPosW;
        intersectRaySphere(sd.posW, dominantDir, probe.posW, probe.radius, intersectPosW);
        uv = dirToSphericalCrd(normalize(intersectPosW - probe.posW));
    }

    float3 ld = probe.resources.specularTexture.SampleLevel(probe.resources.sampler, uv, mipLevel).rgb;

    if(sd.metallic < 1.0) {

        float2 dfg = probe.sharedResources.dfgTexture.SampleLevel(probe.sharedResources.dfgSampler, float2(sd.NdotV, ggxAlpha), 0).xy;
        //ld = lerp(ld * (dfg.x + dfg.y), ld, sd.metallic);
    }

    return ld;
}

float3 evalLightProbeSpecularOld(LightProbeData probe, ShadingData sd, float3 L) {
    float dfgWidth, dfgHeight;
    probe.sharedResources.dfgTexture.GetDimensions(dfgWidth, dfgHeight);

    float width, height, mipCount;
    probe.resources.specularTexture.GetDimensions(0, width, height, mipCount);

    float3 dominantDir = getSpecularDominantDir(sd.N, L, sd.ggxAlpha);
    
    float mipLevel = linearRoughnessToLod(sd.ggxAlpha, mipCount);

    float2 uv;
    if (probe.radius < 0.0f)
    {
        uv = dirToSphericalCrd(dominantDir);
    }
    else
    {
        float3 intersectPosW;
        intersectRaySphere(sd.posW, dominantDir, probe.posW, probe.radius, intersectPosW);
        uv = dirToSphericalCrd(normalize(intersectPosW - probe.posW));
    }

    float3 ld = probe.resources.specularTexture.SampleLevel(probe.resources.sampler, uv, mipLevel).rgb;

    if(sd.metallic < 1.0) {

        float2 dfg = probe.sharedResources.dfgTexture.SampleLevel(probe.sharedResources.dfgSampler, float2(sd.NdotV, sd.ggxAlpha), 0).xy;
        ld = lerp(ld * (sd.specular * dfg.x + dfg.y), ld * sd.specular, sd.metallic);
    } else {
        ld *= sd.specular;
    }

    return ld;
}

/** Evaluate a 2D light-probe filtered using linear-filtering
*/
LightSample evalLightProbeLinear2D(LightProbeData probe, ShadingData sd)
{
    LightSample ls;

    // Calculate the reflection vector
    ls.L = reflect(-sd.V, sd.N);
    
    // Evaluate diffuse component
    ls.diffuse = evalLightProbeDiffuse(probe, sd);

    // Get the specular component
    ls.specular = evalLightProbeSpecular(probe, sd, ls.L);

    ls.diffuse *= probe.intensity;
    ls.specular *= probe.intensity;
    ls.posW = probe.posW;
    ls.distance = length(probe.posW - sd.posW);
    calcCommonLightProperties(sd, ls);

    return ls;
}

LightSample evalLightProbeLinear2DOld(LightProbeData probe, ShadingData sd) {
    LightSample ls;

    // Calculate the reflection vector
    ls.L = reflect(-sd.V, sd.N);

    // Evaluate diffuse component
    ls.diffuse = evalLightProbeDiffuseOld(probe, sd);

    // Get the specular component
    ls.specular = evalLightProbeSpecularOld(probe, sd, ls.L);

    ls.diffuse *= probe.intensity;
    ls.specular *= probe.intensity;
    ls.posW = probe.posW;
    ls.distance = length(probe.posW - sd.posW);
    calcCommonLightProperties(sd, ls);

    return ls;
}

/** Evaluate the properties of a light-probe
*/
LightSample evalLightProbeSampleDiffuse(LightProbeData probe, ShadingData sd) {
    LightSample ls;

    // Calculate dominant diffuse vector
    ls.L = sd.N; //getDiffuseDominantDir(sd.N, sd.V, sd.ggxAlpha);

    // Evaluate diffuse component
    ls.diffuse = evalLightProbeDiffuse(probe, sd);

    // Get the specular component
    ls.specular = 0.0f;

    ls.diffuse *= probe.intensity;
    ls.posW = probe.posW;
    ls.distance = length(probe.posW - sd.posW);
    calcCommonLightProperties(sd, ls);

    return ls;
}

LightSample evalLightProbeSampleSpecular(LightProbeData probe, ShadingData sd) {
    LightSample ls;

    // Calculate the reflection vector
    float3 R = reflect(-sd.V, sd.N);
    ls.L = R; //getSpecularDominantDir(sd.N, R, sd.ggxAlpha);

    // Evaluate diffuse component
    ls.diffuse = 0.0f;

    // Get the specular component
    ls.specular = evalLightProbeSpecular(probe, sd, ls.L);

    ls.specular *= probe.intensity;
    ls.posW = probe.posW;
    ls.distance = length(probe.posW - sd.posW);
    calcCommonLightProperties(sd, ls);

    return ls;
}

LightSample evalLightProbeOld(LightProbeData probe, ShadingData sd) {
    return evalLightProbeLinear2DOld(probe, sd);
}

// ------ Filament based code

#define IBL_INTEGRATION_IMPORTANCE_SAMPLING_COUNT 64

float2 hammersley(uint index) {
    // Compute Hammersley sequence
    // TODO: these should come from uniforms
    // TODO: we should do this with logical bit operations
    const uint numSamples = uint(IBL_INTEGRATION_IMPORTANCE_SAMPLING_COUNT);
    const uint numSampleBits = uint(log2(float(numSamples)));
    const float invNumSamples = 1.0 / float(numSamples);
    uint i = uint(index);
    uint t = i;
    uint bits = 0u;
    for (uint j = 0u; j < numSampleBits; j++) {
        bits = bits * 2u + (t - (2u * (t / 2u)));
        t /= 2u;
    }
    return float2(float(i), float(bits)) * invNumSamples;
}

float3 F_Schlick(const float3 f0, float f90, float VoH) {
    // Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"
    return f0 + (f90 - f0) * pow(1.0 - VoH, 5);
}

float3 fresnel(const float3 f0, float LoH) {
    float f90 = saturate(dot(f0, float3(50.0 * 0.33)));
    return F_Schlick(f0, f90, LoH);
}

float D_GGX(float roughness, float NoH, const float3 h) {
    // Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"

    // In mediump, there are two problems computing 1.0 - NoH^2
    // 1) 1.0 - NoH^2 suffers floating point cancellation when NoH^2 is close to 1 (highlights)
    // 2) NoH doesn't have enough precision around 1.0
    // Both problem can be fixed by computing 1-NoH^2 in highp and providing NoH in highp as well

    // However, we can do better using Lagrange's identity:
    //      ||a x b||^2 = ||a||^2 ||b||^2 - (a . b)^2
    // since N and H are unit vectors: ||N x H||^2 = 1.0 - NoH^2
    // This computes 1.0 - NoH^2 directly (which is close to zero in the highlights and has
    // enough precision).
    // Overall this yields better performance, keeping all computations in mediump

    float oneMinusNoHSquared = 1.0 - NoH * NoH;

    float a = NoH * roughness;
    float k = roughness / (oneMinusNoHSquared + a * a);
    float d = k * k * (1.0 / M_PI);
    return saturate(d);
}

float distribution(float roughness, float NoH, const float3 h) {
    return D_GGX(roughness, NoH, h);
}

float V_SmithGGXCorrelated(float roughness, float NoV, float NoL) {
    // Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
    float a2 = roughness * roughness;
    // TODO: lambdaV can be pre-computed for all the lights, it should be moved out of this function
    float lambdaV = NoL * sqrt((NoV - a2 * NoV) * NoV + a2);
    float lambdaL = NoV * sqrt((NoL - a2 * NoL) * NoL + a2);
    float v = 0.5 / (lambdaV + lambdaL);

    // clamp to the maximum value representable 
    return saturate(v);
}

float visibility(float roughness, float NoV, float NoL) {
    return V_SmithGGXCorrelated(roughness, NoV, NoL);
}

float3 importanceSamplingNdfDggx(float2 u, float roughness) {
    // Importance sampling D_GGX
    float a2 = roughness * roughness;
    float phi = 2.0 * M_PI * u.x;
    float cosTheta2 = (1.0 - u.y) / (1.0 + (a2 - 1.0) * u.y);
    float cosTheta = sqrt(cosTheta2);
    float sinTheta = sqrt(1.0 - cosTheta2);
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float3 evalLightProbeSampleSpecularColor(LightProbeData probe, ShadingData sd, int2 pixel_pos) {
    // TODO: for a true anisotropic BRDF, we need a real tangent space
    //float3 up = abs(sd.N.z) < 0.9999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 up = float3(0.0, 1.0, 0.0);

    float3x3 tangentToWorld;
    tangentToWorld[0] = normalize(cross(up, sd.N));
    tangentToWorld[1] = cross(sd.N, tangentToWorld[0]);
    tangentToWorld[2] = sd.N;

    float roughness = sd.linearRoughness;

    const uint numSamples = uint(IBL_INTEGRATION_IMPORTANCE_SAMPLING_COUNT);
    const float invNumSamples = 1.0 / float(numSamples);

    float3 indirectSpecular = float3(0.0);
    
    // Calculate the reflection vector
    float3 R = reflect(-sd.V, sd.N);

    float3 Ls = evalLightProbeSpecular(probe, sd, R);

    for (uint i = 0u; i < numSamples; i++) {
        float2 u = hammersley(i);
        //float2 u = getHammersley(sample_i1, IBL_INTEGRATION_IMPORTANCE_SAMPLING_COUNT);
        
        float3 H = normalize(mul(importanceSamplingNdfDggx(u, roughness), tangentToWorld));

        float3 L = getSpecularDominantDir(sd.N, R, sd.ggxAlpha);
        float NdotL = dot(sd.N, L);

        // Compute this sample's contribution to the brdf
        if (NdotL >= 0.0) {
            float NdotH = saturate(dot(sd.N, H));
            float LdotH = max(saturate(dot(L, H)), 0.0);

            //float3 Ls = evalLightProbeSpecular(probe, sd, R); TODO: sample jittered R

            float D = 1.0f;
            if (roughness != 0.0f) {
                D = distribution(roughness, NdotH, H);
            }
        
            
            float3 F = fresnel(sd.specular.r, LdotH);

            float3 Fr = F * (D * invNumSamples);

            indirectSpecular += (Fr * Ls);
        }
    }

    return indirectSpecular;
}