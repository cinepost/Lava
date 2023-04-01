#include "Utils/Math/MathConstants.slangh"
#include "CryptomattePass.slangh"

import Scene.Raster;
import Scene.Shading;

import Utils.Math.MathHelpers;


#define is_valid(name) (is_valid_##name != 0)

#ifndef _DATA_LAYERS_COUNT
#define _DATA_LAYERS_COUNT 0
#endif

cbuffer PerFrameCB {
  uint2             gFrameDim;
  float             gSumWeight;
  uint              gSampleNumber;    // Sample number starting from 1
  uint              gSamplesPerFrame;
  uint              gRanksCount;
  uint              gDataLayersCount;
};


// Input channel
Texture2D<PackedHitInfo> gVbuffer;
Buffer<float32_t3> gPreviewHashColorBuffer;

Buffer<float32_t> gFloatHashBuffer;
Buffer<uint32_t> gHashBuffer;

// Preview color output channel
RasterizerOrderedTexture2D<float4> gPreviewColor;

// Data layers
#if _DATA_LAYERS_COUNT > 0
RasterizerOrderedTexture2D<float4> gDataLayers[_DATA_LAYERS_COUNT];
RWStructuredBuffer<CryptomattePassSortingPair> gSortBuffers[_RANK];
#endif

// TODO: test groupshared buffers for counters
//groupshared CryptomattePassSortingPair gLocalPairs[16 * 16];

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // pixel position
    const uint2 ipos = dispatchThreadId.xy;

    // early out
    if (any(ipos >= gFrameDim)) return;

    const HitInfo hit = HitInfo(gVbuffer[ipos]);
    const bool validHit = hit.isValid();
    float4 previewColor = float4(0.);

    uint classID = 0;
    if (validHit && hit.getType() == HitType::Triangle) {
        TriangleHit triangleHit = hit.getTriangleHit();
        #if (_MODE == 0)
        classID = gScene.getMaterialID(triangleHit.instanceID);
        #endif

        #if (_MODE == 1)
        classID = gScene.getInternalID(triangleHit.instanceID);
        #endif

        #if defined(_OUTPUT_PREVIEW) && is_valid(gPreviewColor) && is_valid(gPreviewHashColorBuffer)
        previewColor = float4(gPreviewHashColorBuffer[classID], 1.0f);   
        #endif  // gPreviewColor
    }

    // Output preview color if needed
    #if defined(_OUTPUT_PREVIEW) && is_valid(gPreviewColor)
        gPreviewColor[ipos] = previewColor;    
    #endif  // gPreviewColor

// Current floas hash as uint (for comparsion accuracy)
#if is_valid(gFloatHashBuffer)
    const uint32_t currFloatHash = validHit ? asuint(gFloatHashBuffer[classID]) : 0;
#else
    const uint32_t currFloatHash = 0;
#endif

    const uint bpos = gFrameDim.x * ipos.y + ipos.x;

    CryptomattePassSortingPair localPairs[_RANK];
    [[unroll]] for(uint i = 0; i < _RANK; i++) localPairs[i].init();

    // Check and modify sorting buffer
    //[[unroll]]
    for(uint i = 0; i < _RANK; i++) {
        CryptomattePassSortingPair bufferedPair = gSortBuffers[i][bpos];

        if (!validHit) {
            localPairs[i] = bufferedPair;
        }
        else if (bufferedPair.hash == currFloatHash || bufferedPair.hash == 0) {
            bufferedPair.hash = currFloatHash;
            bufferedPair.counter += 1;
            gSortBuffers[i][bpos] = bufferedPair;
            localPairs[i] = bufferedPair;
            break;
        }
    }


    //// Write data layers

#if is_valid(gDataLayer00)
    [[branch]] if(gSamplesPerFrame == 0 || (gSamplesPerFrame == gSampleNumber)){
        const CryptomattePassSortingPair pair1 = localPairs[0];
        const CryptomattePassSortingPair pair2 = localPairs[1];
        gDataLayers[0][ipos] = float4(asfloat(pair1.hash), float(pair1.counter) / gSumWeight, asfloat(pair2.hash), float(pair2.counter) / gSumWeight);
    }
#endif // gDataLayer00

#if is_valid(gDataLayer01)
    [[branch]] if(gSamplesPerFrame == 0 || (gSamplesPerFrame == gSampleNumber)){
        const CryptomattePassSortingPair pair1 = localPairs[2];
        const CryptomattePassSortingPair pair2 = localPairs[3];
        gDataLayers[1][ipos] = float4(asfloat(pair1.hash), float(pair1.counter) / gSumWeight, asfloat(pair2.hash), float(pair2.counter) / gSumWeight);
    }
#endif // gDataLayer01

#if is_valid(gDataLayer02)
    [[branch]] if(gSamplesPerFrame == 0 || (gSamplesPerFrame == gSampleNumber)){
        const CryptomattePassSortingPair pair1 = localPairs[4];
        const CryptomattePassSortingPair pair2 = localPairs[5];
        gDataLayers[2][ipos] = float4(asfloat(pair1.hash), float(pair1.counter) / gSumWeight, asfloat(pair2.hash), float(pair2.counter) / gSumWeight);
    }
#endif // gDataLayer02

#if is_valid(gDataLayer03)
    [[branch]] if(gSamplesPerFrame == 0 || (gSamplesPerFrame == gSampleNumber)){
        const CryptomattePassSortingPair pair1 = localPairs[6];
        const CryptomattePassSortingPair pair2 = localPairs[7];
        gDataLayers[3][ipos] = float4(asfloat(pair1.hash), float(pair1.counter) / gSumWeight, asfloat(pair2.hash), float(pair2.counter) / gSumWeight);
    }
#endif // gDataLayer03

}
