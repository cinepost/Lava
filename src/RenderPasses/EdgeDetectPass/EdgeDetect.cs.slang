/** Edge detection render pass.
*/

import Scene.Raster;

__exported import Scene.Scene;
//__exported import Scene.Shading;
__exported import Scene.HitInfo;


#define is_valid(name) (is_valid_##name != 0)

#ifndef DEPTH_KERNEL_HALF_SIZE_U
#define DEPTH_KERNEL_HALF_SIZE_U 1
#endif

#ifndef DEPTH_KERNEL_HALF_SIZE_V
#define DEPTH_KERNEL_HALF_SIZE_V 1
#endif

cbuffer PerFrameCB
{
    uint2 gResolution;
    uint2 gDepthKernelCenter = uint2(1, 1);

    float2 gDepthDistanceRange;  // min/max depth distance values
}

// Input edge detection data.
Texture2D<float>            gDepth;            // optional depth buffer
Texture2D<float3>           gNormal;           // optional normal buffer
Texture2D<PackedHitInfo>    gVBuffer;          // optional visibility buffer

// Temporary data.
RWTexture2D<float2>         gTmpDepth;

// Kernel textures
Texture2D<float>            gDepthKernelU;
Texture2D<float>            gDepthKernelV;

// Ouput
RWTexture2D<float4> gOutput;

bool calcShadingData(const float3 rayDir, const HitInfo hit, out ShadingData sd) {
    if (hit.isValid() && hit.getType() == HitType::Triangle) {
        sd = {};
        ITextureSampler lod = ExplicitLodTextureSampler(0.f);
        TriangleHit triangleHit = hit.getTriangleHit();

        // Evaluate Falcor's material parameters at the hit point.
        // TODO: Implement texLOD to enable texture filtering in prepareShadingData().
        const VertexData v = gScene.getVertexData(triangleHit);
        const uint materialID = gScene.getMaterialID(triangleHit.instanceID);
        sd = gScene.materials.prepareShadingData(v, materialID, -rayDir, lod);
        return true;
    }

    return false;
}

float getDepthRaw(uint2 ipos) {
    #if is_valid(gDepth)
        return gDepth[ipos];
    #else
        return 0.f;
    #endif
}

float getDepth(uint2 ipos) {
    float depth = 0.f;
    // Perfer VBuffer
    #if is_valid(gVBuffer)
        const HitInfo hit = HitInfo(gVBuffer[ipos]);
        if(hit.isValid()) {
            const float3 rayDir = normalize(gScene.camera.computeRayPinhole(ipos, gResolution).dir);
            ShadingData sd;
            if(!calcShadingData(rayDir, hit, sd)) return depth;

            const float4 screenP = mul(float4(sd.posW, 1.f), gScene.camera.getViewProj());
            const float4 viewP = mul(screenP, gScene.camera.data.invProjMat);
            depth = -(viewP.z / viewP.w);
        }
    #elif is_valid(gDepth)
        const float2 _screenUV = (float2(ipos) / float2(gResolution)) * 2.0 - float2(1.f);
        const float4 screenP = float4(_screenUV.x, _screenUV.y, gDepth[ipos], 1.0);
        const float4 viewP = mul(screenP, gScene.camera.data.invProjMat);
        depth = -(viewP.z / viewP.w);
    #endif
    
    return depth;
}


/** U pass.
*/
[numthreads(16, 16, 1)]
void passU(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gResolution)) return;
    const uint2 pixelPos = dispatchThreadId.xy;
    const float2 screenUV = float2(pixelPos) / float2(gResolution);

    float dx = getDepth(pixelPos) * gDepthKernelU[uint2(gDepthKernelCenter.x, 0)];
    float dy = dx;

    [[unroll]]
    for(uint i = 1; i <= DEPTH_KERNEL_HALF_SIZE_U; i++) {
        // Horizontal part
        dx += getDepth(pixelPos + uint2(i, 0)) * gDepthKernelU[uint2(gDepthKernelCenter.x + i, 0)];
        dx += getDepth(pixelPos - uint2(i, 0)) * gDepthKernelU[uint2(gDepthKernelCenter.x - i, 0)];
        
        // Vertical part
        dy += getDepth(pixelPos + uint2(0, i)) * gDepthKernelU[uint2(gDepthKernelCenter.x + i, 0)];
        dy += getDepth(pixelPos - uint2(0, i)) * gDepthKernelU[uint2(gDepthKernelCenter.x - i, 0)];
    }

    gTmpDepth[pixelPos] = float2(dx, dy);
}

/** V pass.
*/
[numthreads(16, 16, 1)]
void passV(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gResolution)) return;
    const uint2 pixelPos = dispatchThreadId.xy;
    
    const float2 tmpDepth = gTmpDepth[pixelPos];
    float dx = tmpDepth.x * gDepthKernelV[uint2(gDepthKernelCenter.y, 0)];
    float dy = tmpDepth.y * gDepthKernelV[uint2(gDepthKernelCenter.y, 0)];

    // Horizontal part
    [[unroll]]
    for(uint i = 1; i <= DEPTH_KERNEL_HALF_SIZE_V; i++) {
        dx += gTmpDepth[pixelPos + uint2(0, i)].r * gDepthKernelV[uint2(gDepthKernelCenter.y + i, 0)];
        dx += gTmpDepth[pixelPos - uint2(0, i)].r * gDepthKernelV[uint2(gDepthKernelCenter.y - i, 0)];
    
        dy += gTmpDepth[pixelPos + uint2(i, 0)].g * gDepthKernelV[uint2(gDepthKernelCenter.y + i, 0)];
        dy += gTmpDepth[pixelPos - uint2(i, 0)].g * gDepthKernelV[uint2(gDepthKernelCenter.y - i, 0)];
    }

    // Compute depth edge
    const float depth_dist = sqrt(dx*dx + dy*dy);
    const float depth_dist_clamped = max(0.f, (depth_dist - gDepthDistanceRange.x) * (1.f / (gDepthDistanceRange.y - gDepthDistanceRange.x)));
    float depth_edge = min(1.f, depth_dist_clamped);


    // Write detected edge data
    gOutput[pixelPos] = float4(depth_edge, depth_edge, depth_edge, 1.f);
}
