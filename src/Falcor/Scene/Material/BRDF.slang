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

#define PI 3.14159265359

import Scene.ShadingData;
import Scene.Lights.Lights;

float schlickWeight(float cosTheta) {
    float m = clamp(1.0f - cosTheta, 0.0f , 1.0f);
    return (m * m) * (m * m) * m;
}

float3 fresnelSchlick(float3 f0, float3 f90, float cosTheta) {
    float m = clamp(1.0f - cosTheta, 0.0f , 1.0f);
    return f0 + (f90 - f0) * (m * m) * (m * m) * m;
}

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
float3 evalDiffuseFrostbiteBrdf(ShadingData sd, LightSample ls) {
    float energyBias = lerp(0, 0.5, sd.linearRoughness);
    float energyFactor = lerp(1, 1.0 / 1.51, sd.linearRoughness);

    float fd90 = energyBias + 2 * ls.LdotH * ls.LdotH * sd.linearRoughness;
    float fd0 = 1;
    float lightScatter = fresnelSchlick(fd0, fd90, ls.NdotL).r;
    float viewScatter = fresnelSchlick(fd0, fd90, sd.NdotV).r;
    return (viewScatter * lightScatter * energyFactor * M_1_PI) * sd.diffuse.rgb;
}

float3 evalDiffuseFrostbiteBrdf2(ShadingData sd, LightSample ls) {
    float energyBias = lerp(0, 0.5, sd.linearRoughness);
    float energyFactor = lerp(1, 1.0 / 1.51, sd.linearRoughness);

    float fd90 = energyBias + 2 * ls.LdotH * ls.LdotH * sd.linearRoughness;
    float fd0 = 1;
    float lightScatter = fresnelSchlick(fd0, fd90, ls.LdotH).r;
    float viewScatter = fresnelSchlick(fd0, fd90, sd.NdotV).r;

    float fr = (sd.IoR - 1.f) / (sd.IoR + 1.f);
    float FT0 = 1.0 - fr * fr;
    float FT90 = 0.0;
    float reflectance = lerp(FT0, FT90, schlickWeight(sd.NdotV));

    return (viewScatter * lightScatter * energyFactor * M_1_PI) * reflectance * sd.diffuse.rgb;
}

/** Oren-Nayar diffuse
*/
float3 evalOrenNayarDiffuse(ShadingData sd, LightSample ls) {
    float a = sd.ggxAlpha;
    float3 color = sd.diffuse.rgb;
    float theta_i = acos(ls.NdotL);
    float theta_r = acos(sd.NdotV);
    float LdotV = dot(normalize(ls.L), normalize(sd.V));
    float cos_phi_diff = (LdotV - ls.NdotL*sd.NdotV) / (sin(theta_i)*sin(theta_r) + FLT_EPSILON);
    float alpha = max(theta_i, theta_r);
    float beta  = min(theta_i, theta_r);
    
    if (alpha > M_PI * 0.5) {
        return float3(0);
    }

    float C1 = 1.0 - 0.5 * a / (a + 0.33);
    float C2 = 0.45 * a / (a + 0.09);
    
    if (cos_phi_diff >= 0.0) {
        C2 *= sin(alpha);
    } else {
        C2 *= (sin(alpha) - pow(2.0*beta / M_PI, 3.0));
    }
    
    float C3 = 0.125 * a / (a + 0.09) * pow((4.0*alpha*beta)/M_PI_2, 2.0);

    float L1 = C1 + cos_phi_diff * C2 * tan(beta) + (1.0 - abs(cos_phi_diff)) * C3 * tan((alpha+beta)/2.0);
    float L2 = 0.17 * (a / (a + 0.13)) * (1.0 - cos_phi_diff * (4.0 * beta * beta) / (M_PI_2));
    //return color * (L1 * M_1_PI) + color * color * (L2 * M_1_PI);
    return color * color * (L2 * M_1_PI);
}

