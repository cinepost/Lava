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
#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
import Scene.Shading;
import Utils.Helpers;

#define gCamera gScene.camera
#define gFrameDim gRenderTargetDim
#include "RenderPasses/HBAO/HBAO.Common.slangh"

cbuffer PerFrameCB {

  float2 gRenderTargetDim;
  uint2  gNoiseOffset;

  float4x4 gViewInvMat;

  int gUseAO;
  float gAoFactor;
  int gUseBentNormals;

  // these values needed when deffered ao is not applicable. e.g. transparent objects rendering
  float gAoQuality;
  float gAoDistance;
  int gHizMipOffset;  
};

SamplerState gSampler;
SamplerState gNoiseSampler;
SamplerState gDepthSampler;

Buffer<float2> mipRatio; // to correct texel misalignment
Texture2D gDepthTex;
Texture2D gMaxZBuffer;

Texture2D aoHorizons;
Texture2D gNoiseTex;
Texture2D visibilityBuffer;

static VSOut vsData;

void integrate_slice(float3 normal, float2 t_phi, float2 horizons, inout float visibility, inout float3 bent_normal) {
  
  /* Projecting Normal to Plane P defined by t_phi and omega_o */
  float3 np = float3(t_phi.y, -t_phi.x, 0.0); /* Normal vector to Integration plane */
  float3 t = float3(-t_phi, 0.0);
  float3 n_proj = normal - np * dot(np, normal);
  float n_proj_len = max(1e-16, length(n_proj));

  float cos_n = clamp(n_proj.z / n_proj_len, -1.0, 1.0);
  float n = sign(dot(n_proj, t)) * fast_acos(cos_n); /* Angle between view vec and normal */

  /* (Slide 54) */
  float2 h = fast_acos(horizons);
  h.x = -h.x;

  /* Clamping thetas (slide 58) */
  h.x = n + max(h.x - n, -M_PI_2);
  h.y = n + min(h.y - n, M_PI_2);

  /* Solving inner integral */
  float2 h_2 = 2.0 * h;
  float2 vd = -cos(h_2 - n) + cos_n + h_2 * sin(n);
  float vis = saturate((vd.x + vd.y) * 0.25 * n_proj_len);

  visibility += vis;

  /* O. Klehm, T. Ritschel, E. Eisemann, H.-P. Seidel
   * Bent Normals and Cones in Screen-space
   * Sec. 3.1 : Bent normals */
  float b_angle = (h.x + h.y) * 0.5;
  bent_normal += float3(sin(b_angle) * -t_phi, cos(b_angle)) * vis;
}

void gtao_deferred(float3 normal, float4 noise, int2 ipos, out float visibility, out float3 bent_normal) {
  float grag_depth; // Unused !?

  /* Fetch early, hide latency! */
  float4 horizons = aoHorizons.Load(uint3(ipos, 0));

  float4 dirs;
  dirs.xy = getAoDir(noise.x * 0.5);
  dirs.zw = getAoDir(noise.x * 0.5 + 0.5);

  bent_normal = normal * 1e-8;
  visibility = 1e-8;

  horizons = unpackHorizons(horizons);

  integrate_slice(normal, dirs.xy, horizons.xy, visibility, bent_normal);
  integrate_slice(normal, dirs.zw, horizons.zw, visibility, bent_normal);

  bent_normal = normalize(bent_normal / visibility);

  visibility *= 0.5; /* We integrated 2 slices. */
}

void gtao(float3 normal, float3 posV, float4 noise, out float visibility, out float3 bent_normal) {
  float2 uvs = getUVsFromView(posV);

  float2 max_dir = getMaxDir(posV.z);
  float2 dir = getAoDir(noise.x);

  bent_normal = normal * 1e-8;
  visibility = 1e-8;

  /* Only trace in 2 directions. May lead to a darker result but since it's mostly for
   * alpha blended objects that will have overdraw, we limit the performance impact. */
  float2 horizons = search_horizon_sweep(dir, posV, uvs, noise.y, max_dir);
  integrate_slice(normal, dir, horizons, visibility, bent_normal);

  bent_normal = normalize(bent_normal / visibility);
}

/* Multibounce approximation base on surface albedo.
 * Page 78 in the .pdf version. */
float gtao_multibounce(float visibility, float3 albedo) {
  float aoBounceFac = 0.5;
  if (aoBounceFac == 0.0) {
    return visibility;
  }

  /* Median luminance. Because Colored multibounce looks bad. */
  float lum = dot(albedo, float3(0.3333));

  float a = 2.0404 * lum - 0.3324;
  float b = -4.7951 * lum + 0.6417;
  float c = 2.7552 * lum + 0.6903;

  float x = visibility;
  return max(x, ((x * a + b) * x + c) * x);
}

float specular_occlusion(float NV, float AO, float roughness) {
  return saturate(pow(NV + AO, roughness) - 1.0 + AO);
}

/* Use the right occlusion  */
float hbaoOcclusionCompute(float3 N, float3 posW, int2 ipos, float user_visibility, float4 noise, out float3 bent_normal) {
  
#ifndef USE_REFRACTION
  if (gUseAO != 0) {
    float visibility;
    float3 vnor = mul(N, float3x3(gScene.camera.data.viewMat)) * float3(1, -1, 1);
    
#  ifdef ENABLE_DEFERED_AO
    gtao_deferred(vnor, noise, ipos, visibility, bent_normal);
#  else
    float3 posV = mul(posW, gCamera.data.viewMat);
    gtao(vnor, posV, noise, visibility, bent_normal);
#  endif

    /* Prevent some problems down the road. */
    visibility = max(1e-3, visibility);

    if (gUseBentNormals != 0) {
      /* The bent normal will show the facet look of the mesh. Try to minimize this. */
      float mix_fac = visibility * visibility * visibility;
      bent_normal = normalize(lerp(bent_normal, vnor, mix_fac));

      bent_normal = transformDirection(gScene.camera.data.viewProjMat, bent_normal);
    }
    else {
      bent_normal = N;
    }

    /* Scale by user factor */
    visibility = pow(visibility, gAoFactor);

    return min(visibility, user_visibility);
  }
#endif

  bent_normal = N;
  return user_visibility;
}

