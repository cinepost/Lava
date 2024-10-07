#include "Utils/Math/MathConstants.slangh"
#include "Utils/Sampling/VisibilitySamplesContainer.slangh"

import Utils.Math.PackedFormats;
import Utils.Sampling.VisibilitySamplesContainer;

ParameterBlock<VisibilitySamplesContainer> gVisibilityContainer;

cbuffer CB {
    uint3   shadingThreadGroupSize;
};

RWStructuredBuffer<uint3> gOpaqueIndirectionBuffer;
RWStructuredBuffer<uint3> gTransparentIndirectionBuffer;

uint div_round_up(uint a, uint b) { return (a + b - (uint)1) / b; }

[numthreads(16, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    if(dispatchThreadId.x == 0) {
        gOpaqueIndirectionBuffer[0] = { div_round_up(gVisibilityContainer.opaqueSamplesCount(), shadingThreadGroupSize.x), 1, 1};
        gTransparentIndirectionBuffer[0] = { div_round_up(gVisibilityContainer.transparentListsCount(), shadingThreadGroupSize.x), 1, 1};
    }
}
