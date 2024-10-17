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

// Extra input channels
Texture2D<PackedHitInfo>    gVBuffer;
Texture2D<float>            gDepth;
Texture2D<float4>           gTextureGrads;
Texture2D<float2>           gMotionVector;

// Extra input channels
Texture2D<uint>             gDrawCount;
Texture2D<uint4>            gNormW;

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

//#if defined(USE_VISIBILITY_CONTAINER)
//    #if defined(TRANSPARENT_SHADING_PASS)
//        static TransparentVisibilitySample transparenSamplesList[VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP];
//    #endif
//#endif

#if defined(USE_VISIBILITY_CONTAINER)
[numthreads(GROUP_SIZE_X, 1, 1)]
#else
[numthreads(8, 8, 1)]
#endif
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
#if defined(USE_VISIBILITY_CONTAINER)
    #if defined(TRANSPARENT_SHADING_PASS)
        TransparentVisibilitySample transparenSamplesList[VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP];
    #endif
#endif

    uint2 pixelPos;

    #if !defined(USE_VISIBILITY_CONTAINER)
        // pixel position
        pixelPos = dispatchThreadId.xy;

        // early out
        if (any(pixelPos >= gFrameDim)) return;
    #else
        uint pixelOffset = dispatchThreadId.x;
        // early out
        #if defined(TRANSPARENT_SHADING_PASS)
        if (pixelOffset >= gVisibilityContainer.transparentListsCount()) return;
        #else
        if (pixelOffset >= gVisibilityContainer.opaqueSamplesCount()) return;
        #endif
    #endif

    float sampleAlpha = 1.0f;
    float3 normalW = float3(0.f);
    float3 faceNormalW = float3(0.f);
    HitInfo hit;

#if defined(USE_VISIBILITY_CONTAINER)
    #if defined(TRANSPARENT_SHADING_PASS)
        pixelPos = gVisibilityContainer.transparentPixelPos(pixelOffset);

        //gInOutColor[gVisibilityContainer.offsetToPixelPos(pixelOffset)] = float4(pixelPos.x, pixelPos.y, gVisibilityContainer.isTransparentRootsSorted() ? 1 : 0, 1);
        //return;

        const uint transparentVisibilitySamplesCount = gVisibilityContainer.fetchSortedTransparentSamplesList(pixelOffset, transparenSamplesList);
        if(transparentVisibilitySamplesCount == 0) return;

        float4 colorAccum = gInOutColor[pixelPos];

        for(uint layer = 0; layer < transparentVisibilitySamplesCount; ++layer) {
            TransparentVisibilitySample sample = transparenSamplesList[transparentVisibilitySamplesCount - layer - 1];
            hit = sample.getHitInfo();
            sampleAlpha = sample.alpha;
            normalW = sample.getNormalW();
            faceNormalW = sample.getFaceNormalW();
    #else
        pixelPos = gVisibilityContainer.opaquePixelPos(pixelOffset);
        
        //gInOutColor[gVisibilityContainer.offsetToPixelPos(pixelOffset)] = float4(pixelPos.x, pixelPos.y, gVisibilityContainer.isOpaqueSorted() ? 1 : 0, 1);
        //return;

        OpaqueVisibilitySample sample = gVisibilityContainer.readOpaqueSample(pixelOffset);
        hit = sample.getHitInfo();
        normalW = sample.getNormalW();
        faceNormalW = sample.getFaceNormalW();
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

#if defined(USE_VISIBILITY_CONTAINER)
    if( !gVisibilityContainer.hasCombinedNormals() || !sample.hasCombinedNormals() || !gScene.isObjectSubdividable(triangleHit.instanceID) ) {
        VertexData v = gScene.getVertexData(triangleHit);
        normalW = v.normalW.xyz;
        faceNormalW = v.faceNormalW.xyz;
    }
#else
    if (is_valid(gNormW) && gScene.isObjectSubdividable(triangleHit.instanceID)) {
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
        VertexData v = gScene.getVertexData(triangleHit);
        normalW = v.normalW.xyz;
        faceNormalW = v.faceNormalW.xyz;
    }
    
#endif


#if defined(TRANSPARENT_SHADING_PASS)
    colorAccum = colorAccum * (1.f - sampleAlpha) + float4(outColor, 1.f) * sampleAlpha;

    }

    gInOutColor[pixelPos] = colorAccum;
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
    gPrimID[pixelPos] = hit.getTriangleHit().primitiveIndex;
#endif  // gOutFresnel

#if is_valid(gOpID)
    gOpID[pixelPos] = hit.getTriangleHit().instanceID.index;
#endif  // gOpID

#if is_valid(gDrawCount) && is_valid(gMeshletDrawHeatMap)
    uint draw_count = gDrawCount[pixelPos];
    float temp = (draw_count - 1.0f) * 0.25f;
    gMeshletDrawHeatMap[pixelPos] = float4(gHeatMapColorGenerator.getColor(temp).rgb, draw_count);
#endif
}
