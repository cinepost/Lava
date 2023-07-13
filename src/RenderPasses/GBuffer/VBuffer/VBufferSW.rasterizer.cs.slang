/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "Scene/SceneDefines.slangh"
#include "Utils/Math/MathConstants.slangh"

import VBufferSW;

#ifndef MICROPOLY_MAX_BB_SIZE
#define MICROPOLY_MAX_BB_SIZE 8.f // 4x4 pixel micropoly
#endif

ConstantBuffer<VBufferSW> gVBufferSW;

groupshared uint triangleCount; // triangles
groupshared uint num_vertices;
groupshared uint base_index;
groupshared uint base_vertex;

groupshared float4x4 worldMat;
groupshared GeometryInstanceID instanceID;
groupshared GeometryInstanceData instance;
groupshared MeshDesc meshDesc;
groupshared MeshletDraw draw;
groupshared Meshlet meshlet;

groupshared uint   indices[MESHLET_MAX_POLYGONS_COUNT * 4];
groupshared uint   prim_ids[MESHLET_MAX_POLYGONS_COUNT];
groupshared float3 positions[MESHLET_MAX_VERTICES_COUNT];


float3 transformPoint(const float3 p) {
    const float4 pV = mul(float4(p, 1.f), gScene.camera.getViewProj());
    float3 pS = pV.xyz / pV.w;
    pS.xy *= float2(.5f, -.5f);
    pS.xy += float2(0.5f);
    pS.xy *= gVBufferSW.frameDimF;
    return pS;
}

void rasterizeMicroTriangle(float3 p0, float3 p1, float3 p2, float2 min_p, float2 max_p) {
    for(float y = min_p.y; y <= max_p.y; y += 1.0f) {
        for(float x = min_p.x; x <= max_p.x; x += 1.0f) {
            TriangleHit triangleHit = {};
            //GeometryInstanceID instanceID;
            triangleHit.primitiveIndex = 1;
            triangleHit.barycentrics = float2(0.5f);
            gVBufferSW.writeHit(uint2(x, y), HitInfo(triangleHit.pack()));
        }
    }
}

void rasterizeTriangle(float3 p0, float3 p1, float3 p2, uint primitiveIndex, bool cullCCW) {
    [[branch]] if(p0.z < 0.0f || p1.z < 0.0f || p2.z < 0.0f) return;

    const float2 frame_size = gVBufferSW.frameDimF;
    float3 p10 = p1 - p0;
    float3 p20 = p2 - p0;
    float det = p20.x * p10.y - p20.y * p10.x;
    
    // CCW culling
    [[branch]] if(cullCCW && det >= 0.0f) return;

    float2 min_p = floor(min(min(p0.xy, p1.xy), p2.xy));
    float2 max_p = ceil(max(max(p0.xy, p1.xy), p2.xy));

    //float2 min_p = round(min(min(p0.xy, p1.xy), p2.xy));
    //float2 max_p = round(max(max(p0.xy, p1.xy), p2.xy));

    // Cut offscreen triangle
    [[branch]] if(max_p.x < 0.0f || max_p.y < 0.0f || min_p.x >= frame_size.x || min_p.y >= frame_size.y) return;

    //[[branch]] if(((max_p.x - min_p.x) <= MICROPOLY_MAX_BB_SIZE) && ((max_p.y - min_p.y) <= MICROPOLY_MAX_BB_SIZE)) rasterizeMicroTriangle(p0, p1, p2, min_p, max_p); 

    min_p = clamp(min_p, float2(0.0f), frame_size - 1.0f);
    max_p = clamp(max_p, float2(0.0f), frame_size - 1.0f);

    float2 texcoord_dx = float2(-p20.y, p10.y) / det;
    float2 texcoord_dy = float2(p20.x, -p10.x) / det;

    float2 texcoord_x = texcoord_dx * (min_p.x - p0.x);
    float2 texcoord_y = texcoord_dy * (min_p.y - p0.y);

    for(float y = min_p.y; y <= max_p.y; y += 1.0f) {
        float2 texcoord = texcoord_x + texcoord_y;
        for(float x = min_p.x; x <= max_p.x; x += 1.0f) {
            if(texcoord.x >= 0.0f && texcoord.y >= 0.0f && texcoord.x + texcoord.y <= 1.0f) {
                float z = p10.z * texcoord.x + p20.z * texcoord.y + p0.z;
                uint2 pixelPos = uint2(x, y);
                const uint uint_z = asuint(z);
                
                const uint64_t z64 = uint64_t(asuint(z)) << 32;
                
                const uint idx = (gVBufferSW.frameDim.x * pixelPos.y + pixelPos.x) << 3;
                const uint64_t prim_value = z64 | primitiveIndex;
                const uint tx = asuint(f32tof16(texcoord.x));
                const uint ty = asuint(f32tof16(texcoord.y));
                const uint64_t parm_value = (z64 | (tx << 16)) | ty;
                const uint64_t inst_value = z64 | instanceID.index;
                
                uint64_t v = gLocalDepthPrimBuffer.InterlockedMinU64(idx, prim_value);
                if ( v == prim_value) {
                  gLocalDepthParmBuffer.InterlockedMinU64(idx, parm_value);
                  gLocalDepthInstBuffer.InterlockedMinU64(idx, inst_value);
                }
            }
            texcoord += texcoord_dx;
        }
        texcoord_y += texcoord_dy;
    }
}

