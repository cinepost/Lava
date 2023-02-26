/** Spatial jiteered quad combining render pass.
*/

cbuffer PerFrameCB
{
    uint2   gOutputResolution;
    uint2   gQuadResolution;
}


// Input data to combine.
Texture2D<float4>   gVBuff1;
Texture2D<float4>   gVBuff2;
Texture2D<float4>   gVBuff3;
Texture2D<float4>   gVBuff4;

Texture2D<float>    gDepth1;
Texture2D<float>    gDepth2;
Texture2D<float>    gDepth3;
Texture2D<float>    gDepth4;

// Combined data
RWTexture2D<float4> gOutputVBuff;
RWTexture2D<float>  gOutputDepth;

SamplerState gSampler;


[numthreads(16, 16, 1)]
void combine(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gQuadResolution)) return;
    
    const uint2 srcPixelPos = dispatchThreadId.xy;    
    const uint2 dstPixelPos1 = srcPixelPos * uint2(2);
    const uint2 dstPixelPos2 = dstPixelPos1 + uint2(1, 0);
    const uint2 dstPixelPos3 = dstPixelPos1 + uint2(0, 1);
    const uint2 dstPixelPos4 = dstPixelPos1 + uint2(1, 1);

    gOutputVBuff[dstPixelPos1] = gVBuff1[srcPixelPos];
    gOutputVBuff[dstPixelPos2] = gVBuff2[srcPixelPos];
    gOutputVBuff[dstPixelPos3] = gVBuff3[srcPixelPos];
    gOutputVBuff[dstPixelPos4] = gVBuff4[srcPixelPos];

    gOutputDepth[dstPixelPos1] = gDepth1[srcPixelPos];
    gOutputDepth[dstPixelPos2] = gDepth2[srcPixelPos];
    gOutputDepth[dstPixelPos3] = gDepth3[srcPixelPos];
    gOutputDepth[dstPixelPos4] = gDepth4[srcPixelPos];
}
