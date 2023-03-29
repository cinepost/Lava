#include "Utils/Math/MathConstants.slangh"
#include "CryptomattePass.slangh"

import Scene.Raster;
import Scene.Shading;

import Utils.Math.MathHelpers;


#define is_valid(name) (is_valid_##name != 0)

#define MAX_CRYPTOMATTE_LAYERS 16

#ifndef _PREVIEW_COLOR_MODE
#define _PREVIEW_COLOR_MODE 0
#endif

#ifndef _DATA_LAYERS_COUNT
#define _DATA_LAYERS_COUNT 0
#endif

cbuffer PerFrameCB {
  uint2             gFrameDim;
  float             gSumWeight;
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
    
    uint classID = 0;

    [[branch]] if (!hit.isValid()) {
        #if is_valid(gPreviewColor)
            gPreviewColor[ipos] = float4(.0f);
        #endif  // gPreviewColor
        return;
    }


    if (hit.getType() == HitType::Triangle) {
        TriangleHit triangleHit = hit.getTriangleHit();
        #if (_MODE == 0)
        classID = gScene.getMaterialID(triangleHit.instanceID);
        #endif

        #if (_MODE == 1)
        //instanceID.index
        classID = gScene.getInternalID(triangleHit.instanceID);
        #endif
    }

#if is_valid(gHashBuffer)
    const uint32_t  currHash = gHashBuffer[classID];
#else
    const uint32_t  currHash = 0;
#endif

// Current floas hash as uint (for comparsion accuracy)
#if is_valid(gFloatHashBuffer)
    const uint32_t currFloatHash = asuint(gFloatHashBuffer[classID]);
#else
    const uint32_t currFloatHash = 0;
#endif

    const uint bpos = gFrameDim.x * ipos.y + ipos.x;

    CryptomattePassSortingPair localPairs[_RANK];
    [[unroll]] for(uint i = 0; i < _RANK; i++) localPairs[i].init();

    // Check and modify sorting buffer
    [[unroll]]
    for(uint i = 0; i < _RANK; i++) {
        CryptomattePassSortingPair pair = gSortBuffers[i][bpos];

        [[branch]]
        if ( pair.hash == currFloatHash || pair.hash == 0) {
            pair.hash = currFloatHash;
            pair.counter++;
            gSortBuffers[i][bpos] = pair;
            localPairs[i] = pair;
            break;
        }
    }


    // Write available false color AOVs
#if is_valid(gPreviewColor) && is_valid(gPreviewHashColorBuffer)
    gPreviewColor[ipos] = float4(gPreviewHashColorBuffer[classID], 1.0f);    
#endif  // gPreviewColor

    // Write data layers
#if is_valid(gDataLayer00)
    {
        //const CryptomattePassSortingPair pair1 = gSortBuffers[0][bpos];
        //const CryptomattePassSortingPair pair2 = gSortBuffers[1][bpos];
        //gDataLayers[0][ipos] = float4(asfloat(pair1.hash), float(pair1.counter) / gSumWeight, asfloat(pair2.hash), float(pair2.counter) / gSumWeight);

        const CryptomattePassSortingPair pair1 = localPairs[0];
        const CryptomattePassSortingPair pair2 = localPairs[1];
        gDataLayers[0][ipos] = float4(asfloat(pair1.hash), float(pair1.counter) / gSumWeight, asfloat(pair2.hash), float(pair2.counter) / gSumWeight);
    }
#endif // gDataLayer00

#if is_valid(gDataLayer01)
    {
        //const CryptomattePassSortingPair pair1 = gSortBuffers[2][bpos];
        //const CryptomattePassSortingPair pair2 = gSortBuffers[3][bpos];
        //gDataLayers[1][ipos] = float4(asfloat(pair1.hash), float(pair1.counter) / gSumWeight, asfloat(pair2.hash), float(pair2.counter) / gSumWeight);

        const CryptomattePassSortingPair pair1 = localPairs[2];
        const CryptomattePassSortingPair pair2 = localPairs[3];
        gDataLayers[1][ipos] = float4(asfloat(pair1.hash), float(pair1.counter) / gSumWeight, asfloat(pair2.hash), float(pair2.counter) / gSumWeight);
    }
#endif // gDataLayer01


}
