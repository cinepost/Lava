#include "Utils/Math/MathConstants.slangh"
#include "Utils/Sampling/VisibilitySamplesContainer.slangh"

__exported import Scene.SceneTypes;
__exported import Scene.HitInfo;


/** This struct represents the GPU-side visibility samples container system.
*/
struct VisibilitySamplesContainer {
    uint2       resolution;
    uint        maxTransparentSamplesCount;    ///< Maximum number of transparent surface per pixel.

    uint32_t    flags;                      
    
    RWTexture2D<PackedHitInfo>                      opaqueVisibilitySamplesBuffer;
    RWTexture2D<uint>                               opaqueVisibilitySamplesPositionBuffer;
    RWTexture2D<uint>                               opaqueVisibilitySamplesTransparentOffsetBuffer;

    RWStructuredBuffer<TransparentVisibilitySample> transparentVisibilitySamplesBuffer;

    RWByteAddressBuffer transparentVisibilitySamplesCounterBuffer;

    RWBuffer<uint> infoBuffer;

    HitInfo readOpaqueHit(uint2 pos) {
        return HitInfo(opaqueVisibilitySamplesBuffer[pos]);
    }

    void writeOpaqueHit(uint2 pos, const HitInfo hit) {
        if(isOpaqueSorted()) return;

        InterlockedAdd(infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_OPAQUE_SAMPLES_COUNT_LOCATION], 1);

        opaqueVisibilitySamplesBuffer[pos] = hit.getData();
    }

    void writeTransparentHit(uint2 pos, const HitInfo hit, float alpha, uint64_t depth) {
        if(pos.x >= resolution.x || pos.y >= resolution.y) return;

        TransparentVisibilitySample transparentSample = {};
        transparentSample.hitInfo = hit.getData();
        transparentSample.depth = depth;
        transparentSample.alpha = alpha;
        transparentSample.nextSampleOffset = 0;

        uint prevOffset = opaqueVisibilitySamplesTransparentOffsetBuffer[pos];
        uint transparentSampleOffset;

        if(prevOffset == UINT32_MAX) {
            // First transparent sample
            opaqueVisibilitySamplesTransparentOffsetBuffer[pos] = transparentSampleOffset;
            transparentVisibilitySamplesBuffer[transparentSampleOffset] = transparentSample;
            InterlockedAdd(infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_SAMPLES_COUNT_LOCATION], 1, transparentSampleOffset);

        } else {
            // Next transparent sample
            InterlockedAdd(infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_SAMPLES_COUNT_LOCATION], 1, transparentSampleOffset);
        }
    }

    bool isOpaqueSorted() CONST_FUNCTION {
        return (flags & (uint)VisibilitySamplesContainerFlags::OpaqueSamplesSorted) != 0;
    }

    bool isTransparentSorted() CONST_FUNCTION {
        return (flags & (uint)VisibilitySamplesContainerFlags::TransparentSamplesSorted) != 0;
    }

    uint opaqueSamplesCount() CONST_FUNCTION {
        return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_OPAQUE_SAMPLES_COUNT_LOCATION];
    }

    uint transparentSamplesCount() CONST_FUNCTION {
        return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_SAMPLES_COUNT_LOCATION];
    }

};

#ifdef VISIBILITY_CONTAINER_PARAMETER_BLOCK
// Declare parameter block used for shader reflection.
ParameterBlock<VisibilitySamplesContainer> gVisibilitySamplesContainer;
void main() {};
#endif