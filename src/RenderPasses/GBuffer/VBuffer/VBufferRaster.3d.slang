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
#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
//import Utils.Math.MathHelpers;
//import Scene.Camera.CameraData;
//import Utils.Sampling.SampleGenerator;

RasterizerOrderedTexture2D<float> gHighpDepth;
RasterizerOrderedTexture2D<float2> gMotionVector;
RasterizerOrderedTexture2D<float4> gTextureGrads;

RasterizerOrderedTexture2D<PackedHitInfo> gVBuffer;

cbuffer PerFrameCB {
    uint2 gFrameDim;
    uint  sampleNumber;
    float tj;
};

#define is_valid(name) (is_valid_##name != 0)

struct VBufferPSOut {
    PackedHitInfo packedHitInfo : SV_TARGET0;
};

#ifndef INTERPOLATION_MODE
#define INTERPOLATION_MODE linear
#endif

#ifndef SCENE_HAS_PERPRIM_MATERIALS
#define SCENE_HAS_PERPRIM_MATERIALS 0
#endif


static float fz = FLT_MAX;

struct VBufferVSOut {
    float2 texC : TEXCRD;

    // Per-triangle data
    nointerpolation GeometryInstanceID instanceID   : INSTANCE_ID;
    nointerpolation uint materialID                 : MATERIAL_ID;

    float3 posW : POSITION;  ///< Position in world space.
    double4 posH : SV_POSITION;  ///< Position in clip space.
    INTERPOLATION_MODE float3 normalW    : NORMAL;      ///< Shading normal in world space.

    INTERPOLATION_MODE float  camZ : CAMZ;

#if is_valid(gMotionVector)
    float4 prevPosH : PREVPOSH; ///< Position in clip space for the previous frame.
#endif
};



VBufferVSOut vsMain(VSIn vsIn) {
    VBufferVSOut vsOut;
    const GeometryInstanceID instanceID = { vsIn.instanceID };

    return vsOut;
}


[earlydepthstencil]
VBufferPSOut psMain(VBufferVSOut vsOut, uint triangleIndex : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics) {
    VBufferPSOut psOut;

    return psOut;
}
