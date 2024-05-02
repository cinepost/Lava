#include "Scene/SceneDefines.slangh"
#include "Utils/Math/MathConstants.slangh"

import Scene.Camera.CameraData;

#include "VBufferSW.MicroTriangle.slangh"
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

#ifndef MAX_MICRO_TRIANGLES
#define MAX_MICRO_TRIANGLES 256
#endif

#ifndef MAX_LOD
#define MAX_LOD 3
#endif

#define PP_AXIS_SIZE 2

#define is_valid(name) (is_valid_##name != 0)

ConstantBuffer<VBufferSW> gVBufferSW;

Texture2D<float4> gJitterTexture;
SamplerState gJitterSampler;

groupshared uint gsPrimCount;

groupshared float4x4 worldMat;
groupshared GeometryInstanceID instanceID;
groupshared GeometryInstanceData instance;
groupshared MeshDesc meshDesc;
groupshared MeshletDraw draw;
groupshared MeshletData meshlet;

groupshared StaticVertexData    vertex_data[MESHLET_MAX_VERTICES_COUNT];
groupshared bool                gsSubdivide;
groupshared uint                gsGroupID;

RWStructuredBuffer<MicroTriangle> gMicroTrianglesBuffer;

static const float2 jitterBound = float2(0.5f);

static MicroTriangle micro_triangles[MAX_MICRO_TRIANGLES];
static uint gsPrimID;
static uint micro_triangles_count = 0;


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

void nullSubdivideTriangle(uint triangle_id) {
    if(triangle_id >= gsPrimCount) return;

    const uint8_t3 indices = gScene.meshletLocalIndexData.Load<uint8_t3>(meshlet.localIndexOffset + triangle_id * 3u);
    const uint idx = micro_triangles_count;

    micro_triangles[idx].pw0 = mul(float4(vertex_data[uint(indices[0])].position, 1.f), worldMat).xyz;
    micro_triangles[idx].pw1 = mul(float4(vertex_data[uint(indices[1])].position, 1.f), worldMat).xyz;
    micro_triangles[idx].pw2 = mul(float4(vertex_data[uint(indices[2])].position, 1.f), worldMat).xyz;
    micro_triangles[idx].b0 = float2(0.f);
    micro_triangles[idx].b1 = float2(1.f, 0.f);
    micro_triangles[idx].b2 = float2(0.f, 1.f);
    micro_triangles[idx].lod = 0;

    const float16_t3 esl = microTriangleEdgesLengthsSquared(micro_triangles[idx].pw0, micro_triangles[idx].pw1, micro_triangles[idx].pw2);
    micro_triangles[idx].esl = esl;
    micro_triangles[idx].flags = (uint8_t)((uint)(gsSubdivide ? MicroTriangleFlags::Subdividable : MicroTriangleFlags::None));

    micro_triangles_count = 1;
}

void subdivideTriangleUniform(uint triangle_id, uint8_t lod) {
    if(triangle_id >= micro_triangles_count) return;
    if((micro_triangles_count + 4) > MAX_MICRO_TRIANGLES) return;

    const MicroTriangle src = micro_triangles[triangle_id];
    
    if(!src.isSubdividable() || src.lod == gVBufferSW.kMaxLOD || src.lod == lod) return;
    
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

    // replace original triangle with subdivided part
    
    micro_triangles[triangle_id].pw0 = pw0;
    micro_triangles[triangle_id].pw1 = pw01;
    micro_triangles[triangle_id].pw2 = pw02;
    micro_triangles[triangle_id].b0 = b0;
    micro_triangles[triangle_id].b1 = b01;
    micro_triangles[triangle_id].b2 = b02;
    micro_triangles[triangle_id].lod = _lod;
    micro_triangles[triangle_id].flags = src.flags;
    micro_triangles[triangle_id].esl = esl_a;

    // add other three subdivided triangle parts
    
    const uint idx = micro_triangles_count;

    micro_triangles[idx+0].pw0 = pw01;
    micro_triangles[idx+0].pw1 = pw1;
    micro_triangles[idx+0].pw2 = pw12;
    micro_triangles[idx+0].b0 = b01;
    micro_triangles[idx+0].b1 = b1;
    micro_triangles[idx+0].b2 = b12;
    micro_triangles[idx+0].lod = _lod;
    micro_triangles[idx+0].flags = src.flags;
    micro_triangles[idx+0].esl = esl_b;

    micro_triangles[idx+1].pw0 = pw12;
    micro_triangles[idx+1].pw1 = pw2;
    micro_triangles[idx+1].pw2 = pw02;
    micro_triangles[idx+1].b0 = b12;
    micro_triangles[idx+1].b1 = b2;
    micro_triangles[idx+1].b2 = b02;
    micro_triangles[idx+1].lod = _lod;
    micro_triangles[idx+1].flags = src.flags;
    micro_triangles[idx+1].esl = esl_c;

    // last central triangle

    micro_triangles[idx+2].pw0 = pw02;
    micro_triangles[idx+2].pw1 = pw01;
    micro_triangles[idx+2].pw2 = pw12;
    micro_triangles[idx+2].b0 = b02;
    micro_triangles[idx+2].b1 = b01;
    micro_triangles[idx+2].b2 = b12;
    micro_triangles[idx+2].flags = src.flags;
    micro_triangles[idx+2].lod = _lod;
    micro_triangles[idx+1].esl = {esl_a[1], esl_b[2], esl_c[2]};
    
    micro_triangles_count += 3;
}

