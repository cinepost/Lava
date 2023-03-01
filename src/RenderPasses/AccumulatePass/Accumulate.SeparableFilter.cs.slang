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

/** Sample accumulation render pass.
*/

#include "Accumulate.SeparableFilter.slangh"

import Utils.Sampling.SampleGenerator;

//#define DO_EXPAND(VAL)  VAL ## 1
//#define EXPAND(VAL)     DO_EXPAND(VAL)

#define is_valid(name) (is_valid_##name != 0)

#ifndef _FILTER_HALF_SIZE_H
#define _FILTER_HALF_SIZE_H 0
#endif

#ifndef _FILTER_HALF_SIZE_V
#define _FILTER_HALF_SIZE_V 0
#endif

cbuffer PerFrameCB
{
    uint2   gResolution;
    uint2   gFilterSize;            // Filter size in pixels
    uint2   gKernelTextureSize;     // Kernel texture size in texels
    float2  gSampleOffsetUniform;   // Uniform sample offset from pixel center (rasterizer input only)
    float   gSampleDistanceUniform; // Uniform sample distance from the pixel center (rasterizer input only)
    float2  gMaxSampleOffset;       // Maximum sample offset from cetral pixel center
    uint    gSampleNumber;
}

// Input data to accumulate and accumulated output.
Texture2D<float4>   gInput;

// Optional inputs
Texture2D<float>    gDepth;           // optional depth texture (used for CLOSEST ans FARTHEST filters)
Texture2D<float>    gKernelTexture;   // optional kernel texture
Texture2D<float2>   gSampleOffsets;   // optional per-pixel sample offsets

// Ouput sample plane
RWTexture2D<float4> gOutputFilteredImage;
RWTexture2D<float>  gOutputFilteredDepth;

SamplerState gImageSampler;
SamplerState gKernelSampler;


/** Horizontal filtering pass.
*/
[numthreads(16, 16, 1)]
void filterH(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gResolution)) return;

    const uint2 pixelPos = dispatchThreadId.xy;
    float2 currentSampleOffset = float2(0.f);
    float W = 1.f; // total samples weight

    #if is_valid(gSampleOffsets)
    float2 sampleOffset = gSampleOffsetUniform;
    #else
    float2 sampleOffset = gSampleOffsets[pixelPos];
    #endif

    // Central sample
    float4 outColor = gInput[pixelPos];
    #if (_FILTER_TYPE == SF_TYPE_GAUSSIAN) || (_FILTER_TYPE == SF_TYPE_MITCHELL) || (_FILTER_TYPE == SF_TYPE_SINC)
        #if is_valid(gKernelTexture)
            const float dist = abs(currentSampleOffset.x / gMaxSampleOffset.x);
            W = gKernelTexture.SampleLevel(gKernelSampler, float2(dist, 0.5f), 0);
            outColor *= W;
        #endif
    #elif (_FILTER_TYPE == SF_TYPE_TRIANGLE)
            W = 1.f - abs(currentSampleOffset.x / gMaxSampleOffset.x);
            outColor *= W;
    #endif

    //currentSampleOffset.x = abs(sampleOffset.x);
    //currentSampleOffset.y = abs(sampleOffset.y);
    //if(any(currentSampleOffset >= gMaxSampleOffset)) outColor = float4(0.f);

    #if _FILTER_HALF_SIZE_H != 0
    const uint2 pixelPosF = float2(pixelPos);
    const int2 pixelPosI = int2(pixelPos);

    int2 pixelPosR;

    // Kernel testing part
    #if is_valid(gKernelTexture)
    //outColor = gKernelTexture.SampleLevel(gKernelSampler, float2(pixelPos) * 0.015625f, 0); // float4(0.f, 1.f, 0.f, 1.f);
    #endif

    // First half
    [[unroll]]
    for(uint i = 1; i <= _FILTER_HALF_SIZE_H; i++) {
        pixelPosR = pixelPos + int2(i, 0);
        
        if((pixelPosR.x < 0) || (pixelPosR.x >= gResolution.x)) continue; // sample outsize viewport

        currentSampleOffset.x = abs(float(i) - sampleOffset.x);
        if(currentSampleOffset.x > gMaxSampleOffset.x) continue; // sample outside max offset

        #if (_FILTER_TYPE == SF_TYPE_TRIANGLE)
        
            const float w = 1.f - currentSampleOffset.x / gMaxSampleOffset.x;
            outColor += gInput[pixelPosR] * w;
            W += w;
        
        #elif (_FILTER_TYPE == SF_TYPE_GAUSSIAN) || (_FILTER_TYPE == SF_TYPE_MITCHELL) || (_FILTER_TYPE == SF_TYPE_SINC)
            
            #if is_valid(gKernelTexture)
            const float nd = currentSampleOffset.x / gMaxSampleOffset.x;
            const float w = gKernelTexture.SampleLevel(gKernelSampler, float2(nd, .5f), 0);
            outColor += gInput[pixelPosR] * w;
            W += w;
            #endif
        
        #else // Box filter
        
            outColor += gInput[pixelPosR];
            W += 1.f;
        
        #endif
    }

    // Second half
    [[unroll]]
    for(uint i = 1; i <= _FILTER_HALF_SIZE_H; i++) {
        pixelPosR = pixelPos - int2(i, 0);
        
        if((pixelPosR.x < 0) || (pixelPosR.x >= gResolution.x)) continue; // sample outside viewport

        currentSampleOffset.x = abs(float(i) + sampleOffset.x);
        if(currentSampleOffset.x > gMaxSampleOffset.x) continue; // sample outside max offset

        #if (_FILTER_TYPE == SF_TYPE_TRIANGLE)
        
            const float w = 1.f - currentSampleOffset.x / gMaxSampleOffset.x;
            outColor += gInput[pixelPosR] * w;
            W += w;
        
        #elif (_FILTER_TYPE == SF_TYPE_GAUSSIAN) || (_FILTER_TYPE == SF_TYPE_MITCHELL) || (_FILTER_TYPE == SF_TYPE_SINC)

            #if is_valid(gKernelTexture)
            const float nd = currentSampleOffset.x / gMaxSampleOffset.x;
            const float w = gKernelTexture.SampleLevel(gKernelSampler, float2(nd, .5f), 0);
            outColor += gInput[pixelPosR] * w;
            W += w;
            #endif

        #elif (_FILTER_TYPE == SF_TYPE_SINC)

        #else // Box filter
        
            outColor += gInput[pixelPosR];
            W += 1.f;
        
        #endif
    }

    outColor /= W;
    #endif

    // Write filtered data
    gOutputFilteredImage[pixelPos] = outColor;
}

