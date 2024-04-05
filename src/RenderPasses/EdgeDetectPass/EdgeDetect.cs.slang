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

#ifndef _DEPTH_KERNEL_HALF_SIZE
#define _DEPTH_KERNEL_HALF_SIZE 1
#endif

#ifndef _NORMAL_KERNEL_HALF_SIZE
#define _NORMAL_KERNEL_HALF_SIZE 1
#endif

#ifndef _MATERIAL_KERNEL_HALF_SIZE
#define _MATERIAL_KERNEL_HALF_SIZE 1
#endif

#ifndef _INSTANCE_KERNEL_HALF_SIZE
#define _INSTANCE_KERNEL_HALF_SIZE 1
#endif


#define is_valid(name) (is_valid_##name != 0)

cbuffer PerFrameCB
{
    uint2 gResolution;
    uint gDepthKernelCenter = 1;
    uint gNormalKernelCenter = 1;
    uint gMaterialKernelCenter = 1;
    uint gInstanceKernelCenter = 1;

    float4 gDepthChannelMultiplyer = float4(1.0, 1.0, 1.0, 0.0);
    float4 gNormalChannelMultiplyer = float4(1.0, 1.0, 1.0, 0.0);
    float4 gMaterialChannelMultiplyer = float4(1.0, 1.0, 1.0, 0.0);
    float4 gInstanceChannelMultiplyer = float4(1.0, 1.0, 1.0, 0.0);

    float4 gFullAlpha = float4(0.0, 0.0, 0.0, 1.0);

    float2 gDepthDistanceRange;   // min/max depth distance values
    float2 gNormalThresholdRange; // normal threshold min/max range
}

// Input edge detection data.
Texture2D<float>            gDepth;            // optional depth buffer
Texture2D<float3>           gNormal;           // optional normal buffer
Texture2D<uint>             gMaterialID;       // optional material id buffer
Texture2D<uint>             gInstanceID;       // optional instance id buffer
Texture2D<PackedHitInfo>    gVBuffer;          // optional visibility buffer

// Temporary data.
RWTexture2D<float2>         gTmpDepth;
RWTexture2D<float2>         gTmpNormal;
RWTexture2D<float2>         gTmpMaterialID;
RWTexture2D<float2>         gTmpInstanceID;

// Kernel textures
Texture1D<float>            gDepthKernelU;
Texture1D<float>            gDepthKernelV;

Texture1D<float>            gNormalKernelU;
Texture1D<float>            gNormalKernelV;

Texture1D<float>            gMaterialKernelU;
Texture1D<float>            gMaterialKernelV;

Texture1D<float>            gInstanceKernelU;
Texture1D<float>            gInstanceKernelV;

// Ouput
RWTexture2D<float4> gOutput;

bool calcShadingData(const float3 rayDir, const HitInfo hit, out ShadingData sd) {
    [[branch ]] if (hit.getType() != HitType::Triangle) return false;

    sd = {};
    ITextureSampler lod = ExplicitLodTextureSampler(0.f);
    TriangleHit triangleHit = hit.getTriangleHit();

    [[branch ]] if(gScene.isObjectMatteShaded(triangleHit.instanceID)) return false;

    const VertexData v = gScene.getVertexData(triangleHit);
    const uint materialID = gScene.getMaterialID(triangleHit.instanceID);
    sd = gScene.materials.prepareShadingData(v, materialID, -rayDir, lod);
    return true;
}

float _getDepth(const uint2 ipos, const float defaultDepth, out bool valid) {        
    // Prefer VBuffer
    #if is_valid(gVBuffer)
        const HitInfo hit = HitInfo(gVBuffer[ipos]);
        [[branch ]] if(!hit.isValid()) {
            valid = false;
            return defaultDepth;
        }

        const float3 rayDir = normalize(gScene.camera.computeRayPinhole(ipos, gResolution).dir);
        ShadingData sd;
        
        [[branch ]] if(!calcShadingData(rayDir, hit, sd)) {
            valid = false;
            return defaultDepth;
        }

        const float4 screenP = mul(float4(sd.posW, 1.f), gScene.camera.getViewProjMat());
        const float4 viewP = mul(screenP, gScene.camera.data.invProjMat);
        valid = true;
        return -(viewP.z / viewP.w);
      
    #elif is_valid(gDepth)
        const float2 screenUV = (float2(ipos) / float2(gResolution)) * 2.0 - float2(1.f);
        const float4 screenP = float4(screenUV.x, screenUV.y, gDepth[ipos], 1.0);
        const float4 viewP = mul(screenP, gScene.camera.data.invProjMat);
        valid = true;
        return -(viewP.z / viewP.w);
    #else
        valid = false;
        return defaultDepth;
    #endif
}

