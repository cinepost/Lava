#include "Scene/SceneDefines.slangh"
#include "Utils/Math/MathConstants.slangh"

import Scene.Camera.CameraData;

#include "VBufferSW.MicroTriangle.slangh"
#include "VBufferSW.SubdivData.slangh"
#include "VBufferSW.common.slangh"
#include "VBufferSW.functions.slangh"

import Utils.Sampling.SampleGenerator;
import Utils.Math.MathHelpers;
import Utils.Math.PackedFormats;
import Utils.Math.FormatConversion;
import Utils.Shading.VisbufferUtils;
import VBufferSW;

#ifndef THREADS_COUNT
#define THREADS_COUNT 128
#endif

#ifndef MICROPOLY_MAX_BB_SIZE
#define MICROPOLY_MAX_BB_SIZE 8.f // 4x4 pixel micropoly
#endif

#ifndef MAX_MICRO_TRIANGLES
#define MAX_MICRO_TRIANGLES 256   // Max lod is 4
#endif

#ifndef MAX_LOD
#define MAX_LOD 4
#endif

#ifndef MAX_MT_PER_THREAD
#define MAX_MT_PER_THREAD 256
#endif

#define PP_AXIS_SIZE 2

#define is_valid(name) (is_valid_##name != 0)

#ifndef USE_MT_BUFF
#define USE_MT_BUFF 1
#endif

ConstantBuffer<VBufferSW> gVBufferSW;

RWByteAddressBuffer gThreadLockBuffer;
Texture2D<float4>   gJitterTexture;
SamplerState        gJitterSampler;

groupshared uint gsPrimCount;
groupshared float4x4 worldMat;
groupshared float3x3 worldInvTransposeMat;
groupshared GeometryInstanceID instanceID;
groupshared GeometryInstanceData instance;
groupshared MeshDesc gsMeshDesc;
groupshared MeshletDraw draw;
groupshared MeshletData meshlet;
groupshared MicroPatch3 gsPatch;

groupshared StaticVertexData  vertex_data[MESHLET_MAX_VERTICES_COUNT];
groupshared MicroTriangle     gsMTArray[MAX_MICRO_TRIANGLES];
groupshared uint              gsMicroTrianglesCount;
groupshared bool              gsSubdivide;
groupshared bool              gsCullCCW;
groupshared bool              gsUse16BitIndices;
groupshared uint              gsOffsetY;

RWStructuredBuffer<MicroTriangle>   gMicroTrianglesBuffer;
RWStructuredBuffer<MicroTriangle>   gMicroTriangleBuffers[1024]; 

static constexpr float k18 = 1.f / 8.f;
static constexpr float k38 = 3.f / 8.f;
static constexpr float k58 = 5.f / 8.f;
static constexpr float k316 = 3.f / 16.f;

static const uint   kInvalid = 4294967295u;
static const float2 jitterBound = float2(.5f, .5f);

//static MicroTriangle micro_triangles[MAX_MICRO_TRIANGLES];
static uint gThreadID;

bool edgesCanFit(float16_t3 esl) {
    return esl[0] >= gVBufferSW.minScreenEdgeLenSquared || esl[1] >= gVBufferSW.minScreenEdgeLenSquared || esl[2] >= gVBufferSW.minScreenEdgeLenSquared; 
}

float16_t3 microTriangleEdgesLengthsSquared(float3 pw0, float3 pw1, float3 pw2) {
    float2 p0 = transformPointToScreen(pw0).xy;
    float2 p1 = transformPointToScreen(pw1).xy;
    float2 p2 = transformPointToScreen(pw2).xy;

    return {
        (p0.y-p1.y)*(p0.y-p1.y) + (p0.x-p1.x)*(p0.x-p1.x),
        (p1.y-p2.y)*(p1.y-p2.y) + (p1.x-p2.x)*(p1.x-p2.x),
        (p2.y-p0.y)*(p2.y-p0.y) + (p2.x-p0.x)*(p2.x-p0.x)
    };
}

