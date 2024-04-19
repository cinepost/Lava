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
import Utils.Math.MathHelpers;
import Scene.Camera.CameraData;
import Utils.Sampling.SampleGenerator;

RasterizerOrderedTexture2D<float> gHighpDepth;

RasterizerOrderedTexture2D<float4> gTestTexture;

RasterizerOrderedTexture2D<float2> gMotionVector;
RasterizerOrderedTexture2D<float4> gTextureGrads;

RasterizerOrderedTexture2D<PackedHitInfo> gVBuffer;

cbuffer PerFrameCB {
    uint2 gFrameDim;
    uint  sampleNumber;
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

int xorshift(int value) {
    // Xorshift*32
    // Based on George Marsaglia's work: http://www.jstatsoft.org/v08/i14/paper
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value;
}

VBufferVSOut vsMain(VSIn vsIn) {
    VBufferVSOut vsOut;
    const GeometryInstanceID instanceID = { vsIn.instanceID };

    const float tj = abs(frac(float(xorshift(sampleNumber)) / 3141.592653));

#if defined(COMPUTE_MOTION_BLUR)
    float4x4 worldMat = gScene.getWorldMatrix(instanceID, tj);
#else
    float4x4 worldMat = gScene.getWorldMatrix(instanceID);
#endif

    vsOut.posW = mul(float4(vsIn.pos, 1.f), worldMat).xyz;

#if defined(COMPUTE_MOTION_BLUR)
    const CameraXformData xform = gScene.camera.getXformData(tj);
    const float4x4 viewProjMat = xform.viewProjMat;
#else
    const float4x4 viewProjMat = gScene.camera.getViewProjMat();
    
#endif
    
    vsOut.posH = mul(double4((double3)vsOut.posW, 1.0l), (double4x4)viewProjMat);

    vsOut.posH.x += 2.f * gScene.camera.data.jitterX * vsOut.posH.w;
    vsOut.posH.y += 2.f * gScene.camera.data.jitterY * vsOut.posH.w;
    
    const double4 viewP = mul(vsOut.posH, (double4x4)gScene.camera.data.invProjMat);
    vsOut.camZ = -(viewP.z / viewP.w);

    vsOut.texC = float2(vsIn.texU, vsIn.texV);
    vsOut.normalW = normalize(mul(vsIn.unpack().normal, gScene.getInverseTransposeWorldMatrix(instanceID)));

    vsOut.instanceID = instanceID;
    GeometryInstanceData instance = gScene.getGeometryInstance(instanceID);

    vsOut.materialID = gScene.getMaterialID(instanceID);

#if is_valid(gMotionVector)
    // Compute the vertex position in the previous frame.
    float3 prevPos = vsIn.pos;
    
    if (instance.isDynamic()) {
        uint prevVertexIndex = gScene.meshes[instance.geometryID].prevVbOffset + vsIn.vertexID;
        prevPos = gScene.prevVertices[prevVertexIndex].position;
    }

    float3 prevPosW = mul(float4(prevPos, 1.f), gScene.getPrevWorldMatrix(instanceID)).xyz;
    vsOut.prevPosH = mul(float4(prevPosW, 1.f), gScene.camera.getPrevViewProjMat());
#endif

    return vsOut;
}


float ff(const float a, const float b, float value) {
    return (value - a) / (b - a);
}

#if !is_valid(gHighpDepth)
[earlydepthstencil]
#endif
VBufferPSOut psMain(VBufferVSOut vsOut, uint triangleIndex : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics) {
    VBufferPSOut psOut;

    // Early camera range out
    if((vsOut.posH.z < 0.0) || (vsOut.posH.z > 1.0)) discard; // TODO: proper camera clip planes check here!

    // Using vsOut.posH.xy as pixel coordinate since it has the SV_Position semantic.
    int2 ipos = int2(vsOut.posH.xy);

    beginInvocationInterlock();

#if is_valid(gHighpDepth)
    // Cam depth
    if(vsOut.camZ > gHighpDepth[ipos]) {
        discard;
    } else {
        gHighpDepth[ipos] = vsOut.camZ;
    }
#endif

    const float2 gradX = ddx_fine(vsOut.texC); ///< Gradient of texture coordinate in the screen-space X direction
    const float2 gradY = ddy_fine(vsOut.texC); ///< Gradient of texture coordiante in teh screen-space Y direction

    uint materialID = vsOut.materialID;

    TriangleHit triangleHit;
    triangleHit.instanceID = vsOut.instanceID;
    triangleHit.primitiveIndex = triangleIndex;
    triangleHit.barycentrics = barycentrics.yz;

    // Store hit information.
    psOut.packedHitInfo = triangleHit.pack();
    

#if SCENE_HAS_PERPRIM_MATERIALS
    materialID = gScene.getMaterialID(triangleHit);
#endif 

#if USE_ALPHA_TEST
    // Alpha test.
    // With the default alpha test, only texC of VertexData is needed.
    // Alpha textures is sampled at lod=0 to match the ray traced V-buffer.
    VertexData v = {};
    v.texC = vsOut.texC;
    float lod = 0.f;

    //if (gScene.materials.alphaTest(v, vsOut.materialID, lod)) discard;
#endif

    // Note on barycentrics:
    // The barycentric weights provided to pixel shader correspond to vertices A, B, C of the rasterized triangle.
    // For triangle strips, every odd primitive has the order for vertices B and C flipped. We don't handle triangle
    // strips as DXR does not support them and Falcor uses triangle lists exclusively in its scene representation.
    // DXR intersection attributes store barycentric weights in a float2 for triangle vertices B and C.
    // This is what we store in the hit info.


#if is_valid(gTextureGrads)
    // Store screen space texture gradients
    {
        gTextureGrads[ipos] = float4(gradX, gradY);
    }
#endif

#if is_valid(gTestTexture)
    {
        //float a = vsOut.camZ * 0.0001f; //ff(88.0l, 94.0l, vsOut.camZ);
        float a = ff(gScene.camera.data.nearZ, gScene.camera.data.farZ, vsOut.camZ);
        gTestTexture[ipos] = float4(a, 0.0, 0.0, 0.0);
        //gTestTexture[ipos] = gTestTexture[ipos] + float4(0.25f, 0.0, 0.0, 0.0);
    }
#endif

#if is_valid(gMotionVector)
    // Compute motion vector.
    {
        const float2 pixelPos = ipos + float2(.5f, .5f); // Current sample in pixel coords.
        const float4 prevPosH = vsOut.prevPosH; // Sample in previous frame in clip space coords, no jittering applied.
        const float2 mv = calcMotionVector(pixelPos, prevPosH, gFrameDim) + float2(gScene.camera.data.jitterX, -gScene.camera.data.jitterY); // Remove camera jitter from motion vector
        gMotionVector[ipos] = mv;
    }
#endif

    endInvocationInterlock();

    return psOut;
}
