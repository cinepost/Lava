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

    uint procRoot = gVisibilityContainer.rootTransparentSampleOffsetBufferPP[procPixelOffset];
    if(procRoot == UINT32_MAX) return;
    
    uint existingOffset;
    InterlockedAdd(gVisibilityContainer.infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_SORT_TRANSP_COUNTER_LOCATION], 1, existingOffset);

    uint procPixelPos = u2x16to32(uint16_t2(procPixelOffset % gVisibilityContainer.resolution.x, procPixelOffset / gVisibilityContainer.resolution.x));
    gVisibilityContainer.transparentVisibilitySamplesPositionBufferPP[existingOffset] = procPixelPos;
    InterlockedAdd(gVisibilityContainer.infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_LISTS_COUNT_LOCATION], 1);

    if(existingOffset == procPixelOffset) {
        return;
    }

    uint existingRoot = gVisibilityContainer.rootTransparentSampleOffsetBufferPP[existingOffset];
    if(existingRoot != UINT32_MAX) {
        return;
    }

    //GroupMemoryBarrierWithGroupSync();

    gVisibilityContainer.rootTransparentSampleOffsetBufferPP[existingOffset] = procRoot;
    gVisibilityContainer.rootTransparentSampleOffsetBufferPP[procPixelOffset] = UINT32_MAX;

    GroupMemoryBarrierWithGroupSync();
}