uint getLocalIndix(const uint ibOffset, const uint index) {
#if SCENE_HAS_INDEXED_VERTICES
    // Determine what format of the indices.
    // It's only if the scene has mixed formats that we incur the cost of checking the flag bit.
#if SCENE_HAS_16BIT_INDICES && SCENE_HAS_32BIT_INDICES
    bool read16Bit = gsUse16BitIndices;
#elif SCENE_HAS_16BIT_INDICES
    bool read16Bit = true;
#else
    bool read16Bit = false;
#endif
    // Load the vertex indices.
    uint baseIndex = ibOffset * 4;
    uint vtxIndex;
    if (read16Bit) {
        baseIndex += index * 2;
        vtxIndex = (uint)gScene.indexData.Load<uint16_t>(baseIndex);
    } else {
        baseIndex += index * 4;
        vtxIndex = gScene.indexData.Load<uint>(baseIndex);
    }
#else // !SCENE_HAS_INDEXED_VERTICES
    vtxIndex = index;
#endif
    return vtxIndex;
}

void aux33(float2 uv, float4 o) {
    int2 px = uv;
    gVBufferSW.writeAUX(px + int2(-1, -1), o);
    gVBufferSW.writeAUX(px + int2(-1, 0), o);
    gVBufferSW.writeAUX(px + int2(0, -1), o);
    gVBufferSW.writeAUX(px, o);
    gVBufferSW.writeAUX(px + int2(1, 1), o);
    gVBufferSW.writeAUX(px + int2(0, 1), o);
    gVBufferSW.writeAUX(px + int2(1, 0), o);
    gVBufferSW.writeAUX(px + int2(-1, 1), o);
    gVBufferSW.writeAUX(px + int2(1, -1), o);
}

void rasterizeTriangleIndicesAUX(uint triangle_id) {
    if(triangle_id >= gsMicroTrianglesCount) return;
    const MicroTriangle mt = gsMTArray[triangle_id];

    float2 p0 = transformPointToScreen(mt.pw0).xy;
    float2 p1 = transformPointToScreen(mt.pw1).xy;
    float2 p2 = transformPointToScreen(mt.pw2).xy;

    float2 d0 = normalize((p1 + p2) * .5f - p0) * 6;
    float2 d1 = normalize((p0 + p2) * .5f - p1) * 6;
    float2 d2 = normalize((p0 + p1) * .5f - p2) * 6;

    uint i0 = gScene.meshletVertices[meshlet.vertexOffset + triangle_id * 3 + 0];
    uint i1 = gScene.meshletVertices[meshlet.vertexOffset + triangle_id * 3 + 1];
    uint i2 = gScene.meshletVertices[meshlet.vertexOffset + triangle_id * 3 + 2];

    uint v0 = getLocalIndix(gsMeshDesc.ibOffset, i0);
    uint v1 = getLocalIndix(gsMeshDesc.ibOffset, i1);
    uint v2 = getLocalIndix(gsMeshDesc.ibOffset, i2);

    aux33(p0 + d0, float4(v0, mt.srcPrimID * 3 + 0, 1, 1));
    aux33(p1 + d1, float4(v1, mt.srcPrimID * 3 + 1, 1, 1));
    aux33(p2 + d2, float4(v2, mt.srcPrimID * 3 + 2, 1, 1));
}

