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

#ifndef USE_PP_JITTER
#define USE_PP_JITTER true
#endif

#ifndef USE_D64
#define USE_D64 true
#endif

#define is_valid(name) (is_valid_##name != 0)

ConstantBuffer<VBufferSW> gVBufferSW;

Texture2D<float2> gJitterTexture;
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
groupshared float               cocs[MESHLET_MAX_VERTICES_COUNT];

#if USE_D64
groupshared double      z64pos[MESHLET_MAX_VERTICES_COUNT];
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
    [[branch]] if((pt0.z < 0.0f && pt1.z < 0.0f && pt2.z < 0.0f) || (pt0.z > 1.0f && pt1.z > 1.0f && pt2.z > 1.0f)) return;

    const float2 frame_size = float2(gVBufferSW.frameDim);
    float3 pt10 = pt1.xyz - pt0.xyz;
    float3 pt20 = pt2.xyz - pt0.xyz;
    float det = pt20.x * pt10.y - pt20.y * pt10.x;

    
    // CCW culling
    [[branch]] if(cullCCW && det < 0.0f) return;

    float2 min_p = round(min(min(pt0.xy, pt1.xy), pt2.xy));
    float2 max_p = round(max(max(pt0.xy, pt1.xy), pt2.xy));

    // Cut offscreen triangle
    [[branch]] if(max_p.x < 0.0f || max_p.y < 0.0f || min_p.x >= frame_size.x || min_p.y >= frame_size.y) return;

    //[[branch]] if(((max_p.x - min_p.x) <= MICROPOLY_MAX_BB_SIZE) && ((max_p.y - min_p.y) <= MICROPOLY_MAX_BB_SIZE)) rasterizeMicroTriangle(pt0, pt1, pt2, min_p, max_p); 

    min_p = clamp(min_p, float2(0.0f), frame_size - 1.0f);
    max_p = clamp(max_p, float2(0.0f), frame_size - 1.0f);

    float2 texcoord_dx = float2(-pt20.y, pt10.y) / det;
    float2 texcoord_dy = float2(pt20.x, -pt10.x) / det;

    float2 texcoord_x = texcoord_dx * (min_p.x - pt0.x);
    float2 texcoord_y = texcoord_dy * (min_p.y - pt0.y);

    SampleGenerator lsg = SampleGenerator(gVBufferSW.frameDim, gVBufferSW.sampleNumber);

    for(float y = min_p.y; y <= max_p.y; y += 1.0f) {
        float2 texcoord = texcoord_x + texcoord_y;
        for(float x = min_p.x; x <= max_p.x; x += 1.0f) {
            const uint2 pixelPos = uint2(x, y);

            if(texcoord.x >= 0.0f && texcoord.y >= 0.0f && texcoord.x + texcoord.y <= 1.0f) {
                float z = pt10.z * texcoord.x + pt20.z * texcoord.y + pt0.z;
                
                const uint64_t z64 = uint64_t(asuint(z)) << 32;
                
                const uint idx = (gVBufferSW.frameDim.x * pixelPos.y + pixelPos.x) << 3;
                const uint64_t prim_value = z64 | primitiveIndex;
                const uint tx = asuint(f32tof16(texcoord.x));
                const uint ty = asuint(f32tof16(texcoord.y));
                const uint64_t parm_value = (z64 | (tx << 16)) | ty;
                const uint64_t inst_value = z64 | instanceID.index;
                
                uint64_t v = gLocalDepthBuffer.InterlockedMinU64(idx, prim_value);
                if ( prim_value < v) {
                    gVBufferSW.writeMeshletID(pixelPos, draw.meshletID);
                    gVBufferSW.writeMicroPolyID(pixelPos, primitiveIndex);
                }
            }
            texcoord += texcoord_dx;
        }
        texcoord_y += texcoord_dy;
    }
}

