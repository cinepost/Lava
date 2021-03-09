#pragma once
#include "Utils/HostDeviceShared.slangh"

BEGIN_NAMESPACE_FALCOR

static const uint32_t kMaxSamples = 32;

struct HBAOData
{
    float4 sampleKernel[kMaxSamples];
    float2 noiseScale = float2(1, 1);
    uint32_t kernelSize = 16;
    float radius = 0.1f;
};

END_NAMESPACE_FALCOR
