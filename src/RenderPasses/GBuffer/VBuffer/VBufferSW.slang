/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
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
__exported import Scene.Scene;
__exported import Scene.Shading;
import Utils.Math.MathHelpers;
import Utils.Sampling.SampleGenerator;

#include "VBufferSW.Meshlet.slangh"

RWTexture2D<PackedHitInfo> gVBuffer;
RWByteAddressBuffer gLocalDepthPrimBuffer;
RWByteAddressBuffer gLocalDepthParmBuffer;
RWByteAddressBuffer gLocalDepthInstBuffer;

// Meshlets related buffers
StructuredBuffer<MeshletDraw> gMeshletDrawList;

// Optional buffers
RWTexture2D<float>  gDepth;
RWTexture2D<float2> gMotionVector;
RWTexture2D<float4> gViewW;
RWTexture2D<float4> gMeshlet;
RWTexture2D<uint> gTime;

#define is_valid(name) (is_valid_##name != 0)

//#if !defined(COMPUTE_DEPTH_OF_FIELD) || !defined(USE_ALPHA_TEST)
//#error "Not all defines are set!"
//#endif

#if !defined(COMPUTE_DEPTH_OF_FIELD) 
#define COMPUTE_DEPTH_OF_FIELD false
#endif

#if !defined(USE_ALPHA_TEST)
#define USE_ALPHA_TEST false
#endif

struct VBufferSW {
    static const bool kComputeDepthOfField = COMPUTE_DEPTH_OF_FIELD;
    static const bool kUseAlphaTest = USE_ALPHA_TEST;
    
    uint2 frameDim;
    float2 frameDimF;
    uint dispatchX;
    uint meshletDrawsCount;

    void writeHit(uint2 pixel, const HitInfo hit) {
        gVBuffer[pixel] = hit.getData();

        VertexData v;
        float depth = 1.f;
        float3 prevPosW = float3(0.f);
        float2 motionVector = float2(0.f);

        if (hit.getType() == HitType::Triangle) {
            
            const TriangleHit triangleHit = hit.getTriangleHit();
            v = gScene.getVertexData(triangleHit);
            prevPosW = gScene.getPrevPosW(triangleHit);

        }

        if (is_valid(gDepth)) {
            // Compute depth similar to raster (NDC).
            float4 curPosH = mul(float4(v.posW, 1.f), gScene.camera.data.viewProjMatNoJitter);
            gDepth[pixel] = curPosH.z / curPosH.w;
        }

        if (is_valid(gMotionVector)) {
            // Compute motion vector.
            float2 pixelPos = pixel + float2(0.5f, 0.5f);
            float4 prevPosH = mul(float4(prevPosW, 1.f), gScene.camera.data.prevViewProjMatNoJitter);
            gMotionVector[pixel] = calcMotionVector(pixelPos, prevPosH, frameDim) + float2(gScene.camera.data.jitterX, -gScene.camera.data.jitterY); // Remove camera jitter from motion vector
        }
    }

    void writeMiss(uint2 pixel) {
        gVBuffer[pixel] = uint4(0);

        if (is_valid(gDepth))           gDepth[pixel]           = 1.f;
        if (is_valid(gMotionVector))    gMotionVector[pixel]    = {};
    }
};
