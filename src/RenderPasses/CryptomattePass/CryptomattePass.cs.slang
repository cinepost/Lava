#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
import Scene.Shading;

import Utils.Math.MathHelpers;

#define is_valid(name) (is_valid_##name != 0)

#define MAX_CRYPTOMATTE_LAYERS 16

#ifndef _PREVIEW_COLOR_MODE
#define _PREVIEW_COLOR_MODE 0
#endif

#ifndef _DATA_LAYERS_COUNT
#define _DATA_LAYERS_COUNT 0
#endif

cbuffer PerFrameCB {
  uint2             gFrameDim;
};


// Input channel
Texture2D<PackedHitInfo> gVbuffer;
Buffer<float32_t3> gPreviewHashColorBuffer;

Buffer<float32_t> gMaterialHashBuffer;
Buffer<float32_t> gInstanceHashBuffer;
Buffer<float32_t> gCustattrHashBuffer;

// Preview color output channel
RasterizerOrderedTexture2D<float4> gPreviewLayer;

// Data layers
#if _DATA_LAYERS_COUNT > 0
RasterizerOrderedTexture2D<float4> gDataLayer00;
#endif

#if _DATA_LAYERS_COUNT > 1
RasterizerOrderedTexture2D<float4> gDataLayer01;
#endif

#if _DATA_LAYERS_COUNT > 2
RasterizerOrderedTexture2D<float4> gDataLayer02;
#endif

#if _DATA_LAYERS_COUNT > 3
RasterizerOrderedTexture2D<float4> gDataLayer03;
#endif

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 ipos = dispatchThreadId.xy;

    // early out
    if (any(ipos >= gFrameDim)) return;

    const HitInfo hit = HitInfo(gVbuffer[ipos]);
    
    [[branch]] if (!hit.isValid()) {
#if is_valid(gPreviewLayer)
        gPreviewLayer[ipos] = float4(.0f);
#endif  // gPreviewLayer
        return;
    }

    uint materialID = 0;

    if (hit.getType() == HitType::Triangle) {
        TriangleHit triangleHit = hit.getTriangleHit();
        materialID = gScene.getMaterialID(triangleHit.instanceID);
    }

    // Write available false color AOVs
#if is_valid(gPreviewLayer) && is_valid(gPreviewHashColorBuffer)
    gPreviewLayer[ipos] = float4(gPreviewHashColorBuffer[materialID], 1.0f);    
#endif  // gMatColor

}