MicroPatch3 buildPatch3(uint triangle_id, uint tri_buff_id) {
    MicroPatch3 patch;
    MicroTriangle mt = gsMTArray[tri_buff_id];

    float3 src_pv[3] = {mt.pw0, mt.pw1, mt.pw2};

    patch.pe = {(src_pv[0] + src_pv[1]) * .5f, (src_pv[1] + src_pv[2]) * .5f, (src_pv[2] + src_pv[0]) * .5f};
    patch.pv = src_pv;
    
    const uint8_t3 indices = gScene.meshletLocalIndexData.Load<uint8_t3>(meshlet.localIndexOffset + triangle_id * 3u);

    float3 src_nv[3] = {
        normalize(mul(vertex_data[uint(indices[0])].normal, worldInvTransposeMat)),
        normalize(mul(vertex_data[uint(indices[1])].normal, worldInvTransposeMat)),
        normalize(mul(vertex_data[uint(indices[2])].normal, worldInvTransposeMat))
    };

    uint2 pair0 = gScene.meshNeighborVerticesMap[gsMeshDesc.subdivDataOffset + gScene.meshletVertices[meshlet.vertexOffset + triangle_id * 3 + 2]];
    uint2 pair1 = gScene.meshNeighborVerticesMap[gsMeshDesc.subdivDataOffset + gScene.meshletVertices[meshlet.vertexOffset + triangle_id * 3 + 1]];
    uint2 pair2 = gScene.meshNeighborVerticesMap[gsMeshDesc.subdivDataOffset + gScene.meshletVertices[meshlet.vertexOffset + triangle_id * 3 + 0]];
    
    uint count0  = pair0[0];
    uint offset0 = pair0[1];
    
    uint count1  = pair1[0];
    uint offset1 = pair1[1];

    uint count2  = pair2[0];
    uint offset2 = pair2[1];

    uint d_idx0 = gScene.meshNeighborVertices[offset0];
    uint d_idx1 = gScene.meshNeighborVertices[offset1];
    uint d_idx2 = gScene.meshNeighborVertices[offset2];
    
    uint valence[3] = {count0, count1, count2};

    float3 pv_sum[3] = {float3(0.f), float3(0.f), float3(0.f)};

    if(count0 > 0 && d_idx0 != kInvalid) {
        StaticVertexData vtx_d = gScene.getVertex(gsMeshDesc.vbOffset + d_idx0);
        pv_sum[0] = mul(float4(vtx_d.position, 1.f), worldMat).xyz;
        patch.pe[2] = (src_pv[2] + src_pv[0]) * k38 + (src_pv[1] + pv_sum[0]) * k18;
    } else {
        valence[0]--;
    }

    if(count1 > 0 && d_idx1 != kInvalid) {
        StaticVertexData vtx_d = gScene.getVertex(gsMeshDesc.vbOffset + d_idx1);
        pv_sum[1] = mul(float4(vtx_d.position, 1.f), worldMat).xyz;
        patch.pe[0] = (src_pv[0] + src_pv[1]) * k38 + (src_pv[2] + pv_sum[1]) * k18;
    } else {
        valence[1]--;
    }

    if(count2 > 0 && d_idx2 != kInvalid) {
        StaticVertexData vtx_d = gScene.getVertex(gsMeshDesc.vbOffset + d_idx2);
        pv_sum[2] = mul(float4(vtx_d.position, 1.f), worldMat).xyz;
        patch.pe[1] = (src_pv[1] + src_pv[2]) * k38 + (src_pv[0] + pv_sum[2]) * k18;
    } else {
        valence[2]--;
    }

    float3 nv[3];

    if(count0 < 2) {
        patch.pv[0] = src_pv[0];
    } else {
        float kb = valence[0] == 3 ? k316 : (k58 - pow(k38 + 0.25f * cos(M_2PI / valence[0]), 2)) / valence[0];
        pv_sum[0] *= kb;
        for(uint c = 1; c < count0; ++c) {
            uint idx = gScene.meshNeighborVertices[offset0 + c];
            StaticVertexData vtx = gScene.getVertex(gsMeshDesc.vbOffset + idx);
            //StaticVertexData vtxn = gScene.getVertex(gsMeshDesc.vbOffset + (idx + 1));
            pv_sum[0] += mul(float4(vtx.position, 1.f), worldMat).xyz * kb;
        }
        patch.pv[0] = pv_sum[0] + (src_pv[0] * (1.f - kb * valence[0]));
    }

    if(count1 < 2) {
        patch.pv[1] = src_pv[1];
    } else {
        float kb = valence[1] == 3 ? k316 : (k58 - pow(k38 + 0.25f * cos(M_2PI / valence[1]), 2)) / valence[1];
        pv_sum[1] *= kb;
        for(uint c = 1; c < count1; ++c) {
            uint idx = gScene.meshNeighborVertices[offset1 + c];
            StaticVertexData vtx = gScene.getVertex(gsMeshDesc.vbOffset + idx);
            pv_sum[1] += mul(float4(vtx.position, 1.f), worldMat).xyz * kb;
        }
        patch.pv[1] = pv_sum[1] + (src_pv[1] * (1.f - kb * valence[1]));
    }

    if(count2 < 2) {
        patch.pv[2] = src_pv[2];
    } else {
        float kb = valence[2] == 3 ? k316 : (k58 - pow(k38 + 0.25f * cos(M_2PI / valence[2]), 2)) / valence[2];
        pv_sum[2] *= kb;
        for(uint c = 1; c < count2; ++c) {
            uint idx = gScene.meshNeighborVertices[offset2 + c];
            StaticVertexData vtx = gScene.getVertex(gsMeshDesc.vbOffset + idx);
            pv_sum[2] += mul(float4(vtx.position, 1.f), worldMat).xyz * kb;
        }
        patch.pv[2] = pv_sum[2] + (src_pv[2] * (1.f - kb * valence[2]));
    }

    patch.nv = nv;

    patch.ne[0] = normalize((patch.nv[0] + patch.nv[1]) * .5f);
    patch.ne[1] = normalize((patch.nv[1] + patch.nv[2]) * .5f);
    patch.ne[2] = normalize((patch.nv[2] + patch.nv[0]) * .5f);
    
    return patch;
}

