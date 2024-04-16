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

#include "VBufferSW.common.slangh"
#include "VBufferSW.functions.slangh"

import Utils.Sampling.SampleGenerator;
import Utils.Math.MathHelpers;
import Utils.Shading.VisbufferUtils;
import VBufferSW;

#ifndef THREADS_COUNT
#define THREADS_COUNT MESHLET_MAX_PRIM_COUNT
#endif

#ifndef MICROPOLY_MAX_BB_SIZE
#define MICROPOLY_MAX_BB_SIZE 8.f // 4x4 pixel micropoly
#endif

#define PP_AXIS_SIZE 2

#define is_valid(name) (is_valid_##name != 0)

ConstantBuffer<VBufferSW> gVBufferSW;

Texture2D<float4> gJitterTexture;
SamplerState gJitterSampler;

groupshared uint primCount;

groupshared float4x4 worldMat;
groupshared GeometryInstanceID instanceID;
groupshared GeometryInstanceData instance;
groupshared MeshDesc meshDesc;
groupshared MeshletDraw draw;
groupshared MeshletData meshlet;

groupshared uint32_t            indices[MESHLET_MAX_PRIM_COUNT * 4];
groupshared uint                prim_ids[MESHLET_MAX_PRIM_COUNT];
groupshared float4              positions[MESHLET_MAX_VERTICES_COUNT];
groupshared StaticVertexData    vertex_data[MESHLET_MAX_VERTICES_COUNT];

groupshared float4              pp_positions[PP_AXIS_SIZE * PP_AXIS_SIZE][MESHLET_MAX_VERTICES_COUNT];

#if defined(USE_HIGHP_DEPTH)
groupshared double              z64pos[MESHLET_MAX_VERTICES_COUNT];
#endif

RWStructuredBuffer<uint32_t>    gIndicesBuffer;
RWStructuredBuffer<uint32_t>    gPrimIndicesBuffer; 
RWStructuredBuffer<float3>      gPositionsBuffer;
RWStructuredBuffer<float>       gCocsBuffer;

static const float2 jitterBound = float2(0.5f);

void writeMicropolyPixel(uint3 pos, float2 texcoord, uint geometryInstanceIndex, uint primitiveIndex, uint meshletID) {
    const uint64_t z64 = uint64_t(asuint(pos.z)) << 32;

    const uint idx = (gVBufferSW.frameDim.x * pos.y + pos.x) << 3;
    const uint64_t prim_value = z64 | primitiveIndex;
    const uint tx = asuint(f32tof16(texcoord.x));
    const uint ty = asuint(f32tof16(texcoord.y));
    const uint64_t parm_value = (z64 | (tx << 16)) | ty;
    const uint64_t inst_value = z64 | geometryInstanceIndex;

    const uint64_t v = gLocalDepthBuffer.InterlockedMinU64(idx, prim_value);
    
    if ( prim_value < v) {
        gVBufferSW.writeMeshletID(pos.xy, meshletID);
        gVBufferSW.writeMicroPolyID(pos.xy, primitiveIndex);
    }
}


