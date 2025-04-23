#define is_valid(name) (is_valid_##name != 0)

// Main output channel
RWTexture2D<float4> gInOutColor;

// Extra input channels
Texture2D<PackedHitInfo>    gVBuffer;
Texture2D<float>            gDepth;


cbuffer PerFrameCB {
    uint2 gFrameDim;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {

    uint2 pixelPos = dispatchThreadId.xy;

    // early out
    if (any(pixelPos >= gFrameDim)) return;
}
