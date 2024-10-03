#include "Utils/Math/MathConstants.slangh"

import Scene.Raster;
import Scene.Shading;

import Utils.Math.MathHelpers;
import Utils.Math.PackedFormats;
import Utils.Math.FormatConversion;
import Utils.Color.FalseColorGenerator;
import Utils.Color.HeatMapColorGenerator;
import Utils.Sampling.VisibilitySamplesContainer;


#ifndef FALSE_COLOR_BUFFER_SIZE
#define FALSE_COLOR_BUFFER_SIZE 0
#endif

#define is_valid(name) (is_valid_##name != 0)

// VBuffer
Texture2D<PackedHitInfo> gVBuffer;

// Extra input channels
Texture2D<float>    gDepth;
Texture2D<float4>   gTextureGrads;
Texture2D<float2>   gMotionVector;

// Extra input channels
Texture2D<uint>     gDrawCount;
Texture2D<uint4>    gNormW;

// Extra input buffers
Buffer<float4>      gFalseColorBuffer;

// Visibility samples container (optional)
ParameterBlock<VisibilitySamplesContainer> gVisibilityContainer;

// Main output channel
RWTexture2D<float4> gInOutColor;

// Extra input/output channels
RWTexture2D<uint>   gMeshletID;
RWTexture2D<uint>   gMicroPolyID;
RWTexture2D<float4> gAUX;  // Auxiliary debug data
RWTexture2D<uint>   gTime; // Timing debug buffer
RWTexture2D<float2> gUV;

// Extra output channels
RWTexture2D<float4> gOutNormals;
RWTexture2D<float4> gOutFaceNormals;
RWTexture2D<float4> gOutTangentNormals;
RWTexture2D<float>  gPrimID;
RWTexture2D<float>  gOpID;
RWTexture2D<float4> gMeshletColor;
RWTexture2D<float4> gMicroPolyColor;
RWTexture2D<float4> gTimeFalseColor;
RWTexture2D<float4> gMeshletDrawHeatMap;

cbuffer PerFrameCB {
    uint2           gFrameDim;
};

uniform FalseColorGenerator gFalseColorGenerator;
uniform HeatMapColorGenerator gHeatMapColorGenerator;

#if defined(USE_VISIBILITY_CONTAINER)
static SortedTransparentVisibilitySample transparenSamplesList[VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP];
#endif

#if defined(USE_VISIBILITY_CONTAINER)
[numthreads(GROUP_SIZE_X, 1, 1)]
#else
[numthreads(16, 16, 1)]
#endif
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint2 pixelPos;

    #if !defined(USE_VISIBILITY_CONTAINER)
        // pixel position
        pixelPos = dispatchThreadId.xy;

        // early out
        if (any(pixelPos >= gFrameDim)) return;
    #else
        // early out
        if (dispatchThreadId.x >= gVisibilityContainer.opaqueSamplesCount()) return;
    #endif

    float sampleAlpha = 1.0f;
    HitInfo hit;

#if defined(USE_VISIBILITY_CONTAINER)
    pixelPos = gVisibilityContainer.pixelPos(dispatchThreadId.x);
    ///gInOutColor[{dispatchThreadId.x % gFrameDim.x, dispatchThreadId.x / gFrameDim.x}] = float4(pixelPos.x, pixelPos.y, gVisibilityContainer.isOpaqueSorted() ? 1 : 0, 1);
    //return;

    #if defined(TRANSPARENT_SHADING_PASS)
        const uint transparentVisibilitySamplesCount = gVisibilityContainer.fetchSortedTransaprentSamples(pixelPos, transparenSamplesList);
        if(transparentVisibilitySamplesCount == 0) return;

        float4 colorAccum = gInOutColor[pixelPos];

        for(uint layer = 0; layer < transparentVisibilitySamplesCount; ++layer) {
            hit = HitInfo(transparenSamplesList[layer].hitInfo);
            sampleAlpha = transparenSamplesList[layer].alpha;
    #else
        hit = gVisibilityContainer.readOpaqueHit(dispatchThreadId.x);
    #endif
#else
    hit = HitInfo(gVBuffer[pixelPos]);
#endif

#if !defined(TRANSPARENT_SHADING_PASS)
    if (!hit.isValid()) {
        return;
    }
#endif

    TriangleHit triangleHit = hit.getTriangleHit();
    const float3 barycentrics = triangleHit.getBarycentricWeights();
    float3 outColor = barycentrics;

    VertexData v = gScene.getVertexData(triangleHit);
    float3 normalW = float3(0.f);
    float3 faceNormalW = float3(0.f);

#if is_valid(gNormW)
    if( gScene.isObjectSubdividable(triangleHit.instanceID) ) {
        uint3 packedNormal = gNormW[pixelPos].xyz;
        
        normalW = {
            unpackSnorm16(packedNormal.x >> 16),
            unpackSnorm16(packedNormal.y >> 16),
            unpackSnorm16(packedNormal.z >> 16)
        };

        faceNormalW = {
            unpackSnorm16(packedNormal.x & 0x0000ffff),
            unpackSnorm16(packedNormal.y & 0x0000ffff),
            unpackSnorm16(packedNormal.z & 0x0000ffff)
        };
    } else {
        normalW = v.normalW.xyz;
        faceNormalW = v.faceNormalW.xyz;
    }

#else
    normalW = v.normalW.xyz;
    faceNormalW = v.faceNormalW.xyz;
#endif


#if defined(TRANSPARENT_SHADING_PASS)
    colorAccum = colorAccum * (1.f - sampleAlpha) + float4(outColor, 1.f) * sampleAlpha;

    }

    gInOutColor[pixelPos] = colorAccum;
    return;
#else
    gInOutColor[pixelPos] = float4(outColor, 1.f);
#endif

#if is_valid(gOutNormals)
    gOutNormals[pixelPos] = float4(normalW.xyz, 0.f);
#endif  // gOutNormals

#if is_valid(gOutFaceNormals)
    gOutFaceNormals[pixelPos] = float4(faceNormalW.xyz, 0.f);
#endif  // gOutFaceNormals

#if is_valid(gOutTangentNormals)
    gOutTangentNormals[pixelPos] = float4(v.tangentW.xyz, 0.f);
#endif  // gOutTangentNormals

#if is_valid(gUV)
    gUV[pixelPos] = v.texC;
#endif  // gUV

#if is_valid(gMeshletColor) && is_valid(gMeshletID)
    gMeshletColor[pixelPos] = float4(gFalseColorGenerator.getColor(gMeshletID[pixelPos]).rgb, 1.0f);
#endif  // gMeshletColor


#if is_valid(gMicroPolyColor) && is_valid(gMicroPolyID)
    uint microPolyID = gMicroPolyID[pixelPos];
    gMicroPolyColor[pixelPos] = float4(gFalseColorGenerator.getColor(microPolyID).rgb, float(microPolyID + 1));
#endif  // gMicroPolyColor

#if is_valid(gPrimID)
    gPrimID[pixelPos] = triangleHit.primitiveIndex;
#endif  // gOutFresnel

#if is_valid(gOpID)
    gOpID[pixelPos] = triangleHit.instanceID.index;
#endif  // gOpID

#if is_valid(gDrawCount) && is_valid(gMeshletDrawHeatMap)
    uint draw_count = gDrawCount[pixelPos];
    float temp = (draw_count - 1.0f) * 0.25f;
    gMeshletDrawHeatMap[pixelPos] = float4(gHeatMapColorGenerator.getColor(temp).rgb, draw_count);
#endif
}
