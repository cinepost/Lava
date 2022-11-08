#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
import Scene.Shading;

import Utils.Math.MathHelpers;

#define is_valid(name) (is_valid_##name != 0)

// Input channel
Texture2D<PackedHitInfo> gVbuffer;

// Extra UAV output channels
RasterizerOrderedTexture2D<float4> gMatColor;
RasterizerOrderedTexture2D<float4> gObjColor;
RasterizerOrderedTexture2D<float4> gAttribColor;


cbuffer PerFrameCB {
  uint2             gFrameDim;
};


[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 ipos = dispatchThreadId.xy;

    // early out
    if (any(ipos >= gFrameDim)) return;

    const HitInfo hit = HitInfo(gVbuffer[ipos]);
    
    if (!hit.isValid()) {
#if is_valid(gMatColor)
        gMatColor[ipos] = float4(1.0, 0.0, 1.0, 1.0f);
#endif  // gMatColor

#if is_valid(gObjColor)
        gObjColor[ipos] = float4(.0f);
#endif  // gObjColor

#if is_valid(gAttribColor)
        gAttribColor[ipos] = float4(.0f);
#endif  // gAttribColor

        return;
    }

    // Write available false color AOVs
#if is_valid(gMatColor)
        gMatColor[ipos] = float4(1.0, 0.0, 0.0, 1.0f);
#endif  // gMatColor

#if is_valid(gObjColor)
        gObjColor[ipos] = float4(0.0, 1.0, 0.0, 1.0f);
#endif  // gObjColor

#if is_valid(gAttribColor)
        gAttribColor[ipos] = float4(0.0, 0.0, 1.0, 1.0f);
#endif  // gAttribColor

}