void rasterizePoint(float3 p, float radius = .5f) {
    const float2 frame_size = gVBufferSW.frameDimF;

    [[branch]] if(p.x < radius || p.y < radius || p.x >= (frame_size.x + radius) || p.y >= (frame_size.y + radius)) return;

    TriangleHit triangleHit = {};
    //GeometryInstanceID instanceID;
    triangleHit.primitiveIndex = 1;
    triangleHit.barycentrics = float2(0.5f);
    gVBufferSW.writeHit(uint2(p.x, p.y), HitInfo(triangleHit.pack()));
}

[numthreads(THREADS_COUNT, 1, 1)]
void rasterize(uint3 groupID : SV_GroupID, uint groupIdx : SV_GroupIndex) {
    const uint group_id = groupID.y * gVBufferSW.dispatchX + groupID.x;       // Sequential group index.
    const uint thread_id = groupIdx;                                          // Local thread index in group (range 0..THREADS_COUNT-1).
    const uint global_id = group_id * THREADS_COUNT + thread_id;              // Global element index

    // Early termination
    [[branch]] if (group_id >= gVBufferSW.meshletDrawsCount) return;

    /// Group shared data stage

    // meshlet parameters
    [[branch]] if(thread_id == 0u) {
        draw = gMeshletDrawList[group_id];
        if(draw.drawCount > 0u) {
            instanceID.index = draw.instanceID;
            instance = gScene.getGeometryInstance(instanceID);
            meshDesc = gScene.getMeshDesc(instanceID);
            worldMat = gScene.getWorldMatrix(instanceID);
            meshlet = gScene.meshlets[draw.meshletID];
            triangleCount = meshlet.indexCount / 3;
        }  
    }
    GroupMemoryBarrierWithGroupSync();

    [[branch]] if(draw.drawCount == 0u) return;

    /// Fetch and transform stage

    // Fetch indices
    [[branch]] if(thread_id < triangleCount) {
        const uint meshletTriangleRootIndex = thread_id * 3u;
        prim_ids[thread_id] = gScene.meshletPrimIndices[meshlet.primIndexOffset + thread_id];
        const uint3 _indices = gScene.meshletIndexData.Load<uint8_t3>(meshlet.indexOffset + meshletTriangleRootIndex);

        indices[meshletTriangleRootIndex] = _indices[0];
        indices[meshletTriangleRootIndex + 1] = _indices[1];
        indices[meshletTriangleRootIndex + 2] = _indices[2];
    }

    //GroupMemoryBarrierWithGroupSync();

    // Transform vertices
    uint vi = thread_id;
    uint vi2 = thread_id + THREADS_COUNT;
    if(vi < meshlet.vertexCount) {
        uint vertexOffset = meshlet.vertexOffset + gScene.meshletVertices[vi];
        positions[vi] = transformPoint(mul(float4(gScene.getVertex(vertexOffset).position, 1.f), worldMat).xyz);

    }

    //GroupMemoryBarrierWithGroupSync();

    // Transform more vertices if needed
    if(vi2 < meshlet.vertexCount) {
        uint vertexOffset = meshlet.vertexOffset + gScene.meshletVertices[vi2];
        positions[vi2] = transformPoint(mul(float4(gScene.getVertex(vertexOffset).position, 1.f), worldMat).xyz);
    }

    GroupMemoryBarrierWithGroupSync();


    /// Rasterizing stage
    [[branch]]if(thread_id >= triangleCount) return;

    const bool cullCCW = false;

    const uint meshletTriangleRootIndex = thread_id * 3u;
    rasterizeTriangle(positions[indices[meshletTriangleRootIndex]], positions[indices[meshletTriangleRootIndex + 1]], positions[indices[meshletTriangleRootIndex + 2]], prim_ids[thread_id], cullCCW);
}