// Get screen space normal
float3 _getNormalSS(const uint2 ipos, const float3 defaultN, out bool valid) {
    // Prefer VBuffer
    #if is_valid(gVBuffer)
        const HitInfo hit = HitInfo(gVBuffer[ipos]);
        [[branch ]] if(!hit.isValid()) { valid = false; return defaultN; }


        const float3 rayDir = normalize(gScene.camera.computeRayPinhole(ipos, gResolution).dir);
        ShadingData sd;
        
        [[branch ]] if(!calcShadingData(rayDir, hit, sd)) { valid = false; return defaultN; }

        //return normalize(mul(float4(sd.frame.N, 0.f), gScene.camera.getViewProjMat()).xyz);
        valid = true;
        return normalize(sd.frame.N);

    #elif is_valid(gNormal)
        valid = true;
        return normalize(mul(float4(gNormal[ipos], 0.f), gScene.camera.getViewProjMat()).xyz);
    #else
        valid = false;
        return defaultN;
    #endif
}

float _getMaterialID(const uint2 ipos, out bool valid) {
    // Prefer VBuffer
    #if is_valid(gVBuffer)
        const HitInfo hit = HitInfo(gVBuffer[ipos]);
        [[branch ]] if(!hit.isValid()) { valid = false; return 0.f; }
        [[branch ]] if(hit.getType() != HitType::Triangle) { valid = false; return 0.f; }

        TriangleHit triangleHit = hit.getTriangleHit();
        valid = true;
        return float(gScene.getMaterialID(triangleHit.instanceID) + 1);
    #elif is_valid(gMaterialID)
        valid = true;
        return float(gMaterialID[ipos]);
    #else
        valid = false;
        return 0.f;
    #endif
}

float _getInstanceID(const uint2 ipos, out bool valid) {
    // Prefer VBuffer
    #if is_valid(gVBuffer)
        const HitInfo hit = HitInfo(gVBuffer[ipos]);
        [[branch ]] if(!hit.isValid()) { valid = false; return 0.f; };

        valid = true;
        return float(gScene.getGeometryInstance(hit.getTriangleHit().instanceID).globalMatrixID + 1);
    #elif is_valid(gInstanceID)
        valid = true;
        return float(gInstanceID[ipos]);
    #else
        valid = false;
        return 0.f;
    #endif
}