void nullSubdivideTriangle(uint triangle_id) {
    if(triangle_id >= gsPrimCount) return;

    uint8_t3 indices = gScene.meshletLocalIndexData.Load<uint8_t3>(meshlet.localIndexOffset + triangle_id * 3u);
    
    MicroTriangle mt;
    mt.pw0 = mul(float4(vertex_data[uint(indices[0])].position, 1.f), worldMat).xyz;
    mt.pw1 = mul(float4(vertex_data[uint(indices[1])].position, 1.f), worldMat).xyz;
    mt.pw2 = mul(float4(vertex_data[uint(indices[2])].position, 1.f), worldMat).xyz;

    mt.b0 = float2(0.f);
    mt.b1 = float2(1.f, 0.f);
    mt.b2 = float2(0.f, 1.f);
    mt.esl = microTriangleEdgesLengthsSquared(mt.pw0, mt.pw1, mt.pw2);
    mt.flags = (uint8_t)((uint)(gsSubdivide ? MicroTriangleFlags::Subdividable : MicroTriangleFlags::None));
    mt.lod = 0;
    mt.srcPrimID = gScene.meshletPrimIndices[meshlet.primIndexOffset + triangle_id];

    uint write_id;
    InterlockedAdd(gsMicroTrianglesCount, 1, write_id); 
    gsMTArray[write_id] = mt;
}