void rasterizeTriangle(uint triangle_id, bool cullCCW) {
    if(triangle_id >= micro_triangles_count) return;
    
    // bound triangle
    float4 pt0, pt1, pt2;

    const MicroTriangle tri = micro_triangles[triangle_id];

    float3 pw0 = tri.pw0;
    float3 pw1 = tri.pw1;
    float3 pw2 = tri.pw2;

#if defined(USE_HIGHP_DEPTH)
    double4 pd0 = transformPointToScreenDouble(pw0);
    double4 pd1 = transformPointToScreenDouble(pw1);
    double4 pd2 = transformPointToScreenDouble(pw2);
    
    double dz0 = pd0.z;
    double dz1 = pd1.z;
    double dz2 = pd2.z;

    pt0 = float4(pd0);
    pt1 = float4(pd1);
    pt2 = float4(pd2);
#else
    pt0 = transformPointToScreen(pw0);
    pt1 = transformPointToScreen(pw1);
    pt2 = transformPointToScreen(pw2);
#endif

    // near-far planes rejection
    [[branch]] if((pt0.z < 0.0f && pt1.z < 0.0f && pt2.z < 0.0f) || (pt0.z > 1.0f && pt1.z > 1.0f && pt2.z > 1.0f)) return;

    float4 pt10 = pt1 - pt0;
    float4 pt20 = pt2 - pt0;

#if !defined(COMPUTE_DEPTH_OF_FIELD)
    float det = pt20.x * pt10.y - pt20.y * pt10.x;
    // CCW culling
    #if CULL_MODE == CULL_BACK
    [[branch]] if(cullCCW && det < 0.0f) return;
    #elif CULL_MODE == CULL_FRONT
    [[branch]] if(cullCCW && det > 0.0f) return;
    #endif
#endif

#if defined(COMPUTE_DEPTH_OF_FIELD)
    float c0 = abs(1.0f - gScene.camera.data.focalDistance / pt0.w) * gScene.camera.data.apertureRadius * gScene.camera.data.focalLength;
    float c1 = abs(1.0f - gScene.camera.data.focalDistance / pt1.w) * gScene.camera.data.apertureRadius * gScene.camera.data.focalLength;
    float c2 = abs(1.0f - gScene.camera.data.focalDistance / pt2.w) * gScene.camera.data.apertureRadius * gScene.camera.data.focalLength;
#else
    float c0 = .0f, c1 = .0f, c2 = .0f;
#endif

    int2 bb_min = int2(floor(min(min(pt0.x - c0, pt1.x - c1), pt2.x - c2) - jitterBound.x), floor(min(min(pt0.y - c0, pt1.y - c1), pt2.y - c2) - jitterBound.y));
    int2 bb_max = int2(ceil(max(max(pt0.x + c0, pt1.x + c1), pt2.x + c2) + jitterBound.x), ceil(max(max(pt0.y + c0, pt1.y + c1), pt2.y + c2) + jitterBound.y));

    if ((bb_max.x < 0 && bb_min.x < 0) || (bb_max.y < 0 && bb_min.y < 0) ||
        (bb_max.x >= (gVBufferSW.frameDim.x) && bb_min.x >= (gVBufferSW.frameDim.x)) ||
        (bb_max.y >= (gVBufferSW.frameDim.y) && bb_min.y >= (gVBufferSW.frameDim.y))) return;

    uint2 bb_min_u = uint2(clamp(bb_min, int2(0, 0), int2(gVBufferSW.frameDim - int2(1, 1))));
    uint2 bb_max_u = uint2(clamp(bb_max, int2(0, 0), int2(gVBufferSW.frameDim - int2(1, 1))));

    SampleGenerator lsg = SampleGenerator(gVBufferSW.frameDim, gVBufferSW.sampleNumber);

    for(uint y=bb_min_u.y; y<bb_max_u.y; y++) {
        for(uint x=bb_min_u.x; x<bb_max_u.x; x++) {

            // Jitter (noise) [0, 1] range
            float4 jitter = gJitterTexture[uint2(x - 128 * floor( x / 128.0), y - 128 * floor(y / 128.0))];
            
            // Anti aliasing jitter [-0.5, 0.5] range 
            float2 aaj = jitter.xy - float2(0.5);
            float3 pj0 = pt0.xyz + float3(aaj, .0), pj1 = pt1.xyz + float3(aaj, .0), pj2 = pt2.xyz + float3(aaj, .0);

#if defined(COMPUTE_DEPTH_OF_FIELD)
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
            double z = (dz1 - dz0) * uv.x + (dz2 - dz0) * uv.y + dz0;
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
                triangleHit.primitiveIndex = gsPrimID;
                triangleHit.barycentrics = tri.b0 + uv.x * (tri.b1 - tri.b0) + uv.y * (tri.b2 - tri.b0);

                uint2 pixelPos = uint2(x, y);

                gVBufferSW.writeHit(pixelPos, HitInfo(triangleHit.pack()));

#if is_valid(gTextureGrads)
                //float2 tcrd0 = vertex_data[pi0].texCrd, tcrd1 = vertex_data[pi1].texCrd, tcrd2 = vertex_data[pi2].texCrd;
                
                // TODO: fetch appropriate texture coordinates
                float2 tcrd0 = 0, tcrd1 = 0, tcrd2 = 0;

                float4 pndc0 = mul( float4(pw0, 1.f), gScene.camera.getViewProjMat());
                float4 pndc1 = mul( float4(pw1, 1.f), gScene.camera.getViewProjMat());
                float4 pndc2 = mul( float4(pw2, 1.f), gScene.camera.getViewProjMat());
                BarycentricDeriv deriv = calcFullBary(pndc0, pndc1, pndc2, float2(x, y)/float2(gVBufferSW.frameDim), gVBufferSW.frameDimInv2);
                GradientInterpolationResults g = interpolate2DWithDeriv(deriv, tcrd0, tcrd1, tcrd2);

                gVBufferSW.writeTexGrads(pixelPos, float4(clamp(g.dx, float2(-1.0f), float2(1.0f)), clamp(g.dy, float2(-1.0f), float2(1.0f))));
                gVBufferSW.writeTexGrads(pixelPos, float4(gsGroupID));
#endif

                gVBufferSW.writeMeshletID(pixelPos, draw.meshletID);
                gVBufferSW.writeMicroPolyID(pixelPos, gsPrimID + triangle_id);

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

    gsGroupID = groupID.x;

    // Early termination
    [[branch]] if (group_id >= gVBufferSW.meshletDrawsCount) return;

    /// Group shared data stage

    micro_triangles_count = 0;

    // meshlet parameters
    [[branch]] if(thread_id == 0u) {
        draw = gMeshletDrawList[group_id];
        if(draw.drawCount > 0u) {
            instanceID.index = draw.instanceID;
            instance = gScene.getGeometryInstance(instanceID);
            meshDesc = gScene.getMeshDesc(instanceID);
            worldMat = gScene.getWorldMatrix(instanceID);
            meshlet = gScene.meshlets[draw.meshletID].unpack();
            gsPrimCount = meshlet.primCount;
            gsSubdivide = instance.isSubdividable() && (MAX_LOD > 0);
        }  
    }

    GroupMemoryBarrierWithGroupSync();

    [[branch]] if(draw.drawCount == 0u) return;

    /// Fetch stage

    gsPrimID = gScene.meshletPrimIndices[meshlet.primIndexOffset + thread_id];

    // Fetch vertex data
    const uint vi0 = thread_id;
    const uint vi1 = thread_id + THREADS_COUNT;
    if(vi0 < meshlet.vertexCount) {
        vertex_data[vi0] = gScene.getVertex(meshDesc.vbOffset + gScene.meshletVertices[meshlet.vertexOffset + vi0]);
    }

    // Fetch more vertex data if needed
    if(vi1 < meshlet.vertexCount) {
        vertex_data[vi1] = gScene.getVertex(meshDesc.vbOffset + gScene.meshletVertices[meshlet.vertexOffset + vi1]);        
    }

    GroupMemoryBarrierWithGroupSync();

    /// Work balance stage

    //TODO: brick triangles if there are less primitives than threads available

    /// Micro triangles generation

    nullSubdivideTriangle(thread_id);

    /// Micro triangles subdivision
    [[branch]] if(gsSubdivide) {
        for(uint lod = 0; lod <= (MAX_LOD - 1); ++lod) {
            for(uint lod_iter = 0; lod_iter < pow(2, lod*2); ++lod_iter) {
                subdivideTriangleUniform(lod_iter, lod);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    /// Rasterizing stage

    const bool cullCCW = true;

    [[branch]]
    if(gsSubdivide) {
        for(uint i = 0; i < micro_triangles_count; ++i) {
            rasterizeTriangle(i, cullCCW);
        }
    } else {
        rasterizeTriangle(0, cullCCW);
    }
}