/** U pass.
*/
[numthreads(16, 16, 1)]
void passU(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gResolution)) return;
    const uint2 pixelPos = dispatchThreadId.xy;

    #if is_valid(gVBuffer)
        #if (_TRACE_ALPHA == 0)
        [[branch ]] if(!HitInfo(gVBuffer[pixelPos]).isValid()) {
            #if is_valid(gTmpDepth)
                gTmpDepth[pixelPos] = float2(0.f);
            #endif

            #if is_valid(gTmpNormal)
                gTmpNormal[pixelPos] = float2(0.f);
            #endif

            #if is_valid(gTmpMaterialID)
                gTmpMaterialID[pixelPos] = float2(0.f);
            #endif

            #if is_valid(gTmpInstanceID)
                gTmpInstanceID[pixelPos] = float2(0.f);
            #endif

            return;
        }
        #else
            const bool alpha = HitInfo(gVBuffer[pixelPos]).isValid();
        #endif
    #else

        const bool alpha = false;

    #endif

    bool validDepth = true;
    const float cD = _getDepth(pixelPos, 10.f * gDepthDistanceRange.y, validDepth); // central pixel depth
    float2 d = float2(cD * gDepthKernelU[gDepthKernelCenter], cD * gDepthKernelU[gDepthKernelCenter]);

    bool validNormal = true;
    const float3 cN = _getNormalSS(pixelPos, float3(0.f, 0.f, -1.f), validNormal); // central pixel screen space normal
    float2 n = float2(0.f); // No need to multiply by cN as dot(cN, cN) always 1

    bool validMaterial = true;
    const float cM = _getMaterialID(pixelPos, validMaterial); //central pixel material id
    float2 m_id = float2(cM * gMaterialKernelU[gMaterialKernelCenter], cM * gMaterialKernelU[gMaterialKernelCenter]);
    
    bool validInstance = true;
    const float cI = _getInstanceID(pixelPos, validInstance); //central pixel instance id
    float2 i_id = float2(cI * gInstanceKernelU[gInstanceKernelCenter], cI * gInstanceKernelU[gInstanceKernelCenter]);

    #if (_TRACE_DEPTH == 1) && (is_valid(gVBuffer) || is_valid(gDepth))
    [[unroll]]
    for(uint i = 1; i <= _DEPTH_KERNEL_HALF_SIZE; i++) {
        bool b1, b2, b3, b4;

        const float tD = _getDepth(pixelPos - uint2(i, 0), cD + 10.f * gDepthDistanceRange.y, b1);
        const float bD = _getDepth(pixelPos + uint2(i, 0), cD + 10.f * gDepthDistanceRange.y, b2);
        const float lD = _getDepth(pixelPos - uint2(0, i), cD + 10.f * gDepthDistanceRange.y, b3);
        const float rD = _getDepth(pixelPos + uint2(0, i), cD + 10.f * gDepthDistanceRange.y, b4);

        validDepth = (validDepth && b1 && b2 && b3 && b4);

        d.x += tD * gDepthKernelU[gDepthKernelCenter - i];
        d.x += bD * gDepthKernelU[gDepthKernelCenter + i];
        d.y += lD * gDepthKernelU[gDepthKernelCenter - i];
        d.y += rD * gDepthKernelU[gDepthKernelCenter + i];
    }
    #endif
    
    #if (_TRACE_NORMAL == 1) && (is_valid(gVBuffer) || is_valid(gNormal))
    for(uint i = 1; i <= _NORMAL_KERNEL_HALF_SIZE; i++) {
        bool b1, b2, b3, b4;

        const float3 tN = _getNormalSS(pixelPos - uint2(i, 0), -cN, b1);
        const float3 bN = _getNormalSS(pixelPos + uint2(i, 0), -cN, b2);
        const float3 lN = _getNormalSS(pixelPos - uint2(0, i), -cN, b3);
        const float3 rN = _getNormalSS(pixelPos + uint2(0, i), -cN, b4);

        validNormal = (validNormal && b1 && b2 && b3 && b4);

        n.x += (.5f + (dot(cN, tN) * -.5f)) * gNormalKernelU[gNormalKernelCenter - i];
        n.x += (.5f + (dot(cN, bN) * -.5f)) * gNormalKernelU[gNormalKernelCenter + i];
        n.y += (.5f + (dot(cN, lN) * -.5f)) * gNormalKernelU[gNormalKernelCenter - i];
        n.y += (.5f + (dot(cN, rN) * -.5f)) * gNormalKernelU[gNormalKernelCenter + i];
    }
    #endif

    #if (_TRACE_MATERIAL_ID == 1) && (is_valid(gVBuffer) || is_valid(gMaterialID))
    [[unroll]]
    for(uint i = 1; i <= _MATERIAL_KERNEL_HALF_SIZE; i++) {
        bool b1, b2, b3, b4;

        const float tM = _getMaterialID(pixelPos - uint2(i, 0), b1);
        const float bM = _getMaterialID(pixelPos + uint2(i, 0), b2);
        const float lM = _getMaterialID(pixelPos - uint2(0, i), b3);
        const float rM = _getMaterialID(pixelPos + uint2(0, i), b4);

        validMaterial = (validMaterial && b1 && b2 && b3 && b4);

        m_id.x += tM * gMaterialKernelU[gMaterialKernelCenter - i];
        m_id.x += bM * gMaterialKernelU[gMaterialKernelCenter + i];
        m_id.y += lM * gMaterialKernelU[gMaterialKernelCenter - i];
        m_id.y += rM * gMaterialKernelU[gMaterialKernelCenter + i];
    }
    #endif

    #if (_TRACE_INSTANCE_ID == 1) && (is_valid(gVBuffer) || is_valid(gInstanceID))
    [[unroll]]
    for(uint i = 1; i <= _INSTANCE_KERNEL_HALF_SIZE; i++) {
        bool b1, b2, b3, b4;
        
        const float tI = _getInstanceID(pixelPos - uint2(i, 0), b1);
        const float bI = _getInstanceID(pixelPos + uint2(i, 0), b2);
        const float lI = _getInstanceID(pixelPos - uint2(0, i), b3);
        const float rI = _getInstanceID(pixelPos + uint2(0, i), b4);

        validInstance = (validInstance && b1 && b2 && b3 && b4);

        i_id.x += tI * gInstanceKernelU[gInstanceKernelCenter - i];
        i_id.x += bI * gInstanceKernelU[gInstanceKernelCenter + i];
        i_id.y += lI * gInstanceKernelU[gInstanceKernelCenter - i];
        i_id.y += rI * gInstanceKernelU[gInstanceKernelCenter + i];
    }
    #endif

    #if (_TRACE_DEPTH == 1) && is_valid(gTmpDepth)
        #if (_TRACE_ALPHA == 0)
            gTmpDepth[pixelPos] = validDepth ? d : float2(0.f);
        #else 
            gTmpDepth[pixelPos] = d;
        #endif
    #endif

    #if (_TRACE_NORMAL == 1) && is_valid(gTmpNormal)
        #if (_TRACE_ALPHA == 0)
            gTmpNormal[pixelPos] = validNormal ? n : float2(0.f);
        #else 
            gTmpNormal[pixelPos] = n;
        #endif
    #endif

    #if (_TRACE_MATERIAL_ID == 1) && is_valid(gTmpMaterialID)
        #if (_TRACE_ALPHA == 0)
            gTmpMaterialID[pixelPos] = validMaterial ? m_id : float2(0.f);
        #else 
            gTmpMaterialID[pixelPos] = m_id;
        #endif
    #endif

    #if (_TRACE_INSTANCE_ID == 1) && is_valid(gTmpInstanceID)
        #if (_TRACE_ALPHA == 0)
            gTmpInstanceID[pixelPos] = validInstance ? i_id : float2(0.f);
        #else 
            gTmpInstanceID[pixelPos] = i_id;
        #endif
    #endif
}

