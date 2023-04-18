#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
import Scene.Shading;

import Utils.Math.MathHelpers;

//import Experimental.Scene.Materials.Fresnel;
//import Experimental.Scene.Materials.Microfacet;
//import Experimental.Scene.Materials.StandardMaterial;

#define is_valid(name) (is_valid_##name != 0)

// VBuffer
Texture2D<PackedHitInfo> gVBuffer;

// Extra input channels
Texture2D<float> gDepth;
Texture2D<float4> gTextureGrads;
Texture2D<float2> gMotionVector;

// Main output channel
RWTexture2D<float4> gInOutColor;

cbuffer PerFrameCB {
  uint2             gFrameDim;
};

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 ipos = dispatchThreadId.xy;

    // early out
    if (any(ipos >= gFrameDim)) return;

    const HitInfo hit = HitInfo(gVBuffer[ipos]);
    
    if (hit.isValid()) {
        TriangleHit triangleHit = hit.getTriangleHit();
        const float3 barycentrics = triangleHit.getBarycentricWeights();
        gInOutColor[ipos] = float4(barycentrics, 1.f);
    }
}