/** Vertical filtering pass.
*/
[numthreads(16, 16, 1)]
void filterV(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gResolution)) return;
    const uint2 pixelPos = dispatchThreadId.xy;
    float2 currentSampleOffset = float2(0.f);
    float W = 1.f; // total samples weight

    #if is_valid(gSampleOffsets)
    float2 sampleOffset = gSampleOffsetUniform;
    #else
    float2 sampleOffset = gSampleOffsets[pixelPos];
    #endif

    // Central sample
    float4 outColor = gInput[pixelPos];
    #if (_FILTER_TYPE == SF_TYPE_GAUSSIAN) || (_FILTER_TYPE == SF_TYPE_MITCHELL) || (_FILTER_TYPE == SF_TYPE_SINC)
        #if is_valid(gKernelTexture)
            const float dist = abs(currentSampleOffset.y / gMaxSampleOffset.y);
            W = gKernelTexture.SampleLevel(gKernelSampler, float2(dist, 0.5f), 0);
            outColor *= W;
        #endif
    #elif (_FILTER_TYPE == SF_TYPE_TRIANGLE)
            W = 1.f - abs(currentSampleOffset.y / gMaxSampleOffset.y);
            outColor *= W;
    #endif

    #if _FILTER_HALF_SIZE_V != 0
    const uint2 pixelPosF = float2(pixelPos);
    const int2 pixelPosI = int2(pixelPos);

    int2 pixelPosR;

    // First half
    [[unroll]]
    for(uint i = 1; i <= _FILTER_HALF_SIZE_V; i++) {
        pixelPosR = pixelPos + int2(0, i);
        
        if((pixelPosR.y < 0) || (pixelPosR.y >= gResolution.y)) continue; // sample outside viewport
        
        currentSampleOffset.y = abs(float(i) + sampleOffset.y);
        if(currentSampleOffset.y > gMaxSampleOffset.y) continue; // sample outside max offset

        #if (_FILTER_TYPE == SF_TYPE_TRIANGLE)
        
            const float w = 1.f - currentSampleOffset.y / gMaxSampleOffset.y;
            outColor += gInput[pixelPosR] * w;
            W += w;
        
        #elif (_FILTER_TYPE == SF_TYPE_GAUSSIAN) || (_FILTER_TYPE == SF_TYPE_MITCHELL) || (_FILTER_TYPE == SF_TYPE_SINC)

            #if is_valid(gKernelTexture)
            const float nd = currentSampleOffset.y / gMaxSampleOffset.y;
            const float w = gKernelTexture.SampleLevel(gKernelSampler, float2(nd, .5f), 0);
            outColor += gInput[pixelPosR] * w;
            W += w;
            #endif

        #elif (_FILTER_TYPE == SF_TYPE_SINC)

        #else // Box filter
        
            outColor += gInput[pixelPosR];
            W += 1.f;
        
        #endif
    }

    // Second half
    [[unroll]]
    for(uint i = 1; i <= _FILTER_HALF_SIZE_V; i++) {
        pixelPosR = pixelPos - int2(0, i);
        
        if((pixelPosR.y < 0) || (pixelPosR.y >= gResolution.y)) continue; // sample outside viewport
        
        currentSampleOffset.y = abs(float(i) - sampleOffset.y);
        if(currentSampleOffset.y > gMaxSampleOffset.y) continue; // sample outside max offset

        #if (_FILTER_TYPE == SF_TYPE_TRIANGLE)
        
            const float w = 1.f - currentSampleOffset.y / gMaxSampleOffset.y;
            outColor += gInput[pixelPosR] * w;
            W += w;
        
        #elif (_FILTER_TYPE == SF_TYPE_GAUSSIAN) || (_FILTER_TYPE == SF_TYPE_MITCHELL) || (_FILTER_TYPE == SF_TYPE_SINC)
            
            #if is_valid(gKernelTexture)
            const float nd = currentSampleOffset.y / gMaxSampleOffset.y;
            const float w = gKernelTexture.SampleLevel(gKernelSampler, float2(nd, .5f), 0);
            outColor += gInput[pixelPosR] * w;
            W += w;
            #endif

        #elif (_FILTER_TYPE == SF_TYPE_SINC)

        #else // Box filter
        
            outColor += gInput[pixelPosR];
            W += 1.f;
        
        #endif
    }

    outColor /= W;
    #endif

    // Write filtered data
    gOutputFilteredImage[pixelPos] = outColor;
}

/** Ground truth filtering pass (for quality/speed tests).
*/
[numthreads(16, 16, 1)]
void filterG(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gResolution)) return;
    const uint2 pixelPos = dispatchThreadId.xy;
    
    // Central sample
    float4 outColor = gInput[pixelPos];

    // Write filtered data
    gOutputFilteredImage[pixelPos] = outColor;
}