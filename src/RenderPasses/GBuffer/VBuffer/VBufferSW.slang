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
__exported import Utils.Math.Ray;
__exported import Utils.Timing.GpuTimer;

import Utils.Math.MathHelpers;
import Utils.Sampling.SampleGenerator;

#include "VBufferSW.common.slangh"
#include "VBufferSW.Meshlet.slangh"

#define is_valid(name) (is_valid_##name != 0)

RWTexture2D<PackedHitInfo> gVBuffer;

RWByteAddressBuffer gLocalDepthBuffer;

// Meshlets related buffers
StructuredBuffer<MeshletDraw> gMeshletDrawList;

// Optional buffers
RWTexture2D<float>  gDepth;
RWTexture2D<float2> gMotionVector;
RWTexture2D<float4> gViewW;
RWTexture2D<uint4>  gNormW;
RWTexture2D<float4> gTextureGrads;
RWTexture2D<uint>   gOITStartOffset;

RWTexture2D<uint>   gMeshletID;
RWTexture2D<uint>   gMicroPolyID;

// Optional debug buffers
RWTexture2D<uint>   gDrawCount; // Number of writes into visibility buffer
RWTexture2D<float4> gAUX;       // Just a four channel auxiliary debug buffer for whatever
RWTexture2D<uint>   gTime;      // Timing debug buffer

#if !defined(COMPUTE_DEPTH_OF_FIELD) || !defined(USE_ALPHA_TEST)
#error "Not all defines are set!"
#endif

#if !defined(USE_ALPHA_TEST)
#define USE_ALPHA_TEST false
#endif

#if !defined(USE_SUBDIVISIONS)
#define USE_SUBDIVISIONS false
#endif

#if !defined(MAX_LOD)
#define MAX_LOD 3
#endif

#if !defined(USE_PP_JITTER)
#define USE_PP_JITTER false
#endif

struct VBufferSW {
    static const bool kComputeDepthOfField = COMPUTE_DEPTH_OF_FIELD;
    static const bool kUseAlphaTest = USE_ALPHA_TEST;
    static const bool kUseSubdivisions = USE_SUBDIVISIONS;
    static const uint kMaxLOD = MAX_LOD;
    static const bool kUsePerPixelJitter = USE_PP_JITTER;

    uint2   frameDim;
    float2  frameDimInv;
    float2  frameDimInv2;
    uint    sampleNumber;
    uint    dispatchX;
    uint    meshletDrawsCount;
    float   minScreenEdgeLen;
    float   minScreenEdgeLenSquared;
    float2  rnd;
    uint2   stbnOffset;
    uint    drawableIndex; // one drawable for debug
    uint    drawableOffset;
    uint2   jitterTextureDim;
    uint    transparencySamplesCount;
    float   opacityLimit;

    void writeMeshletID(uint2 pixelPos, uint meshletID) {
#if is_valid(gMeshletID)
        gMeshletID[pixelPos] = meshletID;
#endif // gMeshletID    
    }

    void writeMicroPolyID(uint2 pixelPos, uint microPolyID) {
#if is_valid(gMicroPolyID)
        gMicroPolyID[pixelPos] = microPolyID;
#endif // gMicroPolyID  
    }

    void writeTexGrads(uint2 pixelPos, float4 grads) {
#if is_valid(gTextureGrads)
        gTextureGrads[pixelPos] = grads;
#endif // gTextureGrads  
    }

    void writeHit(uint2 pixelPos, const HitInfo hit) {
        //GroupMemoryBarrierWithGroupSync();
        gVBuffer[pixelPos] = hit.getData();
    }

void writeDepth(uint2 pixelPos, float depth) {
        
#if is_valid(gDepth)          
        gDepth[pixelPos] = depth;
#endif
    }

    void writeMiss(uint2 pixelPos) {
        gVBuffer[pixelPos] = {};

#if is_valid(gDepth)          
        gDepth[pixelPos] = 1.f;
#endif

#if is_valid(gMotionVector)
        gMotionVector[pixelPos] = {};
#endif
    }

    void writeNormal(uint2 pixelPos, uint3 n) {
        gNormW[pixelPos] = uint4(n, 1);
    }

    void writeAUX(uint2 pixelPos, float4 data) {
#if is_valid(gAUX)
        gAUX[pixelPos] = data;
#endif
    }

    Ray generateRay(uint2 pixel, float4 rnd) {
        if (kComputeDepthOfField) {
            if(kUsePerPixelJitter) {
                return gScene.camera.computeRayThinlens(pixel, frameDim, rnd.xy, rnd.zw);
            } else {
                return gScene.camera.computeRayThinlens(pixel, frameDim, rnd.xy, false);
            }
        } else {
            if(kUsePerPixelJitter) {
                return gScene.camera.computeRayPinhole(pixel, frameDim, rnd.xy);
            } else {
                return gScene.camera.computeRayPinhole(pixel, frameDim, false);
            }
        }
    }

    Ray generateRayMB(uint2 pixel, float4 rnd) {
        const float tj = 0.0f;  

        if (kComputeDepthOfField) {
            if(kUsePerPixelJitter) {
                return gScene.camera.computeRayThinlensMB(pixel, frameDim, rnd.xy, rnd.zw, tj);
            } else {
                return gScene.camera.computeRayThinlensMB(pixel, frameDim, rnd.xy, tj, false);
            }
        } else {
            if(kUsePerPixelJitter) {
                return gScene.camera.computeRayPinholeMB(pixel, frameDim, rnd.xy, tj);
            } else {
                return gScene.camera.computeRayPinholeMB(pixel, frameDim, tj, false);
            }
        }
    }

    void beginTime(inout GpuTimer timer) {
        if (is_valid(gTime)) timer.start();
    }

    void endTime(uint2 pixel, inout GpuTimer timer) {
        if (is_valid(gTime)) gTime[pixel] = timer.getElapsed();
    }
};
