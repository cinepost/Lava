/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
import Scene.Raster;
import Utils.Sampling.SampleGenerator;


cbuffer PerFrameCB {
  uint              gSampleNumber;        // Interframe sample number.
  uint              gSamplesPerFrame;

}

float3 getGeometricFaceNormalW(const GeometryInstanceID instanceID, const uint triangleIndex) {
    uint3 vtxIndices = gScene.getIndices(instanceID, triangleIndex);
    float3 p0 = gScene.vertices[vtxIndices[0]].position;
    float3 p1 = gScene.vertices[vtxIndices[1]].position;
    float3 p2 = gScene.vertices[vtxIndices[2]].position;
    float3 N = cross(p1 - p0, p2 - p0);
    
    //if (isObjectFrontFaceCW(instanceID)) N = -N;
    
    float3x3 worldInvTransposeMat = gScene.getInverseTransposeWorldMatrix(instanceID);
    return normalize(mul(N, worldInvTransposeMat));
}

void main(VSOut vsOut, uint triangleIndex : SV_PrimitiveID) : SV_TARGET {
    //float3 viewDir = normalize(gScene.camera.getPosition() - vsOut.posW);

    // Early discard (backface, phantom, etc)
    //float3 faceNormal = getGeometricFaceNormalW(vsOut.instanceID, triangleIndex);
    //if (dot(viewDir, sd.N) < 0.f) discard;

    // pixel position
    int2 ipos = int2(vsOut.posH.xy);

    // Mesh and material IDs for sampling
    uint geometryID = vsOut.instanceID.index;
    uint materialID = vsOut.materialID;

    // pseudo-random sample generator
    SampleGenerator sg = SampleGenerator.create(ipos + uint2(geometryID, materialID), gSampleNumber);

    // Calculate surface material opacity
    //ShadingData sd = prepareShadingData(vsOut, triangleIndex, viewDir);

    //if (sd.opacity < sampleNext1D(sg)) discard;
    
#ifndef DISABLE_ALPHA_TEST
    // Calling prepareShadingData() to discard pixels that fail alpha test. The pixel shader has no other side effects.
    //prepareShadingData(vsOut, triangleIndex, viewDir);
#endif

}