/** Oren-Nayar diffuse
*/
float3 evalOrenNayarDiffuse2(ShadingData sd, LightSample ls) {
    float roughness2 = sd.ggxAlpha;
    float3 color = sd.diffuse.rgb;
    float theta_i = acos(ls.NdotL);
    float theta_r = acos(sd.NdotV);
    float LdotV = dot(normalize(ls.L), normalize(sd.V));
    float3 vDir = normalize(sd.V);
    float3 lDir = normalize(ls.L);
    float3 N = sd.N;

    float a = 1.0 - 0.5 * roughness2 / (roughness2 + 0.33);
    float b = 0.45 * roughness2 / (roughness2 + 0.09);

    float cosPhi = dot(normalize(vDir - sd.NdotV * N), normalize(lDir - ls.NdotL * N));
    
    float sinNV = sqrt(1.0 - sd.NdotV * sd.NdotV);
    float sinNL = sqrt(1.0 - ls.NdotL * ls.NdotL);
    
    float s = sd.NdotV < ls.NdotL ? sinNV : sinNL;
    float t = sd.NdotV > ls.NdotL ? sinNV / sd.NdotV : sinNL / ls.NdotL;
    return sd.diffuse.rgb * M_1_PI * (a + b * cosPhi * s * t);
}

/** Qualitative Oren-Nayar diffuse
*/
float3 evalQualitativeOrenNayarDiffuse(ShadingData sd, LightSample ls) {
    float a = sd.ggxAlpha;
    float3 color = sd.diffuse.rgb;
    float theta_i = acos(ls.NdotL);
    float theta_r = acos(sd.NdotV);
    float LdotV = dot(normalize(ls.L), normalize(sd.V));
    float cos_phi_diff = (LdotV - ls.NdotL*sd.NdotV) / (sin(theta_i)*sin(theta_r) + FLT_EPSILON);
    float alpha = max(theta_i, theta_r);
    float beta  = min(theta_i, theta_r);
    
    if (alpha > M_PI * 0.5) {
        return float3(0);
    }

    float A = 1.0 - 0.5 * a / (a + 0.33);
    float B = 0.45 * a / (a + 0.09);
    float C = sin(alpha) * tan(beta);
    float L1 = A + B * max(0.0, cos_phi_diff) * C;

    float fr = (sd.IoR - 1.f) / (sd.IoR + 1.f);
    float FT0 = 1.0 - fr * fr;
    float FT90 = 0.0;
    float transmittance = lerp(FT0, FT90, schlickWeight(sd.NdotV));
    //transmittance = lerp(0.5, 0.0, schlickWeight(sd.NdotV));

    return color * (L1 * M_1_PI) * transmittance;
}

/** Improved Oren-Nayar diffuse
*/
float3 evalImprovedOrenNayarDiffuse(ShadingData sd, LightSample ls) {
    float a = sd.ggxAlpha;
    float3 color = sd.diffuse.rgb;
    float theta_i = acos(ls.NdotL);
    float theta_r = acos(sd.NdotV);
    float LdotV = dot(normalize(ls.L), normalize(sd.V));
    float cos_phi_diff = (LdotV - ls.NdotL*sd.NdotV) / (sin(theta_i)*sin(theta_r) + FLT_EPSILON);


    float A1 = 1.0 - 0.5 * a/(a + 0.33);
    float A2 = 0.17 * a / (a + 0.13);
    
    float B = 0.45 * a / (a + 0.09);
    float s = cos_phi_diff * sin(theta_i) * sin(theta_r);
    float t = 1.0;
    if (s > 0.0) {
        t = max(ls.NdotL, sd.NdotV);
    }
    float3 L1 = color * (A1 + B * s / t);
    float3 L2 = color * color * A2;

    return (L1 + L2) * M_1_PI;
}

float3 evalDiffuseBrdf(ShadingData sd, LightSample ls) {

return evalQualitativeOrenNayarDiffuse(sd, ls);

#if DiffuseBrdf == DiffuseBrdfLambert
    return evalDiffuseLambertBrdf(sd, ls);
#elif DiffuseBrdf == DiffuseBrdfDisney
    return evalDiffuseDisneyBrdf(sd, ls);
#elif DiffuseBrdf == DiffuseBrdfFrostbite
    return evalDiffuseFrostbiteBrdf(sd, ls);
#endif
}