void rasterizeMicroTriangle(float3 pt0, float3 pt1, float3 pt2, float2 min_p, float2 max_p) {
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

void rasterizeTriangle(uint pi0, uint pi1, uint pi2, uint primitiveIndex, bool cullCCW) {
    float4 pt0 = positions[pi0], pt1 = positions[pi1], pt2 = positions[pi2];

    // near-far planes rejection
    [[branch]] if((pt0.z < 0.0f && pt1.z < 0.0f && pt2.z < 0.0f) || (pt0.z > 1.0f && pt1.z > 1.0f && pt2.z > 1.0f)) return;

    float4 pt10 = pt1 - pt0;
    float4 pt20 = pt2 - pt0;

#if !COMPUTE_DEPTH_OF_FIELD
    float det = pt20.x * pt10.y - pt20.y * pt10.x;
    // CCW culling
    #if CULL_MODE == CULL_BACK
    [[branch]] if(cullCCW && det < 0.0f) return;
    #elif CULL_MODE == CULL_FRONT
    [[branch]] if(cullCCW && det > 0.0f) return;
    #endif
#endif

#if COMPUTE_DEPTH_OF_FIELD
    float c0 = abs(1.0f - gScene.camera.data.focalDistance / pt0.w) * gScene.camera.data.apertureRadius * gScene.camera.data.focalLength;
    float c1 = abs(1.0f - gScene.camera.data.focalDistance / pt1.w) * gScene.camera.data.apertureRadius * gScene.camera.data.focalLength;
    float c2 = abs(1.0f - gScene.camera.data.focalDistance / pt2.w) * gScene.camera.data.apertureRadius * gScene.camera.data.focalLength;
#else
    float c0 = .0f, c1 = .0f, c2 = .0f;
#endif

    int2 bb_min = int2(floor(min(min(pt0.x - c0, pt1.x - c1), pt2.x - c2) - jitterBound.x), floor(min(min(pt0.y - c0, pt1.y - c1), pt2.y - c2) - jitterBound.y));
    int2 bb_max = int2(ceil(max(max(pt0.x + c0, pt1.x + c1), pt2.x + c2) + jitterBound.x), ceil(max(max(pt0.y + c0, pt1.y + c1), pt2.y + c2) + jitterBound.y));

    //if((bb_max.x < 0) || (bb_max.y < 0) || (bb_min.x > (gVBufferSW.frameDim.x-2)) || (bb_min.y > (gVBufferSW.frameDim.y-2))) return;

    uint2 bb_min_u = uint2(clamp(bb_min, int2(0), int2(gVBufferSW.frameDim - int2(1))));
    uint2 bb_max_u = uint2(clamp(bb_max, int2(0), int2(gVBufferSW.frameDim - int2(1))));

    SampleGenerator lsg = SampleGenerator(gVBufferSW.frameDim, gVBufferSW.sampleNumber);

    for(uint y=bb_min_u.y; y<bb_max_u.y; y++) {
        for(uint x=bb_min_u.x; x<bb_max_u.x; x++) {

            // Jitter (noise) [0, 1] range
            float4 jitter = gJitterTexture[uint2(x - 128 * floor( x / 128.0), y - 128 * floor(y / 128.0))];
            
            // Anti aliasing jitter [-0.5, 0.5] range 
            float2 aaj = jitter.xy - float2(0.5);
            float3 pj0 = pt0.xyz + float3(aaj, .0), pj1 = pt1.xyz + float3(aaj, .0), pj2 = pt2.xyz + float3(aaj, .0);

#if COMPUTE_DEPTH_OF_FIELD
            float2 coc_sample = sample_disk(jitter.zw); // range [-1, 1]
            
            pj0.xy += coc_sample * c0; 
            pj1.xy += coc_sample * c1; 
            pj2.xy += coc_sample * c2;
            
            // CCW culling
            #if CULL_MODE == CULL_BACK
            [[branch]] if(cullCCW && cross(float3(pj1.xy - pj0.xy, .0), float3(pj2.xy - pj0.xy, .0)).z > 0.0f) continue;
            #elif CULL_MODE == CULL_FRONT
            [[branch]] if(cullCCW && cross(float3(pj1.xy - pj0.xy, .0), float3(pj2.xy - pj0.xy, .0)).z < 0.0f) continue;
            #endif
#endif

            float2 uv = triIntersect(float2(x, y), pj0.xy, pj1.xy, pj2.xy);
            
            if( uv.x < 0.) continue;

#if defined(USE_HIGHP_DEPTH)
            double z0 = z64pos[pi0], z1 = z64pos[pi1], z2 = z64pos[pi2];
            double z = (z1 - z0) * uv.x + (z2 - z0) * uv.y + z0;
#else
            float z = pt10.z * uv.x + pt20.z * uv.y + pt0.z;
#endif
            if(z < 0.0f || z > 1.0) continue;

#if defined(USE_HIGHP_DEPTH)

            const uint64_t depth_value = z * UINT64_MAX;
            const uint idx = (gVBufferSW.frameDim.x * y + x) << 3;       
            [[branch]] if ( depth_value < gLocalDepthBuffer.InterlockedMinU64(idx, depth_value)) {
#else
            const uint32_t depth_value = z * 4294967295u;
            const uint idx = (gVBufferSW.frameDim.x * y + x) << 2;
            uint32_t out_value = depth_value;
            gLocalDepthBuffer.InterlockedMin(idx, depth_value, out_value);
            [[branch]] if ( depth_value < out_value) {
#endif
                TriangleHit triangleHit = {};
                triangleHit.instanceID.index = instanceID.index;
                triangleHit.primitiveIndex = primitiveIndex;
                triangleHit.barycentrics = uv;   

                uint2 pixelPos = uint2(x, y);

                gVBufferSW.writeHit(pixelPos, HitInfo(triangleHit.pack()));

#if is_valid(gTextureGrads)
                float2 tcrd0 = vertex_data[pi0].texCrd, tcrd1 = vertex_data[pi1].texCrd, tcrd2 = vertex_data[pi2].texCrd;

                float4 pndc0 = mul( float4(mul( float4(vertex_data[pi0].position, 1.0), worldMat).xyz, 1.f), gScene.camera.getViewProjMat());
                float4 pndc1 = mul( float4(mul( float4(vertex_data[pi1].position, 1.0), worldMat).xyz, 1.f), gScene.camera.getViewProjMat());
                float4 pndc2 = mul( float4(mul( float4(vertex_data[pi2].position, 1.0), worldMat).xyz, 1.f), gScene.camera.getViewProjMat());

                BarycentricDeriv deriv = calcFullBary(pndc0, pndc1, pndc2, float2(x, y)/float2(gVBufferSW.frameDim), gVBufferSW.frameDimInv2);
                GradientInterpolationResults g = interpolate2DWithDeriv(deriv, tcrd0, tcrd1, tcrd2);

                gVBufferSW.writeTexGrads(pixelPos, float4(clamp(g.dx, float2(-1.0f), float2(1.0f)), clamp(g.dy, float2(-1.0f), float2(1.0f))));
#endif

                gVBufferSW.writeMeshletID(pixelPos, draw.meshletID);
                gVBufferSW.writeMicroPolyID(pixelPos, primitiveIndex);

#if is_valid(gDrawCount)
                InterlockedAdd(gDrawCount[pixelPos], 1);
#endif

            }
        }
    } 
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
            meshlet = gScene.meshlets[draw.meshletID].unpack();
            primCount = meshlet.primCount;
        }  
    }

    GroupMemoryBarrierWithGroupSync();

    [[branch]] if(draw.drawCount == 0u) return;

    /// Fetch and transform stage

    // Transform vertices
    const uint vi0 = thread_id;
    const uint vi1 = thread_id + THREADS_COUNT;
    if(vi0 < meshlet.vertexCount) {
        uint vertexOffset = meshDesc.vbOffset + gScene.meshletVertices[meshlet.vertexOffset + vi0];

        StaticVertexData v = gScene.getVertex(vertexOffset);
        vertex_data[vi0] = v;
#if defined(USE_HIGHP_DEPTH)
        double4 pS = transformPointToScreenDouble(mul(double4(v.position, 1.0), worldMat).xyz);
        z64pos[vi0] = pS.z;
        positions[vi0] = float4(pS);
#else
        positions[vi0] = transformPointToScreen(mul(float4(v.position, 1.f), worldMat).xyz);
#endif
    }

    // Transform more vertices if needed
    if(vi1 < meshlet.vertexCount) {
        uint vertexOffset = meshDesc.vbOffset + gScene.meshletVertices[meshlet.vertexOffset + vi1];

        StaticVertexData v = gScene.getVertex(vertexOffset);
        vertex_data[vi1] = v;
#if defined(USE_HIGHP_DEPTH)
        double4 pS = transformPointToScreenDouble(mul(double4(v.position, 1.0), worldMat).xyz);
        z64pos[vi1] = pS.z;
        positions[vi1] = float4(pS);
#else
        positions[vi1] = transformPointToScreen(mul(float4(v.position, 1.f), worldMat).xyz);
#endif
        
    }

    GroupMemoryBarrierWithGroupSync();


    /// Rasterizing stage
    [[branch]]if(thread_id >= primCount) return;

    const bool cullCCW = true;
    const uint prim_id = gScene.meshletPrimIndices[meshlet.primIndexOffset + thread_id];
    const uint8_t3 indices = gScene.meshletLocalIndexData.Load<uint8_t3>(meshlet.localIndexOffset + thread_id * 3u);

    rasterizeTriangle(indices[0], indices[1], indices[2], prim_id, cullCCW);
}
