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

    PackedHitInfo procHit = gVisibilityContainer.opaqueVisibilitySamplesBuffer[procPixelOffset];
    if(procHit[0] == 0) return;

    uint existingOffset;
    InterlockedAdd(gVisibilityContainer.infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_SORT_OPAQUE_COUNTER_LOCATION], 1, existingOffset);

    uint procPixelPos = u2x16to32(uint16_t2(procPixelOffset % gVisibilityContainer.resolution.x, procPixelOffset / gVisibilityContainer.resolution.x));
    gVisibilityContainer.opaqueVisibilitySamplesPositionBufferPP[existingOffset] = procPixelPos;
    InterlockedAdd(gVisibilityContainer.infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_OPAQUE_SAMPLES_COUNT_LOCATION], 1);

    if(existingOffset == procPixelOffset) {
        return;
    }

    PackedHitInfo existingHit = gVisibilityContainer.opaqueVisibilitySamplesBuffer[existingOffset];
    if(existingHit[0] != 0) {
        return;
    }

    GroupMemoryBarrierWithGroupSync();

    gVisibilityContainer.opaqueVisibilitySamplesBuffer[existingOffset] = procHit;
    gVisibilityContainer.opaqueVisibilitySamplesBuffer[procPixelOffset] = {};

    GroupMemoryBarrierWithGroupSync();
}