float GTR2(float NdotH, float a) {
    float a2 = a * a;
    float t = 1.0f + (a2 - 1.0f) * NdotH * NdotH;
    return a2 / (PI * t * t);
}

float D_GGX(float NdotH , float a ) {
    // Divide by PI is apply later
    float a2 = a * a ;
    float f = 1.0f + ( NdotH * a2 - NdotH ) * NdotH;
    return a2 / (f * f) ;
}

float smithG_GGX(float NdotV, float ggxAlpha) {
    float a = ggxAlpha * ggxAlpha;
    float b = NdotV * NdotV;
    return 1. / (abs(NdotV) + max(sqrt(a + b - a * b), 0.0001));
}

float evalGGX(float ggxAlpha, float NdotH) {
    float a2 = ggxAlpha * ggxAlpha;
    float d = ((NdotH * a2 - NdotH) * NdotH + 1);
    return a2 / (d * d);
}

float evalSmithGGX(float NdotL, float NdotV, float ggxAlpha) {
    // Optimized version of Smith, already taking into account the division by (4 * NdotV)
    float a2 = ggxAlpha * ggxAlpha;
    // `NdotV *` and `NdotL *` are inversed. It's not a mistake.
    float ggxv = NdotL * sqrt((-NdotV * a2 + NdotV) * NdotV + a2);
    float ggxl = NdotV * sqrt((-NdotL * a2 + NdotL) * NdotL + a2);
    return 0.5f / (ggxv + ggxl);

}

float3 evalSpecularBrdf(ShadingData sd, LightSample ls) {
    float ggxAlpha = sd.ggxAlpha;

    float D = evalGGX(ggxAlpha, ls.NdotH);
    float G = evalSmithGGX(ls.NdotL, sd.NdotV, ggxAlpha);
    float3 F = fresnelSchlick(sd.specular, 1, saturate(ls.LdotH));
    return D * G * F * M_1_PI;
}

float3 evalSpecularBrdfMantra_(ShadingData sd, LightSample ls) {
    float ggxAlpha = sd.ggxAlpha;

    float D = evalGGX(ggxAlpha, ls.NdotH);
    float G = evalSmithGGX(ls.NdotL, sd.NdotV, ggxAlpha);
    float3 F = fresnelSchlick(sd.specular, 1, saturate(ls.LdotH));
    
    float a = saturate(ls.NdotL);

    float fr = (sd.IoR - 1.f) / (sd.IoR + 1.f);
    float FR0 = fr * fr;
    float FR90 = 1.0;
    float reflectance = lerp(FR0, FR90, schlickWeight(sd.NdotV));
    reflectance = lerp(1 - FR0, 0.0, schlickWeight(sd.NdotV));

    return D * G * F * M_1_PI * lerp(a, 1, reflectance);
}

float3 evalSpecularBrdfMantra(ShadingData sd, LightSample ls) {
    float ggxAlpha = sd.ggxAlpha;

    float specularTint = 0.0;

    float Cdlum = .3 * sd.specular.r + .6 * sd.specular.g + .1 * sd.specular.b; // luminance approx.

    float3 Ctint = Cdlum > 0. ? sd.specular / Cdlum : float3(1.); // normalize lum. to isolate hue+sat
    float3 Cspec0 = lerp(sd.specular *.08 * lerp(float3(1.), Ctint, specularTint), sd.specular, sd.metallic);
    
    float Ds = GTR2(ls.NdotH, ggxAlpha);
    float FH = schlickWeight(ls.LdotH);
    float3 Fs = lerp(Cspec0, float3(1), FH);
    float Gs;
    Gs  = smithG_GGX(ls.NdotL, ggxAlpha);
    Gs *= smithG_GGX(sd.NdotV, ggxAlpha);
    
    return Gs*Fs*Ds;
}