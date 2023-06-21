#include "Utils/Math/MathConstants.slangh"
import VBufferSW;

ConstantBuffer<VBufferSW> gVBufferSW;

[numthreads(16, 16, 1)]
void reconstruct(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 pixelPos = dispatchThreadId.xy;

    // early out
    if (any(pixelPos >= gVBufferSW.frameDim)) return;

    const uint idx = gVBufferSW.frameDim.x * pixelPos.y + pixelPos.x;
    const uint2 depth_prim = gLocalDepthPrimBuffer.Load<uint2>(idx << 3);

    const uint uidepth = depth_prim.y;
    
    if( uidepth != 0) {
        const uint2 depth_parm = gLocalDepthParmBuffer.Load<uint2>(idx << 3);
        const uint2 instanceIndex = gLocalDepthInstBuffer.Load<uint2>(idx << 3);

        TriangleHit triangleHit = {};
        triangleHit.instanceID.index = instanceIndex.x;
        triangleHit.primitiveIndex = depth_prim.x;
        triangleHit.barycentrics = float2(asfloat16((uint16_t)(depth_parm.x >> 16)), asfloat16((uint16_t)(depth_parm.x & 0x0000FFFF)));   
        gVBufferSW.writeHit(pixelPos, HitInfo(triangleHit.pack()));
    }
}
