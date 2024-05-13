#include "Scene/SceneDefines.slangh"
#include "Utils/Math/MathConstants.slangh"

#include "VBufferSW.SubdivData.slangh"

__exported import Scene.Scene;

#ifndef THREADS_COUNT
#define THREADS_COUNT 32
#endif

cbuffer PerFrameCB {
  uint gSubdivMeshesCount;
};


RWStructuredBuffer<MeshSubdivData>  gSubdivMeshIndicesBuffer;
RWStructuredBuffer<EvenVertexData>  gEvenVertexValenceBuffer;
RWStructuredBuffer<uint32_t>        gOddVeticesIndexVBuffer;
RWStructuredBuffer<uint32_t>        gEvenVerticesNeighboursIndexBuffer; 


[numthreads(THREADS_COUNT, 1, 1)]
void build(uint3 groupID : SV_GroupID, uint groupIdx : SV_GroupIndex) {
    const uint group_id = groupID.y * gVBufferSW.dispatchX + groupID.x;       // Sequential group index.
    const uint thread_id = groupIdx;                                          // Local thread index in group (range 0..THREADS_COUNT-1).
    const uint global_id = group_id * THREADS_COUNT + thread_id;              // Global element index

    // Early termination
    if (global_id >= gSubdivMeshesCount) return;
}