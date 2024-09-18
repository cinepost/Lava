#include "Utils/Math/MathConstants.slangh"
#include "Utils/Sampling/VisibilitySamplesContainer.slangh"

__exported import Scene.SceneTypes;
__exported import Scene.HitInfo;

#ifndef VISIBILITY_SAMPLES_CONTAINER_TRACK_TRANSPARENT_SAMPLES_COUNT_PP
#define VISIBILITY_SAMPLES_CONTAINER_TRACK_TRANSPARENT_SAMPLES_COUNT_PP 0
#endif

/** This struct represents the GPU-side visibility samples container system.
*/
struct VisibilitySamplesContainer {
    uint2       resolution;
    uint        maxTransparentSamplesCountPP;    ///< Maximum number of transparent surface per pixel.
    float       alphaThresholdMin;
    float       alphaThresholdMax;

    uint32_t    flags;                      
    
    RWTexture2D<PackedHitInfo>                      opaqueVisibilitySamplesBuffer;
    RWTexture2D<uint>                               opaqueVisibilitySamplesPositionBuffer;
    RWTexture2D<uint>                               opaqueVisibilitySamplesTransparentOffsetBuffer;

    RWTexture2D<uint>                               transparentVisibilitySamplesCountBuffer;
    RWStructuredBuffer<TransparentVisibilitySample> transparentVisibilitySamplesBuffer;

    RWStructuredBuffer<uint> infoBuffer;

    HitInfo readOpaqueHit(uint2 pos) {
        return HitInfo(opaqueVisibilitySamplesBuffer[pos]);
    }

    TransparentVisibilitySample readFirstTransparentVisibilitySample(uint2 pos) {
        uint sampleOffset = opaqueVisibilitySamplesTransparentOffsetBuffer[pos];
        if(sampleOffset == UINT32_MAX) return {};

        return transparentVisibilitySamplesBuffer[sampleOffset];
    }

    TransparentVisibilitySample readTransparentVisibilitySample(uint offset) {
        return transparentVisibilitySamplesBuffer[offset];
    }

    void writeOpaqueHit(uint2 pos, const HitInfo hit) {
        if(isOpaqueSorted()) return;

        InterlockedAdd(infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_OPAQUE_SAMPLES_COUNT_LOCATION], 1);

        opaqueVisibilitySamplesBuffer[pos] = hit.getData();
    }

    void writeTransparentHit(uint2 pos, const HitInfo hit, float alpha, uint64_t depth) {
        if(pos.x >= resolution.x || pos.y >= resolution.y) return;
        if(!hit.isValid() || alpha < alphaThresholdMin) return;

        if(alpha > alphaThresholdMax) {
            writeOpaqueHit(pos, hit);
            return;
        }

        infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_HAS_TRANSPARENT_SAMPLES_LOCATION] = 1;

        TransparentVisibilitySample transparentSample = {};
        transparentSample.hitInfo = hit.getData();
        transparentSample.depth = depth;
        transparentSample.alpha = alpha;

        uint currSamplesCount = 0;
        uint currSampleOffset = opaqueVisibilitySamplesTransparentOffsetBuffer[pos];
        uint newSampleOffset;

        InterlockedAdd(transparentVisibilitySamplesCountBuffer[pos], 1, currSamplesCount);

        if(currSampleOffset == UINT32_MAX || currSamplesCount == 0) {
            // First transparent sample
            
            #if (VISIBILITY_SAMPLES_CONTAINER_TRACK_TRANSPARENT_SAMPLES_COUNT_PP == 0)
                InterlockedAdd(infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_SAMPLES_COUNT_LOCATION], 1, newSampleOffset);
            #else
                newSampleOffset = currSamplesCount % maxTransparentSamplesCountPP + pos.x + pos.y * resolution.x;
            #endif

            opaqueVisibilitySamplesTransparentOffsetBuffer[pos] = newSampleOffset;
            transparentVisibilitySamplesBuffer[newSampleOffset] = transparentSample;

        } else {
            // Next transparent sample
            
            #if (VISIBILITY_SAMPLES_CONTAINER_TRACK_TRANSPARENT_SAMPLES_COUNT_PP == 0)
                InterlockedAdd(infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_SAMPLES_COUNT_LOCATION], 1, newSampleOffset);
                transparentVisibilitySamplesBuffer[currSampleOffset].nextSampleOffset = newSampleOffset;
            #else

            #endif

            opaqueVisibilitySamplesTransparentOffsetBuffer[pos] = newSampleOffset;
            transparentVisibilitySamplesBuffer[newSampleOffset] = transparentSample;
        }
    }

    bool isOpaqueSorted() CONST_FUNCTION {
        return (flags & (uint)VisibilitySamplesContainerFlags::OpaqueSamplesSorted) != 0;
    }

    bool isTransparentSorted() CONST_FUNCTION {
        return (flags & (uint)VisibilitySamplesContainerFlags::TransparentSamplesSorted) != 0;
    }

    bool hasTransparentSamples() CONST_FUNCTION {
        return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_HAS_TRANSPARENT_SAMPLES_LOCATION];
    }

    uint opaqueSamplesCount() CONST_FUNCTION {
        return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_OPAQUE_SAMPLES_COUNT_LOCATION];
    }

    uint transparentSamplesCount() CONST_FUNCTION {
        return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_SAMPLES_COUNT_LOCATION];
    }

    uint transparentSamplesCount(uint2 pos) CONST_FUNCTION {
#if (VISIBILITY_SAMPLES_CONTAINER_TRACK_TRANSPARENT_SAMPLES_COUNT_PP == 0)
        return min(transparentVisibilitySamplesCountBuffer[pos], maxTransparentSamplesCountPP);
#else
        return maxTransparentSamplesCountPP;
#endif
    }

    float getAlphaThresholdMin() CONST_FUNCTION {
        return alphaThresholdMin;
    }

    float getAlphaThresholdMax() CONST_FUNCTION {
        return alphaThresholdMax;
    }
};

#ifdef VISIBILITY_CONTAINER_PARAMETER_BLOCK
// Declare parameter block used for shader reflection.
ParameterBlock<VisibilitySamplesContainer> gVisibilitySamplesContainer;
void main() {};
#endif