void rasterizeTriangle2(uint pi0, uint pi1, uint pi2, uint primitiveIndex, bool cullCCW) {
    float4 pt0 = positions[pi0], pt1 = positions[pi1], pt2 = positions[pi2];
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

    float c0 = cocs[pi0], c1 = cocs[pi1], c2 = cocs[pi2];
    c0 *= gScene.camera.data.apertureRadius * gScene.camera.data.focalDistance * gScene.camera.data.focalLength;
    c1 *= gScene.camera.data.apertureRadius * gScene.camera.data.focalDistance * gScene.camera.data.focalLength;
    c2 *= gScene.camera.data.apertureRadius * gScene.camera.data.focalDistance * gScene.camera.data.focalLength;

    int2 bb_min = int2(floor(min(min(pt0.x - c0, pt1.x - c1), pt2.x - c2) - jitterBound.x), floor(min(min(pt0.y - c0, pt1.y - c1), pt2.y - c2) - jitterBound.y));
    int2 bb_max = int2(ceil(max(max(pt0.x + c0, pt1.x + c1), pt2.x + c2) + jitterBound.x), ceil(max(max(pt0.y + c0, pt1.y + c1), pt2.y + c2) + jitterBound.y));

    if((bb_max.x < 0) || (bb_max.y < 0) || (bb_min.x > (gVBufferSW.frameDim.x-2)) || (bb_min.y > (gVBufferSW.frameDim.y-2))) return;

    uint2 bb_min_u = uint2(clamp(bb_min, int2(0), int2(gVBufferSW.frameDim - int2(1))));
    uint2 bb_max_u = uint2(clamp(bb_max, int2(0), int2(gVBufferSW.frameDim - int2(1))));

    SampleGenerator lsg = SampleGenerator(gVBufferSW.frameDim, gVBufferSW.sampleNumber);

    for(uint y=bb_min_u.y; y<bb_max_u.y; y++) {
        for(uint x=bb_min_u.x; x<bb_max_u.x; x++) {

            // Jitter (noise) [0, 1] range
            float2 jitter = gJitterTexture[uint2(x - 128 * floor( x / 128.0), y - 128 * floor(y / 128.0))].xy;
            
            // Anti aliasing jitter [-0.5, 0.5] range 
            float2 aaj = jitter - float2(0.5);

            float2 coc_sample = sample_disk(jitter); // range [-1, 1]
            
            float3 pj0 = pt0.xyz, pj1 = pt1.xyz, pj2 = pt2.xyz;
            pj0.xy += aaj + coc_sample * c0; 
            pj1.xy += aaj + coc_sample * c1; 
            pj2.xy += aaj + coc_sample * c2;

#if COMPUTE_DEPTH_OF_FIELD
            // CCW culling
            #if CULL_MODE == CULL_BACK
            [[branch]] if(cullCCW && cross(float3(pj1.xy - pj0.xy, .0), float3(pj2.xy - pj0.xy, .0)).z > 0.0f) continue;
            #elif CULL_MODE == CULL_FRONT
            [[branch]] if(cullCCW && cross(float3(pj1.xy - pj0.xy, .0), float3(pj2.xy - pj0.xy, .0)).z < 0.0f) continue;
            #endif
#endif

            float2 uv = triIntersect(float2(x, y), pj0.xy, pj1.xy, pj2.xy);
            
            if( uv.x < 0.) continue;

#if USE_D64
            double z0 = z64pos[pi0], z1 = z64pos[pi1], z2 = z64pos[pi2];
            double z = (z1 - z0) * uv.x + (z2 - z0) * uv.y + z0;
#else
            float z = pt10.z * uv.x + pt20.z * uv.y + pt0.z;
#endif
            if(z < 0.0f || z > 1.0) continue;

#if USE_D64
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

                float4 pndc0 = mul( float4(mul( float4(vertex_data[pi0].position, 1.0), worldMat).xyz, 1.f), gScene.camera.getViewProjMatNoJitter());
                float4 pndc1 = mul( float4(mul( float4(vertex_data[pi1].position, 1.0), worldMat).xyz, 1.f), gScene.camera.getViewProjMatNoJitter());
                float4 pndc2 = mul( float4(mul( float4(vertex_data[pi2].position, 1.0), worldMat).xyz, 1.f), gScene.camera.getViewProjMatNoJitter());

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


void rasterizePoint(float3 p, float radius = .5f) {
    const float2 frame_size = float2(gVBufferSW.frameDim);

    // TODO: eraly offscreen test here

    const uint2 screenCenter = uint2(p.xy);
    const uint screenMaxRadius = uint(ceil(radius));

    TriangleHit triangleHit = {};
    triangleHit.primitiveIndex = 1;

    SampleGenerator lsg = SampleGenerator(gVBufferSW.frameDim, gVBufferSW.sampleNumber);

    for( uint x = screenCenter.x - screenMaxRadius; x < screenCenter.x + screenMaxRadius; x++) {
        for( uint y = screenCenter.y - screenMaxRadius; y < screenCenter.y + screenMaxRadius; y++) {
            const uint2 pixelPos = uint2(x, y);

            if((p.x > frame_size.x) || (p.y > frame_size.y)) continue;

            float2 jitter = sampleNext2D(lsg);

            const float r1 = jitter.x + float(x) - p.x;
            const float r2 = jitter.y + float(y) - p.y;
            if(sqrt(r1*r1+r2*r2) >= radius) continue;

            triangleHit.barycentrics = float2(0.5f);
            gVBufferSW.writeAUX(pixelPos, float4(1.f, 1.f, 1.f, 1.f));

            writeMicropolyPixel(float3(pixelPos.xy, p.z), float2(0.5), 1, 1, 1);

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
#if USE_D64
        double4 pS = transformPointToScreenDouble(mul(double4(v.position, 1.0), worldMat).xyz);
        z64pos[vi0] = pS.z;
#else
        float4 pS = transformPointToScreen(mul(float4(v.position, 1.f), worldMat).xyz);
#endif
        positions[vi0] = pS;

#if COMPUTE_DEPTH_OF_FIELD
        cocs[vi0] = abs(pS.w - gScene.camera.data.focalDistance) / pS.w;
#else
        cocs[vi0] = 0.0f;
#endif
    }

    // Transform more vertices if needed
    if(vi1 < meshlet.vertexCount) {
        uint vertexOffset = meshDesc.vbOffset + gScene.meshletVertices[meshlet.vertexOffset + vi1];

        StaticVertexData v = gScene.getVertex(vertexOffset);
        vertex_data[vi1] = v;
#if USE_D64
        double4 pS = transformPointToScreenDouble(mul(double4(v.position, 1.0), worldMat).xyz);
        z64pos[vi1] = pS.z;
#else
        float4 pS = transformPointToScreen(mul(float4(v.position, 1.f), worldMat).xyz);
#endif
        positions[vi1] = pS;

#if COMPUTE_DEPTH_OF_FIELD
        cocs[vi1] = abs(pS.w - gScene.camera.data.focalDistance) / pS.w;
#else
        cocs[vi1] = 0.0f;
#endif
    }

    GroupMemoryBarrierWithGroupSync();


    /// Rasterizing stage
    [[branch]]if(thread_id >= primCount) return;

    const bool cullCCW = true;
    const uint prim_id = gScene.meshletPrimIndices[meshlet.primIndexOffset + thread_id];
    const uint8_t3 indices = gScene.meshletLocalIndexData.Load<uint8_t3>(meshlet.localIndexOffset + thread_id * 3u);

    rasterizeTriangle2(indices[0], indices[1], indices[2], prim_id, cullCCW);
}