void subdivideTriangleUniform(uint triangle_id, uint8_t lod) {
    if(triangle_id >= gsMicroTrianglesCount) return;

    const uint src_idx = triangle_id;
    const MicroTriangle src = gsMTArray[src_idx];

    if(src.lod == gVBufferSW.kMaxLOD || src.lod == lod) return;
    
    const float3 pw0 = src.pw0;
    const float3 pw1 = src.pw1;
    const float3 pw2 = src.pw2;

    const float3 pw01 = (pw0 + pw1) * .5f;
    const float3 pw02 = (pw0 + pw2) * .5f;
    const float3 pw12 = (pw1 + pw2) * .5f;

    float16_t3 esl_a = microTriangleEdgesLengthsSquared(pw0, pw01, pw02);
    float16_t3 esl_b = microTriangleEdgesLengthsSquared(pw01, pw1, pw12);
    float16_t3 esl_c = microTriangleEdgesLengthsSquared(pw12, pw2, pw02);

    if(!edgesCanFit(esl_a) || !edgesCanFit(esl_b) || !edgesCanFit(esl_c)) return;

    const float2 b0 = src.b0;
    const float2 b1 = src.b1;
    const float2 b2 = src.b2;

    const float2 b01 = (b0 + b1) * .5f;
    const float2 b02 = (b0 + b2) * .5f;
    const float2 b12 = (b1 + b2) * .5f;

    const uint8_t _lod = src.lod + 1;

    MicroTriangle mt;
    mt.lod = _lod;
    mt.flags = src.flags;
    mt.srcPrimID = src.srcPrimID;

    // replace original triangle with subdivided part

    mt.pw0 = pw0;
    mt.pw1 = pw01;
    mt.pw2 = pw02;
    mt.b0 = b0;
    mt.b1 = b01;
    mt.b2 = b02;
    mt.esl = esl_a;

    gsMTArray[src_idx] = mt;

    // add other three subdivided triangle parts

    mt.pw0 = pw01;
    mt.pw1 = pw1;
    mt.pw2 = pw12;
    mt.b0 = b01;
    mt.b1 = b1;
    mt.b2 = b12;
    mt.esl = esl_b;

    uint write_id;
    InterlockedAdd(gsMicroTrianglesCount, 1, write_id);
    gsMTArray[write_id] = mt;

    mt.pw0 = pw12;
    mt.pw1 = pw2;
    mt.pw2 = pw02;
    mt.b0 = b12;
    mt.b1 = b2;
    mt.b2 = b02;
    mt.esl = esl_c;

    InterlockedAdd(gsMicroTrianglesCount, 1, write_id);
    gsMTArray[write_id] = mt;

    // last central triangle

    mt.pw0 = pw02;
    mt.pw1 = pw01;
    mt.pw2 = pw12;
    mt.b0 = b02;
    mt.b1 = b01;
    mt.b2 = b12;
    mt.esl = {esl_a[1], esl_b[2], esl_c[2]};

    InterlockedAdd(gsMicroTrianglesCount, 1, write_id);
    gsMTArray[write_id] = mt;
}

