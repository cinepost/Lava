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

/** This file contains legacy BRDF functions. It will be replaced by the newer BSDF code.
*/

#include "Utils/Math/MathConstants.slangh"
#include "Experimental/Scene/Material/BxDFConfig.slangh"
#include "Scene/Material/BRDFCommon.slangh"
#include "Scene/Material/ParovozBRDF.slangh"

import Scene.ShadingData;
import Scene.Lights.Lights;


/** Disney's diffuse term. Based on https://disney-animation.s3.amazonaws.com/library/s2012_pbs_disney_brdf_notes_v2.pdf
*/

float disneyDiffuseFresnel(float NdotV, float NdotL, float LdotH, float linearRoughness) {
    float fd90 = 0.5 + 2 * LdotH * LdotH * linearRoughness;
    float fd0 = 1;
    float lightScatter = fresnelSchlick(fd0, fd90, NdotL).r;
    float viewScatter = fresnelSchlick(fd0, fd90, NdotV).r;
    return lightScatter * viewScatter;
}

float3 evalDiffuseDisneyBrdf(ShadingData sd, LightSample ls) {
    return disneyDiffuseFresnel(sd.NdotV, ls.NdotL, ls.LdotH, sd.linearRoughness) * M_1_PI * sd.diffuse.rgb;
}

float principledDiffuseFresnel(float NdotV, float NdotL, float LdotH, float linearRoughness) {
    float fd90 = 1.0 + 2 * LdotH * LdotH * linearRoughness;
    float fd0  = 1.0;
    float fl = schlickWeight(NdotL);
    float fv = schlickWeight(abs(NdotV));
    
    return lerp(fd0, fd90, fl) * lerp(fd0, fd90, fv);
}

float3 evalPrincipledDiffuseBrdf(ShadingData sd, LightSample ls) {
    return principledDiffuseFresnel(sd.NdotV, ls.NdotL, ls.LdotH, sd.linearRoughness) * M_1_PI * sd.diffuse.rgb;
}

/** Lambertian diffuse
*/
float3 evalDiffuseLambertBrdf(ShadingData sd, LightSample ls) {
    return sd.diffuse.rgb * (1 / M_PI);
}

/** Frostbites's diffuse term. Based on https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
*/

float frostbiteDiffuseFresnel(float NdotV, float NdotL, float LdotH, float linearRoughness) {
    float energyBias = lerp(0, 0.5, linearRoughness);
    float energyFactor = lerp(1, 1.0 / 1.51, linearRoughness);

    float fd90 = energyBias + 2 * LdotH * LdotH * linearRoughness;
    float fd0 = 1;
    float lightScatter = fresnelSchlick(fd0, fd90, NdotL).r;
    float viewScatter = fresnelSchlick(fd0, fd90, NdotV).r;
    return viewScatter * lightScatter * energyFactor;
}

float3 evalDiffuseFrostbiteBrdf(ShadingData sd, LightSample ls) {
    return frostbiteDiffuseFresnel(sd.NdotV, ls.NdotL, ls.LdotH, sd.linearRoughness) * M_1_PI * sd.diffuse.rgb;
}


float evalDiffuseFresnel(float NdotV, float NdotL, float LdotH, float linearRoughness) {

#if DiffuseBrdf == DiffuseBrdfFrostbite
    return frostbiteDiffuseFresnel(NdotV, NdotL, LdotH, linearRoughness);
#else
    return disneyDiffuseFresnel(NdotV, NdotL, LdotH, linearRoughness);
#endif
}

float3 evalDiffuseBrdf(ShadingData sd, LightSample ls) {

#if DiffuseBrdf == DiffuseBrdfLambert
    return evalDiffuseLambertBrdf(sd, ls);
#elif DiffuseBrdf == DiffuseBrdfDisney
    return evalDiffuseDisneyBrdf(sd, ls);
#elif DiffuseBrdf == DiffuseBrdfFrostbite
    return evalDiffuseFrostbiteBrdf(sd, ls);
#endif
}

float3 evalSpecularBrdfOriginal(ShadingData sd, LightSample ls) {
    float ggxAlpha = sd.ggxAlpha;

    float D = evalGGX(ggxAlpha, ls.NdotH);
    float G = evalSmithGGX(ls.NdotL, sd.NdotV, ggxAlpha);
    float3 F = fresnelSchlick(sd.specular, 1, saturate(ls.LdotH));
    return D * G * F * M_1_PI;
}

float3 evalSpecularBrdf(ShadingData sd, LightSample ls) {
    return evalSpecularBrdfOriginal(sd, ls);
    //return evalParovozSpecular(sd, ls);
}