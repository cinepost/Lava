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
#include "VBufferSW.functions.slangh"

import Utils.Math.MathHelpers;
import VBufferSW;

#ifndef THREADS_COUNT
#define THREADS_COUNT MESHLET_MAX_POLYGONS_COUNT
#endif

#ifndef MICROPOLY_MAX_BB_SIZE
#define MICROPOLY_MAX_BB_SIZE 8.f // 4x4 pixel micropoly
#endif

ConstantBuffer<VBufferSW> gVBufferSW;

cbuffer PerFrameCB {
  uint  gMaxEdgeLength; // Target edge length in pixels.
  uint  gMaxEdgeDivs;   // Maximum edge divisions. 
};

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

groupshared uint32_t    indices[MESHLET_MAX_POLYGONS_COUNT * 4];
groupshared uint32_t    prim_ids[MESHLET_MAX_POLYGONS_COUNT];
groupshared float3      positions[MESHLET_MAX_VERTICES_COUNT];
groupshared float       cocs[MESHLET_MAX_VERTICES_COUNT];

RWStructuredBuffer<uint32_t>    gIndicesBuffer;
RWStructuredBuffer<uint32_t>    gPrimIndicesBuffer; 
RWStructuredBuffer<float3>      gPositionsBuffer;
RWStructuredBuffer<float>       gCocsBuffer;    

#define is_valid(name) (is_valid_##name != 0)


struct TriFace {
    float3 p0;
    float3 p1;
    float3 p2;

    __init(float3 p0, float3 p1, float3 p2) {
        this.p0 = p0;
        this.p1 = p1;
        this.p2 = p2;
    }
}

// Computing the subdivision matrix M0 or M1 from a binary value.
float3x3 bitToXform (in uint bit ) {
    float s = float( bit ) - 0.5;
    float3 c1 = float3 ( s , -0.5 , 0);
    float3 c2 = float3 ( -0.5 , -s , 0);
    float3 c3 = float3 (+0.5 , +0.5 , 1);
    return float3x3 (c1 , c2 , c3 );
}

// Key to transformation matrix decoding routine.
float3x3 keyToXform (in uint key ) {
    float3x3 xf = {
                    1, 0, 0,
                    0, 1, 0,
                    0, 0, 1
                  };
    while( key > 1u ) {
        xf = bitToXform( key & 1u) * xf;
        key = key >> 1u;    
    }
    return xf;
}

// Implicit subdivision procedures.
uint parentKey (in uint key ) {
    return ( key >> 1u );
}

void childrenKeys (in uint key , out uint children [2]) {
    children [0] = ( key << 1u) | 0u;
    children [1] = ( key << 1u) | 1u;
}

// Determining if the key represents the 0-child of its parent.
bool isChildZeroKey (in uint key ) { return ( key & 1u == 0u); }


// Determining whether a key is a root key or a leaf key.
bool isRootKey (in uint key ) { return ( key == 1u); }
bool isLeafKey (in uint key ) { return firstbithigh( key ) == 31; }

// barycentric interpolation
float3 berp (in float3 v [3] , in float2 u) {
    return v [0] + u.x * (v [1] - v [0]) + u.y * (v [2] - v [0]) ;
}

void triDivide2(float3 p0, float3 p1, float3 p2, out TriFace[2] faces) {
    float3 p12 = (p1 + p2) * .5;
    faces[0] = TriFace(p0, p1, p12);
    faces[1] = TriFace(p0, p12, p2);
}

void triDivide3(float3 p0, float3 p1, float3 p2, out TriFace[3] faces) {
    float3 p01 = (p0 + p1) * .5;
    float3 p20 = (p2 + p0) * .5;
    faces[0] = TriFace(p0, p01, p20);
    faces[1] = TriFace(p1, p20, p01);
    faces[2] = TriFace(p2, p20, p1);
}

void triDivide4(float3 p0, float3 p1, float3 p2, out TriFace[4] faces) {
    float3 p01 = (p0 + p1) * .5;
    float3 p12 = (p1 + p2) * .5;
    float3 p20 = (p2 + p0) * .5;
    faces[0] = TriFace(p0, p01, p20);
    faces[1] = TriFace(p01, p12, p20);
    faces[2] = TriFace(p2, p20, p12);
    faces[3] = TriFace(p1, p12, p01);
}


[numthreads(THREADS_COUNT, 1, 1)]
void tesselate(uint3 groupID : SV_GroupID, uint groupIdx : SV_GroupIndex) {
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
        const uint8_t3 _indices = gScene.meshletIndexData.Load<uint8_t3>(meshlet.indexOffset + meshletTriangleRootIndex);

        indices[meshletTriangleRootIndex] = _indices[0];
        indices[meshletTriangleRootIndex + 1] = _indices[1];
        indices[meshletTriangleRootIndex + 2] = _indices[2];
    }

    // Transform vertices
    const uint vi0 = thread_id;
    const uint vi1 = thread_id + THREADS_COUNT;
    if(vi0 < meshlet.vertexCount) {
        uint vertexOffset = meshDesc.vbOffset + gScene.meshletVertices[meshlet.vertexOffset + vi0];
        float4 pS = transformPointToScreen(mul(float4(gScene.getVertex(vertexOffset).position, 1.f), worldMat).xyz);
        positions[vi0] = pS.xyz;
#if COMPUTE_DEPTH_OF_FIELD
        cocs[vi0] = abs(pS.w - gScene.camera.data.focalDistance) / pS.w;
#else
        cocs[vi0] = 0.0f;
#endif
    }

    // Transform more vertices if needed
    if(vi1 < meshlet.vertexCount) {
        uint vertexOffset = meshDesc.vbOffset + gScene.meshletVertices[meshlet.vertexOffset + vi1];
        float4 pS = transformPointToScreen(mul(float4(gScene.getVertex(vertexOffset).position, 1.f), worldMat).xyz);
        positions[vi1] = pS.xyz;
#if COMPUTE_DEPTH_OF_FIELD
        cocs[vi1] = abs(pS.w - gScene.camera.data.focalDistance) / pS.w;
#else
        cocs[vi1] = 0.0f;
#endif
    }

    GroupMemoryBarrierWithGroupSync();


    /// Tesselating stage
    [[branch]]if(thread_id >= triangleCount) return;

    const uint meshletTriangleRootIndex = thread_id * 3u;
    float3 p0 = positions[indices[meshletTriangleRootIndex]];
    float3 p1 = positions[indices[meshletTriangleRootIndex + 1]];
    float3 p2 = positions[indices[meshletTriangleRootIndex + 2]];

    float e01 = distance(p0.xy, p1.xy);
    float e12 = distance(p1.xy, p2.xy);
    float e20 = distance(p2.xy, p0.xy);

    uint  div01 = max(floor(e01 / gMaxEdgeLength), gMaxEdgeDivs);
    uint  div12 = max(floor(e12 / gMaxEdgeLength), gMaxEdgeDivs);
    uint  div20 = max(floor(e20 / gMaxEdgeLength), gMaxEdgeDivs);


    // Emit test subdiv
    TriFace faces[4];
    triDivide4(p0, p1, p2, faces);
    
}