// this one works strange
float hbaoSpecularOcclusionCompute(float3 N, float3 bentNormal, float finalAO, int2 ipos, float roughness, inout float3 specularDir) {
  specularDir = normalize(lerp(specularDir, bentNormal, roughness * (1.0 - finalAO)));

  /* Visibility to cone angle (eq. 18). */
  float vis_angle = fast_acos(sqrt(1 - finalAO));
  
  /* Roughness to cone angle (eq. 26). */
  float spec_angle = max(0.001, fast_acos(cone_cosine(roughness)));
  
  /* Angle between cone axes. */
  float cone_cone_dist = fast_acos(saturate(dot(bentNormal, specularDir)));
  float cone_nor_dist = fast_acos(saturate(dot(N, specularDir)));

  float isect_solid_angle = spherical_cap_intersection(vis_angle, spec_angle, cone_cone_dist);
  float specular_solid_angle = spherical_cap_intersection(M_PI_2, spec_angle, cone_nor_dist);
  float specular_occlusion = isect_solid_angle / specular_solid_angle;
  
  /* Mix because it is unstable in unoccluded areas. */
  finalAO = lerp(isect_solid_angle / specular_solid_angle, 1.0, pow(finalAO, 8.0));

  /* Scale by user factor */
  finalAO = pow(saturate(finalAO), gAoFactor);
  return finalAO;
}

VSOut vs(VSIn vIn)
{
    VSOut vsOut;
    vsOut = defaultVS(vIn);
#ifdef _OUTPUT_MOTION_VECTORS
    vsOut.prevPosH.xy += vsOut.prevPosH.w * 2 * float2(gScene.camera.data.jitterX, gScene.camera.data.jitterY);
#endif
    return vsOut;
}

struct PsOut
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
#ifdef _OUTPUT_MOTION_VECTORS
    float2 motion : SV_TARGET2;
#endif
};

[earlydepthstencil]
PsOut ps(VSOut vOut, uint triangleIndex : SV_PrimitiveID) {
    PsOut psOut;

    // pixel position
    int2 ipos = int2(vOut.posH.xy);

    // screen space texture coordinates gradient

    float2 gradX = ddx_fine(vOut.texC) * 0.5;// * gRenderTargetDim.x; ///< Gradient of texture coordinate in the screen-space X direction
    float2 gradY = ddy_fine(vOut.texC) * 0.5;// * gRenderTargetDim.y; ///< Gradient of texture coordiante in teh screen-space Y direction
    
    float3 viewDir = normalize(gScene.camera.getPosition() - vOut.posW);
    
    //ShadingData sd = prepareShadingData(vOut, triangleIndex, viewDir);
    ShadingData sd = prepareShadingData(vOut, triangleIndex, viewDir, gradX, gradY);

    float4 finalColor = float4(0, 0, 0, 1);

    float diffuseAO = 1.0f;
    float specularAO = 1.0f;

#ifdef USE_SSAO
    float4 noise = gNoiseTex.SampleLevel(gNoiseSampler, ((ipos + gNoiseOffset) / float2(64)), 0);
    float3 bentNormal;
    float user_visibility = 1.0; // TODO: fetch matrial provided occlusion texture
    
    float3 faceNormal = gScene.getFaceNormalW(vsOut.meshInstanceID, triangleIndex); // Maybe faceNormal should work better than shadingNormal ?

    diffuseAO = hbaoOcclusionCompute(sd.N, vOut.posW, ipos, user_visibility, noise, bentNormal);
    specularAO = hbaoSpecularOcclusion(sd.NdotV, diffuseAO, 1.0f - sd.linearRoughness);
#endif

    for (uint l = 0; l < gScene.getLightCount(); l++) {
        float shadowFactor = 1;
        if (l == 0) {
            //shadowFactor = visibilityBuffer.Load(int3(vOut.posH.xy, 0)).r;
            //shadowFactor *= sd.opacity;
        }
        finalColor.rgb += evalMaterial(sd, gScene.getLight(l), shadowFactor).color.rgb;
    }


    // Add the emissive component
    finalColor.rgb += sd.emissive;
    
    finalColor.a = sd.opacity;

    // World light probe (env light) 
    ShadingResult env = evalMaterialIBL(sd, gScene.lightProbe, ipos);
  
    env.diffuse *= diffuseAO;
    env.specular *= specularAO;
    
    finalColor.rgb += env.diffuse + env.specular;

    // Final
    psOut.color = finalColor;
    psOut.normal = float4(vOut.normalW * 0.5f + 0.5f, 1.0f);

#ifdef _OUTPUT_MOTION_VECTORS
    // Using vOut.posH.xy as pixel coordinate since it has the SV_Position semantic.
    psOut.motion = calcMotionVector(vOut.posH.xy, vOut.prevPosH, gRenderTargetDim);
#endif

#if defined(_VISUALIZE_CASCADES) && defined(_ENABLE_SHADOWS)
    float3 cascadeColor = visibilityBuffer.Load(int3(ipos, 0)).gba;
    psOut.color.rgb *= cascadeColor;
#endif

    // DEBUG
    //psOut.color.rgb = sd.diffuse;

    // Return
    return psOut;
}
