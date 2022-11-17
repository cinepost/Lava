#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
import Scene.Shading;

import Utils.Math.MathHelpers;

#define is_valid(name) (is_valid_##name != 0)

#define MAX_CRYPTOMATTE_LAYERS 16

// Input channel
Texture2D<PackedHitInfo> gVbuffer;

// Extra UAV output channels
RasterizerOrderedTexture2D<float4> gMatColor;
RasterizerOrderedTexture2D<float4> gObjColor;
RasterizerOrderedTexture2D<float4> gAttribColor;


cbuffer PerFrameCB {
  uint2             gFrameDim;
};

#if _OUTPUT_MATERIALS
Buffer<float32_t> gMaterialHashBuffer;
#endif 

#if _OUTPUT_INSTANCES
Buffer<float32_t> gInstanceHashBuffer;
#endif 

#if _OUTPUT_CUSTATTRS
Buffer<float32_t> gCustattrHashBuffer;
#endif

#if is_valid(gMatColor)
Buffer<float32_t3> gMaterialPreviewColorBuffer;
#endif


[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 ipos = dispatchThreadId.xy;

    // early out
    if (any(ipos >= gFrameDim)) return;

    const HitInfo hit = HitInfo(gVbuffer[ipos]);
    
    if (!hit.isValid()) {
#if is_valid(gMatColor)
        gMatColor[ipos] = float4(.0f);
#endif  // gMatColor

#if is_valid(gObjColor)
        gObjColor[ipos] = float4(.0f);
#endif  // gObjColor

#if is_valid(gAttribColor)
        gAttribColor[ipos] = float4(.0f);
#endif  // gAttribColor

        return;
    }

    uint materialID = 0;

    if (hit.getType() == HitType::Triangle) {
        TriangleHit triangleHit = hit.getTriangleHit();
        materialID = gScene.getMaterialID(triangleHit.instanceID);
    }

    // Write available false color AOVs
#if is_valid(gMatColor)
#if _OUTPUT_MATERIALS
    gMatColor[ipos] = float4(gMaterialPreviewColorBuffer[materialID], 1.0f);
#else
    gMatColor[ipos] = float4(0.0f);
#endif
#endif  // gMatColor

#if is_valid(gObjColor)
        gObjColor[ipos] = float4(0.0, 1.0, 0.0, 1.0f);
#endif  // gObjColor

#if is_valid(gAttribColor)
        gAttribColor[ipos] = float4(0.0, 0.0, 1.0, 1.0f);
#endif  // gAttribColor

}
