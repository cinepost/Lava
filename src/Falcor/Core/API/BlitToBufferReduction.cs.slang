/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
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
#include "BlitToBufferReduction.slangh"

cbuffer BlitParamsCB {
    uint2  gResolution;
    float2 gOffset;
    float2 gScale;
    float4 gCompTransformR;
    float4 gCompTransformG;
    float4 gCompTransformB;
    float4 gCompTransformA;
}

#if SAMPLE_COUNT <= 1
Texture2D<float4> gTex;
#else
Texture2DMS<float4> gTex;
#endif

SamplerState gSampler;
SamplerState gSamplerR;
SamplerState gSamplerG;
SamplerState gSamplerB;
SamplerState gSamplerA;

//RWBuffer<OutDataType> gOutputBuffer;
RWByteAddressBuffer gOutputBuffer;


[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    if (any(dispatchThreadId.xy >= gResolution)) return;
    
    const uint2 pixelPos = dispatchThreadId.xy;
    const uint  buffPos  = gResolution.x * pixelPos.y + pixelPos.x;
    
    const uint addr = buffPos * PIXEL_STRIDE_BYTES;

    float2 texC = float2(pixelPos) / float2(gResolution);
    float4 src;

#if COMPLEX_BLIT
    float4 srcR = gTex.Sample(gSamplerR, texC);
    float4 srcG = gTex.Sample(gSamplerG, texC);
    float4 srcB = gTex.Sample(gSamplerB, texC);
    float4 srcA = gTex.Sample(gSamplerA, texC);

    src.r = dot(gCompTransformR, srcR);
    src.g = dot(gCompTransformG, srcG);
    src.b = dot(gCompTransformB, srcB);
    src.a = dot(gCompTransformA, srcA);
#else
    src = gTex.Sample(gSampler, texC);
#endif

#if FORMAT_TYPE == FORMAT_TYPE_FLOAT

    #if DST_HALF_FLOAT
        gOutputBuffer.Store<float16_t3>(addr, { float16_t(src.r), float16_t(src.g), float16_t(src.b) });
    #else
        gOutputBuffer.Store<float32_t3>(addr, { float32_t(src.r), float32_t(src.g), float32_t(src.b) });
    #endif

#elif FORMAT_TYPE == FORMAT_TYPE_SINT

//    gOutputBuffer.Store(addr, value);

#elif FORMAT_TYPE == FORMAT_TYPE_UINT

//    gOutputBuffer.Store(addr, value);

#else 

//    gOutputBuffer.Store(addr, value);

#endif
    return;
/*

#if SAMPLE_COUNT <= 1

#if COMPLEX_BLIT
#if SRC_INT
    #error Complex blit not supported for integer source format
#else
    float4 srcR = gTex.Sample(gSamplerR, texC);
    float4 srcG = gTex.Sample(gSamplerG, texC);
    float4 srcB = gTex.Sample(gSamplerB, texC);
    float4 srcA = gTex.Sample(gSamplerA, texC);

    float4 res;
    res.r = dot(gCompTransformR, srcR);
    res.g = dot(gCompTransformG, srcG);
    res.b = dot(gCompTransformB, srcB);
    res.a = dot(gCompTransformA, srcA);

    gOutputBuffer[buffPos] = res;
#endif
#else // !COMPLEX_BLIT
#if SRC_INT
    uint4 res = asuint(gTex.Sample(gSampler, texC));
#else
    float4 res = gTex.Sample(gSampler, texC);
#endif
    gOutputBuffer[buffPos] = res;
#endif

#else // SAMPLE_COUNT > 1

#if COMPLEX_BLIT
    #error Complex blit not supported for sample count > 1
#else // !COMPLEX_BLIT
#if SRC_INT
    #error Blit for sample count > 1 not supported for integer source format
#else
    uint3 dims;
    gTex.GetDimensions(dims.x, dims.y, dims.z);
    uint2 crd = (uint2)(float2(dims.xy) * texC);
    float4 c = float4(0, 0, 0, 0);

    [unroll]
    for (uint i = 0; i < SAMPLE_COUNT; i++)
    {
        c += gTex.Load(crd, i);
    }

    c /= SAMPLE_COUNT;
    gOutputBuffer[buffPos] = c;
#endif
#endif

#endif

*/
}
