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
import Scene.Scene;
import Scene.ShadingData;
import Utils.Helpers;

#ifdef _SPHERICAL_MAP
Texture2D gTexture;
#else
TextureCube gTexture;
#endif
SamplerState gSampler;

[[vk::binding(0, 0)]]
cbuffer PerFrameCB : register(b0) {
    float4x4 gWorld;
    float4x4 gViewMat;
    float4x4 gProjMat;
    float3 gIntensity;
    float gScale;
    float gTransparency;
};

[shader("vertex")]
void vs(float4 posL : POSITION, out float3 dir : NORMAL, out float4 posH : SV_POSITION) {

#ifdef _SKYBOX_SOLID_MODE
    dir = float3(1.0, 0.0, 0.0);
#else
    dir = posL.xyz;
#endif

    float4 viewPos = mul(mul(posL, gWorld), gViewMat);
    posH = mul(viewPos, gProjMat);
    posH.xy *= gScale;
    posH.z = posH.w;
}

[shader("fragment")]
float4 ps(float3 dir : NORMAL) : SV_TARGET {

return float4(0.0, 0.5, 1.0, 0.0);

#ifdef _SKYBOX_SOLID_MODE
    return float4(gIntensity, gTransparency);
#else
    #ifdef _SPHERICAL_MAP
        float2 uv = dirToSphericalCrd(dir);
        return gTexture.Sample(gSampler, uv) * float4(gIntensity.r, gIntensity.g, gIntensity.b, gTransparency);
    #else
        return gTexture.SampleLevel(gSampler, normalize(dir), 0) * float4(gIntensity.r, gIntensity.g, gIntensity.b, gTransparency);
    #endif
#endif
}