/** V pass.
*/
[numthreads(16, 16, 1)]
void passV(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gResolution)) return;
    const uint2 pixelPos = dispatchThreadId.xy;

    #if (_TRACE_ALPHA == 0)
        #if is_valid(gVBuffer)
            [[branch ]] if(!HitInfo(gVBuffer[pixelPos]).isValid()) {
                gOutput[pixelPos] = float4(0.f);
                return;
            }
        #endif
    #endif

    #if is_valid(gTmpDepth)
    const float2 tmpDepth = gTmpDepth[pixelPos];
    #else
    const float2 tmpDepth = float2(0.f);
    #endif
    float2 d = float2(tmpDepth.x * gDepthKernelV[gDepthKernelCenter], tmpDepth.y * gDepthKernelV[gDepthKernelCenter]);

    #if is_valid(gTmpNormal)
    const float2 tmpNormal = gTmpNormal[pixelPos];
    #else 
    const float2 tmpNormal = float2(0.f);
    #endif
    float2 n = float2(tmpNormal.x * gNormalKernelV[gNormalKernelCenter], tmpNormal.y * gNormalKernelV[gNormalKernelCenter]);

    #if is_valid(gTmpMaterialID)
    const float2 tmpMaterialID = gTmpMaterialID[pixelPos];
    #else 
    const float2 tmpMaterialID = float2(0.f);
    #endif
    float2 m_id = float2(tmpMaterialID.x * gMaterialKernelV[gMaterialKernelCenter], tmpMaterialID.y * gMaterialKernelV[gMaterialKernelCenter]);

    #if is_valid(gTmpInstanceID)
    const float2 tmpInstanceID = gTmpInstanceID[pixelPos];
    #else 
    const float2 tmpInstanceID = float2(0.f);
    #endif
    float2 i_id = float2(tmpInstanceID.x * gInstanceKernelV[gNormalKernelCenter], tmpInstanceID.y * gInstanceKernelV[gNormalKernelCenter]);


    #if (_TRACE_DEPTH == 1) && is_valid(gTmpDepth)
    [[unroll]]
    for(uint i = 1; i <= _DEPTH_KERNEL_HALF_SIZE; i++) {
        d.x += gTmpDepth[pixelPos + uint2(0, i)].x * gDepthKernelV[gDepthKernelCenter + i];
        d.x += gTmpDepth[pixelPos - uint2(0, i)].x * gDepthKernelV[gDepthKernelCenter - i];
        d.y += gTmpDepth[pixelPos + uint2(i, 0)].y * gDepthKernelV[gDepthKernelCenter + i];
        d.y += gTmpDepth[pixelPos - uint2(i, 0)].y * gDepthKernelV[gDepthKernelCenter - i];
    }
    #endif

    #if (_TRACE_NORMAL == 1) && is_valid(gTmpNormal)
    [[unroll]]
    for(uint i = 1; i <= _NORMAL_KERNEL_HALF_SIZE; i++) {
        n.x += gTmpNormal[pixelPos + uint2(0, i)].x * gNormalKernelV[gNormalKernelCenter + i];
        n.x += gTmpNormal[pixelPos - uint2(0, i)].x * gNormalKernelV[gNormalKernelCenter - i];
        n.y += gTmpNormal[pixelPos + uint2(i, 0)].y * gNormalKernelV[gNormalKernelCenter + i];
        n.y += gTmpNormal[pixelPos - uint2(i, 0)].y * gNormalKernelV[gNormalKernelCenter - i];
    }
    #endif

    #if (_TRACE_MATERIAL_ID == 1) && is_valid(gTmpMaterialID)
    [[unroll]]
    for(uint i = 1; i <= _MATERIAL_KERNEL_HALF_SIZE; i++) {
        const float2 mu = gTmpMaterialID[pixelPos + uint2(0, i)];
        const float2 md = gTmpMaterialID[pixelPos - uint2(0, i)];

        //m_id.x += mu.x * gMaterialKernelV[gMaterialKernelCenter + i];
        //m_id.x += md.x * gMaterialKernelV[gMaterialKernelCenter - i];
    
        //m_id.y += mu.y * gMaterialKernelU[gMaterialKernelCenter + i];
        //m_id.y += md.y * gMaterialKernelU[gMaterialKernelCenter - i];
    }
    #endif

    #if (_TRACE_INSTANCE_ID == 1) && is_valid(gTmpInstanceID)
    [[unroll]]
    for(uint i = 1; i <= _INSTANCE_KERNEL_HALF_SIZE; i++) {
        //i_id.x += gTmpInstanceID[pixelPos + uint2(0, i)].x * gInstanceKernelV[gInstanceKernelCenter + i];
        //i_id.x += gTmpInstanceID[pixelPos - uint2(0, i)].x * gInstanceKernelV[gInstanceKernelCenter - i];
        //i_id.y += gTmpInstanceID[pixelPos + uint2(i, 0)].y * gInstanceKernelV[gInstanceKernelCenter + i];
        //i_id.y += gTmpInstanceID[pixelPos - uint2(i, 0)].y * gInstanceKernelV[gInstanceKernelCenter - i];
    }
    #endif

    // Compute Depth edge
    #if is_valid(gTmpDepth)
    d /= 18.0f;
    const float depth_gradient_mag = sqrt(d.x*d.x + d.y*d.y);
    #else
    const float depth_gradient_mag = 0.f;
    #endif
    const float depth_dist_clamped = max(0.f, (depth_gradient_mag - gDepthDistanceRange.x) * (1.f / (gDepthDistanceRange.y - gDepthDistanceRange.x)));
    const float depth_edge = min(1.f, depth_dist_clamped);


    // Compute Normal edge
    #if is_valid(gTmpNormal)
    n /= 8.0f;
    const float normal_gradient_mag = sqrt(n.x*n.x + n.y*n.y);
    #else
    const float normal_gradient_mag = 0.f;
    #endif
    const float normal_dist_clamped = max(0.f, (normal_gradient_mag - gNormalThresholdRange.x) * (1.f / (gNormalThresholdRange.y - gNormalThresholdRange.x)));
    const float normal_edge = min(1.f, normal_dist_clamped);
    
    // Compute MaterialID edge
    #if is_valid(gTmpMaterialID)
    m_id /= 8.f;
    const float m_id_gradient_mag = sqrt(m_id.x*m_id.x + m_id.y*m_id.y);
    #else
    const float m_id_gradient_mag = 0.f;
    #endif
    const float m_id_edge = (m_id_gradient_mag < .0001f) ? 0.f : 1.f;

    // Compute InstanceID edge
    #if is_valid(gTmpInstanceID)
    i_id /= 8.0f;
    const float i_id_gradient_mag = sqrt(i_id.x*i_id.x + i_id.y*i_id.y);
    #else
    const float i_id_gradient_mag = 0.f;
    #endif
    const float i_id_edge = (i_id_gradient_mag < .0001f) ? 0.f : 1.f;
    

    // Combined edge
    const float4 edge_combined = max(max(max(depth_edge * gDepthChannelMultiplyer, normal_edge * gNormalChannelMultiplyer), m_id_edge * gMaterialChannelMultiplyer), i_id_edge * gInstanceChannelMultiplyer);

    // Write detected edge data
    gOutput[pixelPos] = edge_combined + gFullAlpha;

}
