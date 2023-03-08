/** Edge detection render pass.
*/

import Scene.Raster;
import Scene.Shading;

__exported import Scene.Scene;
__exported import Scene.Shading;
__exported import Scene.HitInfo;

import Experimental.Scene.Material.StandardMaterial;

import Scene.Material.ShadingUtils;

#define is_valid(name) (is_valid_##name != 0)

#ifndef _DEPTH_KERNEL_HALF_SIZE_U
#define _DEPTH_KERNEL_HALF_SIZE_U 1
#endif

#ifndef _DEPTH_KERNEL_HALF_SIZE_V
#define _DEPTH_KERNEL_HALF_SIZE_V 1
#endif

cbuffer PerFrameCB
{
    uint2 gResolution;
    uint2 gDepthKernelCenter = uint2(1, 1);
    uint2 gNormalKernelCenter = uint2(1, 1);

    float2 gDepthDistanceRange;   // min/max depth distance values
    float2 gNormalThresholdRange; // normal threshold min/max range
}

// Input edge detection data.
Texture2D<float>            gDepth;            // optional depth buffer
Texture2D<float3>           gNormal;           // optional normal buffer
Texture2D<PackedHitInfo>    gVBuffer;          // optional visibility buffer

// Temporary data.
RWTexture2D<float2>         gTmpDepth;
RWTexture2D<float2>         gTmpNormal;
RWTexture2D<float2>         gTmpMaterial;
RWTexture2D<float2>         gTmpInstance;

// Kernel textures
Texture2D<float>            gDepthKernelU;
Texture2D<float>            gDepthKernelV;

Texture2D<float>            gNormalKernelU;
Texture2D<float>            gNormalKernelV;

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

