#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
import Scene.Shading;

import Utils.Math.MathHelpers;

//import Experimental.Scene.Materials.Fresnel;
//import Experimental.Scene.Materials.Microfacet;
//import Experimental.Scene.Materials.StandardMaterial;

#ifndef FALSE_COLOR_BUFFER_SIZE
#define FALSE_COLOR_BUFFER_SIZE 0
#endif

#define is_valid(name) (is_valid_##name != 0)

// VBuffer
Texture2D<PackedHitInfo> gVBuffer;

// Extra input channels
Texture2D<uint> gMeshletID;
Texture2D<float> gDepth;
Texture2D<float4> gTextureGrads;
Texture2D<float2> gMotionVector;

// Extra input buffers
Buffer<float4> gFalseColorBuffer;

// Main output channel
RWTexture2D<float4> gInOutColor;

// Extra output channels
RasterizerOrderedTexture2D<float>  gPrimID;
RasterizerOrderedTexture2D<float>  gOpID;
RasterizerOrderedTexture2D<float4> gMeshletColor;

cbuffer PerFrameCB {
  uint2             gFrameDim;
};

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 pixelPos = dispatchThreadId.xy;

    // early out
    if (any(pixelPos >= gFrameDim)) return;

    const HitInfo hit = HitInfo(gVBuffer[pixelPos]);
    
    if (!hit.isValid()) return;

    TriangleHit triangleHit = hit.getTriangleHit();
    const float3 barycentrics = triangleHit.getBarycentricWeights();
    gInOutColor[pixelPos] = float4(barycentrics, 1.f);

#if is_valid(gPrimID)
    gPrimID[pixelPos] = triangleHit.primitiveIndex;
#endif  // gOutFresnel

#if is_valid(gOpID)
    gOpID[pixelPos] = triangleHit.instanceID.index;
#endif  // gOpID

}
