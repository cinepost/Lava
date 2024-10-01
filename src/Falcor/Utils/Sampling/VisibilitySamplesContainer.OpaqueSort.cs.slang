#include "Utils/Math/MathConstants.slangh"
#include "Utils/Sampling/VisibilitySamplesContainer.slangh"

import Utils.Math.PackedFormats;
import Utils.Sampling.VisibilitySamplesContainer;

ParameterBlock<VisibilitySamplesContainer> gVisibilityContainer;

cbuffer CB {
    uint3   shadingThreadGroupSize;
};

RWStructuredBuffer<uint> gOpaqueIndirectionBuffer;
RWStructuredBuffer<uint> gTransparentIndirectionBuffer;

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    if(dispatchThreadId.x >= gVisibilityContainer.opaqueSamplesBufferSize) return;

    uint procPixelOffset = dispatchThreadId.x;

    PackedHitInfo procHit = gVisibilityContainer.opaqueVisibilitySamplesBuffer[procPixelOffset];
    if(procHit[0] == 0) return;

    uint existingOffset;
    InterlockedAdd(gOpaqueIndirectionBuffer[0], 1, existingOffset);
    if(existingOffset == procPixelOffset) return;

    PackedHitInfo existingHit = gVisibilityContainer.opaqueVisibilitySamplesBuffer[existingOffset];
    if(existingHit[0] != 0) return;

    //AllMemoryBarrierWithGroupSync();

    uint procTransparentRootOffset        = gVisibilityContainer.rootTransparentSampleOffsetBufferPP[procPixelOffset];
    uint procPixelPos                     = gVisibilityContainer.opaqueVisibilitySamplesPositionBuffer[procPixelOffset];
    uint procTransparentSamplesCount      = gVisibilityContainer.transparentVisibilitySamplesCountBufferPP[procPixelOffset];

    uint existingTransparentRootOffset    = gVisibilityContainer.rootTransparentSampleOffsetBufferPP[existingOffset];
    uint existingPixelPos                 = gVisibilityContainer.opaqueVisibilitySamplesPositionBuffer[existingOffset];
    uint existingTransparentSamplesCount  = gVisibilityContainer.transparentVisibilitySamplesCountBufferPP[existingOffset];

    //AllMemoryBarrierWithGroupSync();
    GroupMemoryBarrierWithGroupSync();

    gVisibilityContainer.opaqueVisibilitySamplesBuffer[existingOffset] = procHit;
    gVisibilityContainer.rootTransparentSampleOffsetBufferPP[existingOffset] = procTransparentRootOffset;
    gVisibilityContainer.opaqueVisibilitySamplesPositionBuffer[existingOffset] = procPixelPos;
    gVisibilityContainer.transparentVisibilitySamplesCountBufferPP[existingOffset] = procTransparentSamplesCount;

    gVisibilityContainer.opaqueVisibilitySamplesBuffer[procPixelOffset] = existingHit;
    gVisibilityContainer.rootTransparentSampleOffsetBufferPP[procPixelOffset] = existingTransparentRootOffset;
    gVisibilityContainer.opaqueVisibilitySamplesPositionBuffer[procPixelOffset] = existingPixelPos;
    gVisibilityContainer.transparentVisibilitySamplesCountBufferPP[procPixelOffset] = existingTransparentSamplesCount;

    //AllMemoryBarrierWithGroupSync();
    GroupMemoryBarrierWithGroupSync();

    //gVisibilityContainer.invalidateOpaqueSample(pixelOffset);

    //gVisibilityContainer.infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_FLAGS_LOCATION] |= (uint)0x1;
    
}