float getDepth(const uint2 ipos) {
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

// Get screen space normal
float3 getNormalSS(const uint2 ipos) {
    // Perfer VBuffer
    #if is_valid(gVBuffer)
        const HitInfo hit = HitInfo(gVBuffer[ipos]);
        if(hit.isValid()) {
            const float3 rayDir = normalize(gScene.camera.computeRayPinhole(ipos, gResolution).dir);
            ShadingData sd;
            if(calcShadingData(rayDir, hit, sd)) {
                return normalize(mul(float4(sd.N, 0.f), gScene.camera.getViewProj()).xyz);
            }
        }
        return float3(.0f, .0f, 1.f);
    #elif is_valid(gNormal)
        return gNormal[ipos];
    #else
        return float3(.0f, .0f, 1.f);
    #endif
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

    float3 N = getNormalSS(pixelPos); // current pixel screen space normal
    float nx = N.x * gNormalKernelU[uint2(gNormalKernelCenter.x, 0)];
    float ny = N.y * gNormalKernelU[uint2(gNormalKernelCenter.x, 0)];

    [[unroll]]
    for(uint i = 1; i <= _DEPTH_KERNEL_HALF_SIZE_U; i++) {
        #if defined(_TRACE_DEPTH) && (is_valid(gVBuffer) || is_valid(gDepth))
        // Horizontal part
        dx += getDepth(pixelPos + uint2(i, 0)) * gDepthKernelU[uint2(gDepthKernelCenter.x + i, 0)];
        dx += getDepth(pixelPos - uint2(i, 0)) * gDepthKernelU[uint2(gDepthKernelCenter.x - i, 0)];
        
        // Vertical part
        dy += getDepth(pixelPos + uint2(0, i)) * gDepthKernelU[uint2(gDepthKernelCenter.x + i, 0)];
        dy += getDepth(pixelPos - uint2(0, i)) * gDepthKernelU[uint2(gDepthKernelCenter.x - i, 0)];
        #endif
    
        #if defined(_TRACE_NORMAL) && (is_valid(gVBuffer) || is_valid(gNormal))
            // Horizontal part
            nx += getNormalSS(pixelPos + uint2(i, 0)).x * gNormalKernelU[uint2(gNormalKernelCenter.x + i, 0)];
            nx += getNormalSS(pixelPos - uint2(i, 0)).x * gNormalKernelU[uint2(gNormalKernelCenter.x - i, 0)];
    
            // Vertical part
            ny += getNormalSS(pixelPos + uint2(0, i)).y * gNormalKernelU[uint2(gNormalKernelCenter.x - i, 0)];
            ny += getNormalSS(pixelPos - uint2(0, i)).y * gNormalKernelU[uint2(gNormalKernelCenter.x + i, 0)];
        #endif
    }

    #if is_valid(gTmpDepth)
        gTmpDepth[pixelPos] = float2(dx, dy);
    #endif

    #if is_valid(gTmpNormal)
        gTmpNormal[pixelPos] = float2(nx, ny);
    #endif
}

/** V pass.
*/
[numthreads(16, 16, 1)]
void passV(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gResolution)) return;
    const uint2 pixelPos = dispatchThreadId.xy;
    
    #if is_valid(gTmpDepth)
    const float2 tmpDepth = gTmpDepth[pixelPos];
    #else
    const float2 tmpDepth = float2(0.f);
    #endif
    float dx = tmpDepth.x * gDepthKernelV[uint2(gDepthKernelCenter.y, 0)];
    float dy = tmpDepth.y * gDepthKernelV[uint2(gDepthKernelCenter.y, 0)];

    #if is_valid(gTmpNormal)
    const float2 tmpNormal = gTmpNormal[pixelPos];
    #else 
    const float2 tmpNormal = float2(0.f);
    #endif

    float nx = tmpNormal.x * gNormalKernelV[uint2(gNormalKernelCenter.y, 0)];
    float ny = tmpNormal.y * gNormalKernelV[uint2(gNormalKernelCenter.y, 0)];

    // Horizontal part
    [[unroll]]
    for(uint i = 1; i <= _DEPTH_KERNEL_HALF_SIZE_V; i++) {
        #if defined(_TRACE_DEPTH) && is_valid(gTmpDepth)
        dx += gTmpDepth[pixelPos + uint2(0, i)].r * gDepthKernelV[uint2(gDepthKernelCenter.y + i, 0)];
        dx += gTmpDepth[pixelPos - uint2(0, i)].r * gDepthKernelV[uint2(gDepthKernelCenter.y - i, 0)];
    
        dy += gTmpDepth[pixelPos + uint2(i, 0)].g * gDepthKernelV[uint2(gDepthKernelCenter.y + i, 0)];
        dy += gTmpDepth[pixelPos - uint2(i, 0)].g * gDepthKernelV[uint2(gDepthKernelCenter.y - i, 0)];
        #endif

        #if defined(_TRACE_NORMAL) && is_valid(gTmpNormal)
        nx += gTmpNormal[pixelPos + uint2(0, i)].r * gNormalKernelV[uint2(gNormalKernelCenter.y + i, 0)];
        nx += gTmpNormal[pixelPos - uint2(0, i)].r * gNormalKernelV[uint2(gNormalKernelCenter.y - i, 0)];
    
        ny += gTmpNormal[pixelPos + uint2(i, 0)].g * gNormalKernelV[uint2(gNormalKernelCenter.y + i, 0)];
        ny += gTmpNormal[pixelPos - uint2(i, 0)].g * gNormalKernelV[uint2(gNormalKernelCenter.y - i, 0)];
        #endif
    }

    // Compute Depth edge
    #if is_valid(gTmpDepth)
    const float depth_dist = sqrt(dx*dx + dy*dy);
    #else
    const float depth_dist = 0.f;
    #endif
    const float depth_dist_clamped = max(0.f, (depth_dist - gDepthDistanceRange.x) * (1.f / (gDepthDistanceRange.y - gDepthDistanceRange.x)));
    float depth_edge = min(1.f, depth_dist_clamped);


    // Compute Normal edge
    #if is_valid(gTmpNormal)
    const float normal_dist = sqrt(nx*nx + ny*ny);
    #else
    const float normal_dist = 0.f;
    #endif
    const float normal_dist_clamped = max(0.f, (normal_dist - gNormalThresholdRange.x) * (1.f / (gNormalThresholdRange.y - gNormalThresholdRange.x)));
    float normal_edge = min(1.f, normal_dist_clamped);
    
    // Combined edge
    const float edge_combined = max(depth_edge, normal_edge);

    // Write detected edge data
    gOutput[pixelPos] = float4(edge_combined, edge_combined, edge_combined, 1.f);
}
