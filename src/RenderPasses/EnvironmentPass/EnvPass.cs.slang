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

import Scene.Scene;
import Utils.Math.MathHelpers;
import Experimental.Scene.Lights.LightHelpers;

__exported import Scene.Lights.LightData;
__exported import Scene.Camera.Camera;

#define is_valid(name) (is_valid_##name != 0)

cbuffer PerFrameCB {
    uint2 gFrameDim;
    float3 gIntensity;
    float gScale;
    float gOpacity;
    uint lightsCount;
    uint2 gBackTextureDim;
    Camera gCamera;
};

Buffer<uint32_t> gLightIDs;

Texture2D<float4> gBackdropTexture;
SamplerState gSampler;

// Main output channel
RWTexture2D<float4> gOutColor;

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 ipos = dispatchThreadId.xy;

    // early out
    if (any(ipos >= gFrameDim)) return;

    float4 backColor = gCamera.getBackgroundColor();

#if is_valid(gBackdropTexture)

    float2 texelOffset = float2(.5f) / (float2)gBackTextureDim;
    float2 uv = float2(ipos) / ((float2)gFrameDim + float2(1.0, 1.0));
    uv += texelOffset;

    float2 gradX = float2(1.0f / (float)gFrameDim.x, 0.0f);
    float2 gradY = float2(0.0f, 1.0f / (float)gFrameDim.y);

    backColor += gBackdropTexture.SampleGrad(gSampler, uv, gradX, gradY);
#else

    const float3 rayDir = normalize(gCamera.computeRayPinhole(ipos, gFrameDim).dir);

    float  localMaxDistance = FLT_MAX;
    float3 cameraPosW = gCamera.getPosition();

    for(uint i = 0; i < lightsCount; ++i) {
        const LightData light = gScene.getLight(gLightIDs[i]);
        if(!light.visibleToCamera()) continue;

        AnalyticLightSample ls = {};
        if(intersectLight(cameraPosW, rayDir, light, ls)) {
            backColor.rgb += ls.Li * light.directDiffuseIntensity;
            backColor.a = 1.0f;
            // && (ls.distance < localMaxDistance)
            localMaxDistance = ls.distance;
        }
    }
#endif

    gOutColor[ipos] = backColor * float4(gIntensity, 1.0f) * clamp(gOpacity, 0.0f, 1.0f);

}