void rasterizeScanLines(uint thread_id, bool cullCCW) {
    const uint y_d = THREADS_COUNT;
    const uint y_start = (thread_id + gsOffsetY) % THREADS_COUNT;

    SampleGenerator lsg = SampleGenerator(gVBufferSW.frameDim, gVBufferSW.sampleNumber);

    for(uint micro_triangle_id = 0; micro_triangle_id < gsMicroTrianglesCount; ++micro_triangle_id) {

        MicroTriangle tri = gsMTArray[micro_triangle_id];

        #if defined(USE_HIGHP_DEPTH)
            double4 pd0 = transformPointToScreenDouble(tri.pw0);
            double4 pd1 = transformPointToScreenDouble(tri.pw1);
            double4 pd2 = transformPointToScreenDouble(tri.pw2);
            
            double dz0 = pd0.z;
            double dz1 = pd1.z;
            double dz2 = pd2.z;

            float4 pt0 = float4(pd0);
            float4 pt1 = float4(pd1);
            float4 pt2 = float4(pd2);
        #else
            float4 pt0 = transformPointToScreen(tri.pw0);
            float4 pt1 = transformPointToScreen(tri.pw1);
            float4 pt2 = transformPointToScreen(tri.pw2);
        #endif

        // near-far planes rejection
        if((pt0.z < 0.0f && pt1.z < 0.0f && pt2.z < 0.0f) || (pt0.z > 1.0f && pt1.z > 1.0f && pt2.z > 1.0f)) continue;
        
        float4 pt10 = pt1 - pt0;
        float4 pt20 = pt2 - pt0;

        float det = pt20.x * pt10.y - pt20.y * pt10.x;

#if (COMPUTE_DEPTH_OF_FIELD == 0)
        // CCW culling
#if CULL_MODE == CULL_BACK
        if(det < 0.0f) continue;
#elif CULL_MODE == CULL_FRONT
        if(det > 0.0f) continue;
#endif
#endif

        #if (COMPUTE_DEPTH_OF_FIELD == 0)
            float c0 = .0f, c1 = .0f, c2 = .0f;
        #else
            float c0 = abs(1.0f - gScene.camera.data.focalDistance / pt0.w) * gScene.camera.data.apertureRadius * gScene.camera.data.focalLength;
            float c1 = abs(1.0f - gScene.camera.data.focalDistance / pt1.w) * gScene.camera.data.apertureRadius * gScene.camera.data.focalLength;
            float c2 = abs(1.0f - gScene.camera.data.focalDistance / pt2.w) * gScene.camera.data.apertureRadius * gScene.camera.data.focalLength;
        #endif

        int2 bb_min = int2(floor(min(min(pt0.x - c0, pt1.x - c1), pt2.x - c2) - jitterBound.x), floor(min(min(pt0.y - c0, pt1.y - c1), pt2.y - c2) - jitterBound.y));
        int2 bb_max = int2(ceil(max(max(pt0.x + c0, pt1.x + c1), pt2.x + c2) + jitterBound.x), ceil(max(max(pt0.y + c0, pt1.y + c1), pt2.y + c2) + jitterBound.y));

        // discard offscreen
        if ((bb_max.x < 0 && bb_min.x < 0) || (bb_max.y < 0 && bb_min.y < 0) ||
            (bb_max.x >= (gVBufferSW.frameDim.x) && bb_min.x >= (gVBufferSW.frameDim.x)) ||
            (bb_max.y >= (gVBufferSW.frameDim.y) && bb_min.y >= (gVBufferSW.frameDim.y))) continue;

        uint2 bb_min_u = uint2(clamp(bb_min, int2(0, 0), int2(gVBufferSW.frameDim - int2(1, 1))));
        uint2 bb_max_u = uint2(clamp(bb_max, int2(0, 0), int2(gVBufferSW.frameDim - int2(1, 1))));

        bb_min_u.y -= 1;
        bb_max_u.y += 1;

        for(uint y = y_start; y < gVBufferSW.frameDim.y; y += y_d) {
            //if(y < bb_min_u.y || y > bb_max_u.y) continue;

            uint lock_idx = y << 2;
            
            /*
            uint s=0;
            while (true && (s < 10)) {
                uint memLockValue;

                gThreadLockBuffer.InterlockedCompareExchange(lock_idx, 0, 4294967295u, memLockValue);
        
                if (memLockValue == 0) {
                    break;
                }
                s++;
            }
            */

            for(uint x = bb_min_u.x; x <= bb_max_u.x; ++x) {

                // Jitter (noise) [0, 1] range
                float4 jitter = gJitterTexture[uint2(x % gVBufferSW.jitterTextureDim.x, y % gVBufferSW.jitterTextureDim.y)].rgba;
                
                // Anti aliasing jitter [-0.5, 0.5] range 
                float2 aaj = 0.f; //jitter.xy - float2(0.5f, 0.5f);
                float3 pj0 = pt0.xyz + float3(aaj, .0), pj1 = pt1.xyz + float3(aaj, .0), pj2 = pt2.xyz + float3(aaj, .0);

        #if (COMPUTE_DEPTH_OF_FIELD == 1)
            float2 coc_sample = sample_disk(jitter.zw); // range [-1, 1]
            
            pj0.xy += coc_sample * c0; 
            pj1.xy += coc_sample * c1; 
            pj2.xy += coc_sample * c2;
            
            // CCW culling
            #if CULL_MODE == CULL_BACK
            if(cullCCW && cross(float3(pj1.xy - pj0.xy, .0), float3(pj2.xy - pj0.xy, .0)).z > 0.0f) continue;
            #elif CULL_MODE == CULL_FRONT
            if(cullCCW && cross(float3(pj1.xy - pj0.xy, .0), float3(pj2.xy - pj0.xy, .0)).z < 0.0f) continue;
            #endif
        #endif

                float2 uv = triIntersect(float2(x, y), pj0.xy, pj1.xy, pj2.xy);
                if( uv.x < 0.f) continue;  
                
                #if defined(USE_HIGHP_DEPTH)
                    double z = (dz1 - dz0) * uv.x + (dz2 - dz0) * uv.y + dz0;
                #else
                    float z = pt10.z * uv.x + pt20.z * uv.y + pt0.z;
                #endif
                if(z < 0.f || z > 1.f) continue;

                uint2 pixelPos = uint2(x, y);

    #if defined(USE_HIGHP_DEPTH)
                const uint64_t depth_value = z * UINT64_MAX;
                const uint idx = (gVBufferSW.frameDim.x * y + x) << 3;       
                if ( depth_value < gLocalDepthBuffer.InterlockedMinU64(idx, depth_value)) {
    #else
                const uint32_t depth_value = z * kInvalid;
                const uint idx = (gVBufferSW.frameDim.x * y + x) << 2;
                uint32_t out_value = depth_value;
                gLocalDepthBuffer.InterlockedMin(idx, depth_value, out_value);
                if ( depth_value < out_value) {
    #endif

                    TriangleHit triangleHit = {};
                    triangleHit.instanceID.index = instanceID.index;
                    triangleHit.primitiveIndex = tri.srcPrimID;
                    triangleHit.barycentrics = tri.b0 + uv.x * (tri.b1 - tri.b0) + uv.y * (tri.b2 - tri.b0);

                    gVBufferSW.writeHit(pixelPos, HitInfo(triangleHit.pack()));
                    gVBufferSW.writeDepth(pixelPos, z);

                #if is_valid(gNormW)
                    if(gsSubdivide) {
                        float3 normalW = tri.getNormalW(uv);
                        float3 faceNormalW = tri.getFaceNormalW();

                        uint3 combinedNormal = {
                            packSnorm16(faceNormalW.x) | (floatToSnorm16(normalW.x) << 16),
                            packSnorm16(faceNormalW.y) | (floatToSnorm16(normalW.y) << 16),
                            packSnorm16(faceNormalW.z) | (floatToSnorm16(normalW.z) << 16)
                        };
                        gVBufferSW.writeNormal(pixelPos, combinedNormal);
                    }
                #endif

                    gVBufferSW.writeMeshletID(pixelPos, draw.meshletID);
                    gVBufferSW.writeMicroPolyID(pixelPos, (tri.srcPrimID * MAX_MICRO_TRIANGLES) + micro_triangle_id);
                    

                #if is_valid(gDrawCount)
                    InterlockedAdd(gDrawCount[pixelPos], 1);
                #endif
                }
            }
            /*
            gThreadLockBuffer.Store(lock_idx, 0);
            //uint memLockValue;
            //gThreadLockBuffer.InterlockedCompareExchange(lock_idx, kInvalid, 0, memLockValue);
            */
        }
    }
}

[numthreads(THREADS_COUNT, 1, 1)]
void rasterize(uint3 groupID : SV_GroupID, uint groupIdx : SV_GroupIndex) {
    const uint group_id = groupID.y * gVBufferSW.dispatchX + groupID.x;       // Sequential group index.
    const uint thread_id = groupIdx;                                          // Local thread index in group (range 0..THREADS_COUNT-1).
    const uint global_id = group_id * THREADS_COUNT + thread_id;              // Global element index

    // Thread global data
    gThreadID = thread_id;

    // Early termination
    [[branch]] if (group_id >= gVBufferSW.meshletDrawsCount) return;

    /// Group shared data stage
    gsOffsetY = group_id;
    gsMicroTrianglesCount = 0;
    gsCullCCW = true;

    // meshlet parameters
    if(thread_id == 0u) {
        draw = gMeshletDrawList[group_id];
        if(draw.drawCount > 0u) {
            instanceID.index = draw.instanceID;
            instance = gScene.getGeometryInstance(instanceID);
            gsMeshDesc = gScene.getMeshDesc(instanceID);
            worldMat = gScene.getWorldMatrix(instanceID);
            worldInvTransposeMat = gScene.getInverseTransposeWorldMatrix(instanceID);
            meshlet = gScene.meshlets[draw.meshletID].unpack();
            gsPrimCount = meshlet.primCount;
            gsUse16BitIndices = gsMeshDesc.flags & uint(MeshFlags::Use16BitIndices);
            gsSubdivide = instance.isSubdividable() && (MAX_LOD > 0);
        }  
    }

    GroupMemoryBarrierWithGroupSync();

    [[branch]] if(draw.drawCount == 0u) return;

    /// Fetch stage

    // Fetch vertex data
    const uint vi0 = thread_id;
    const uint vi1 = thread_id + THREADS_COUNT;
    if(vi0 < meshlet.vertexCount) {
        vertex_data[vi0] = gScene.getVertex(gsMeshDesc.vbOffset + gScene.meshletVertices[meshlet.vertexOffset + vi0]);
    }

    // Fetch more vertex data if needed
    if(vi1 < meshlet.vertexCount) {
        vertex_data[vi1] = gScene.getVertex(gsMeshDesc.vbOffset + gScene.meshletVertices[meshlet.vertexOffset + vi1]);        
    }

    GroupMemoryBarrierWithGroupSync();

    /// Work balance stage

    //TODO: brick triangles if there are less primitives than threads available

    [[branch]] if(gsSubdivide) {
        for(uint t = 0; t < gsPrimCount; ++t) {
            if(thread_id == 0u) {
                gsMicroTrianglesCount = 0;
                nullSubdivideTriangle(t);
                gsPatch = buildPatch3(t, 0);
            }

            uint lod = 1;
            if(thread_id < pow(2, (lod-1)*2)) subdivideTriangleUniform(thread_id, lod);
            GroupMemoryBarrierWithGroupSync();

            lod = 2;
            if(thread_id < pow(2, (lod-1)*2)) subdivideTriangleUniform(thread_id, lod);
            GroupMemoryBarrierWithGroupSync();

            lod = 3;
            if(thread_id < pow(2, (lod-1)*2)) subdivideTriangleUniform(thread_id, lod);
            GroupMemoryBarrierWithGroupSync();
                
            lod = 4;
            if(thread_id < pow(2, (lod-1)*2)) subdivideTriangleUniform(thread_id, lod);
            GroupMemoryBarrierWithGroupSync();

            // deform

            if(vi0 < gsMicroTrianglesCount) {
                float2 b0 = gsMTArray[vi0].b0, b1 = gsMTArray[vi0].b1, b2 = gsMTArray[vi0].b2;
                gsMTArray[vi0].pw0 = gsPatch.P(b0);
                gsMTArray[vi0].pw1 = gsPatch.P(b1);
                gsMTArray[vi0].pw2 = gsPatch.P(b2);

                gsMTArray[vi0].nw0 = gsPatch.N(b0);
                gsMTArray[vi0].nw1 = gsPatch.N(b1);
                gsMTArray[vi0].nw2 = gsPatch.N(b2);
            }

            if(vi1 < gsMicroTrianglesCount) {
                float2 b0 = gsMTArray[vi1].b0, b1 = gsMTArray[vi1].b1, b2 = gsMTArray[vi1].b2;
                gsMTArray[vi1].pw0 = gsPatch.P(b0);
                gsMTArray[vi1].pw1 = gsPatch.P(b1);
                gsMTArray[vi1].pw2 = gsPatch.P(b2);

                gsMTArray[vi1].nw0 = gsPatch.N(b0);
                gsMTArray[vi1].nw1 = gsPatch.N(b1);
                gsMTArray[vi1].nw2 = gsPatch.N(b2);
            }

            GroupMemoryBarrierWithGroupSync();
            rasterizeScanLines(thread_id, gsCullCCW);
            GroupMemoryBarrierWithGroupSync();
        }
    } else {
        //rasterize_no_subdiv();

        nullSubdivideTriangle(thread_id);
        GroupMemoryBarrierWithGroupSync();
        rasterizeScanLines(thread_id, gsCullCCW);
        GroupMemoryBarrierWithGroupSync();
    }

    //gVBufferSW.writeAUX({0, 0}, float4(WaveGetLaneCount(), 0, 0, 1.f));
    //gVBufferSW.writeAUX({10, group_id}, float4(group_id, 0, 0, 1.f));
}
