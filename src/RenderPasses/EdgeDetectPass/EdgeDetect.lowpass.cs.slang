/** Edge detection render pass.
*/
#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
import Scene.Shading;

__exported import Scene.Scene;
__exported import Scene.Shading;
__exported import Scene.HitInfo;

import Experimental.Scene.Materials.StandardMaterial;

import Scene.Material.ShadingUtils;

#ifndef _FILTER_SIZE
#define _FILTER_SIZE 0
#endif

#define is_valid(name) (is_valid_##name != 0)

cbuffer PerFrameCB
{
    uint2 gResolution;
}

// Input
Texture2D<PackedHitInfo>   gVBuffer;

// Ouput
RWTexture2D<PackedHitInfo> gOutputVBuffer;

/** Low pass filter.
*/
[numthreads(16, 16, 1)]
void lowPass(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gResolution)) return;
    const uint2 pixelPos = dispatchThreadId.xy;

    //#if is_valid(gVBuffer)
    //    [[branch ]] if(!HitInfo(gVBuffer[pixelPos]).isValid()) {
    //        gOutput[pixelPos] = float4(0.f);
    //        return;
    //    }
    //#endif

    gOutputVBuffer[pixelPos] = gVBuffer[pixelPos];
}
