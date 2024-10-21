#include "Utils/Math/MathConstants.slangh"
#include "Utils/Sampling/VisibilitySamplesContainer.slangh"

import Utils.Math.PackedFormats;
import Utils.Math.FormatConversion;
import Utils.Sampling.VisibilitySamplesContainer;

ParameterBlock<VisibilitySamplesContainer> gVisibilityContainer;

cbuffer CB {

};

RWStructuredBuffer<uint> gOpaqueIndirectionBuffer;
RWStructuredBuffer<uint> gTransparentIndirectionBuffer;


[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    if(dispatchThreadId.x >= gVisibilityContainer.resolution1D) return;

    uint procPixelOffset = dispatchThreadId.x;
    uint16_t2 procPixelPos = uint16_t2(procPixelOffset % gVisibilityContainer.resolution.x, procPixelOffset / gVisibilityContainer.resolution.x);

#if VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_SAMPLES_TEXTURE
    PackedHitInfo procHit = gVisibilityContainer.opaqueVisibilitySamplesExternalTexture[procPixelPos];
#else
    PackedHitInfo procHit = gVisibilityContainer.opaqueVisibilitySamplesBuffer[procPixelOffset];
#endif
    if(procHit[0] == 0) return;

    uint existingOffset;
    InterlockedAdd(gVisibilityContainer.infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_SORT_OPAQUE_COUNTER_LOCATION], 1, existingOffset);

    gVisibilityContainer.opaqueVisibilitySamplesPositionBufferPP[existingOffset] = u2x16to32(procPixelPos);
    InterlockedAdd(gVisibilityContainer.infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_OPAQUE_SAMPLES_COUNT_LOCATION], 1);

    if(existingOffset == procPixelOffset) {
        return;
    }

    #if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
        uint4 procCombinedNormal;
        #if VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_NORMALS_TEXTURE
            procCombinedNormal = gVisibilityContainer.opaqueCombinedNormalsExternalTexture[procPixelPos];
        #else
            procCombinedNormal = gVisibilityContainer.opaqueCombinedNormalsBuffer[procPixelOffset];
        #endif
    #endif

    uint16_t2 existingPixelPos = uint16_t2(existingOffset % gVisibilityContainer.resolution.x, existingOffset / gVisibilityContainer.resolution.x);
#if VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_SAMPLES_TEXTURE
    PackedHitInfo existingHit = gVisibilityContainer.opaqueVisibilitySamplesExternalTexture[existingPixelPos];
#else
    PackedHitInfo existingHit = gVisibilityContainer.opaqueVisibilitySamplesBuffer[existingOffset];
#endif

    if(existingHit[0] != 0) {
        return;
    }

    //GroupMemoryBarrierWithGroupSync();

#if VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_SAMPLES_TEXTURE
    gVisibilityContainer.opaqueVisibilitySamplesExternalTexture[existingPixelPos] = procHit;
    gVisibilityContainer.opaqueVisibilitySamplesExternalTexture[procPixelPos] = {};
#else
    gVisibilityContainer.opaqueVisibilitySamplesBuffer[existingOffset] = procHit;
    gVisibilityContainer.opaqueVisibilitySamplesBuffer[procPixelOffset] = {};
#endif


#if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
    #if VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_NORMALS_TEXTURE
        gVisibilityContainer.opaqueCombinedNormalsExternalTexture[existingPixelPos] = procCombinedNormal;
        //gVisibilityContainer.opaqueCombinedNormalsExternalTexture[procPixelPos] = uint3(0);
    #else
        gVisibilityContainer.opaqueCombinedNormalsBuffer[existingOffset] = procCombinedNormal;
        //gVisibilityContainer.opaqueCombinedNormalsBuffer[procPixelOffset] = uint3(0);
    #endif
#endif

    GroupMemoryBarrierWithGroupSync();
}
