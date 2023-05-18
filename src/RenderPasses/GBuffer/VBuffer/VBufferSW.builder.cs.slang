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
import VBufferSW;

#ifndef GROUP_SIZE
#deine  GROUP_SIZE 128
#endif

RWStructuredBuffer<Meshlet> gOutMeshlets;

groupshared uint num_triangles; // triangles
groupshared uint num_vertices;
groupshared uint base_index;
groupshared uint base_vertex;

groupshared MeshletDraw draw;
groupshared Meshlet meshlet;

groupshared uint   indices[MESHLET_MAX_ELEMENTS_COUNT * 3u];
groupshared float3 positions[MESHLET_MAX_ELEMENTS_COUNT];


// Build meshlets from specs, one meshlet per thread

[numthreads(GROUP_SIZE, 1, 1)]
void build(uint3 groupID : SV_GroupID, uint groupIdx : SV_GroupIndex) {
    const uint group_id = groupID.y * gVBufferSW.dispatchX + groupID.x;         // Sequential group index.
    const uint thread_id = groupIdx;                                            // Local thread index in group (range 0..MESHLET_MAX_ELEMENTS_COUNT-1).
    const uint global_id = group_id * MESHLET_MAX_ELEMENTS_COUNT + thread_id;    // Global element index
    
    // Early termination
    [[branch]] if (group_id >= gVBufferSW.meshletDrawsCount) return;
    
    // meshlet parameters
    [[branch]] if(thread_id == 0u) {
      draw = gMeshletDrawList[group_id];
      if(draw.drawCount == 0u) return;
      meshlet = gMeshlets[draw.meshletID];
    }
    GroupMemoryBarrier();
    
    //if (thread_id >= meshlet.vertex_count) return;

    const bool cullCCW = false;

    let vtx = gScene.getVertex(global_id);

    rasterizePoint(transformPoint(vtx.position));

    // Test draws
    rasterizeTriangle(float3(group_id*4, thread_id*4, 0.1), float3(group_id*4 + 4, thread_id*4, 0.1), float3(group_id*4, thread_id*4 + 4, 0.1), cullCCW);
}
