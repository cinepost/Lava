#include "Utils/Math/MathConstants.slangh"

import Utils.Math.IntervalArithmetic;
import Utils.Math.PackedFormats;

static const uint kGroupWidth = CHUNK_WIDTH;
static const uint kChunkSize = CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_WIDTH;
static const uint kBrickWidth = BRICK_WIDTH;

cbuffer CB {
    uint        maxOpaqueSamplesCount;
    uint2       resolution;
};

Texture2D<uint>                                 rootTransparentSampleOffsetBufferPP;
RWStructuredBuffer<TransparentVisibilitySample> transparentVisibilitySamplesBuffer;
StructuredBuffer<uint>                          infoBuffer;

[numthreads(256, 1, 1)]
void main(uint3 groupID : SV_GroupID, uint3 localSubChunkCoords : SV_GroupThreadID, uint subChunkID : SV_GroupIndex) {
    const uint chunkID = groupID.x;
    const uint3 chunkCoords = gChunkCoords.Load3((3 * chunkID) << 2);

    uint rootOffset = rootTransparentSampleOffsetBufferPP;
    transparentVisibilitySamplesBuffer[rootOffset].nextSampleOffset = UINT32_MAX;
}
