#include "Utils/Math/MathConstants.slangh"
#include "Utils/HostDeviceShared.slangh"
#include "Utils/Sampling/VisibilitySamplesContainer.slangh"

import Utils.Math.FormatConversion;

__exported import Scene.SceneTypes;
__exported import Scene.HitInfo;


#ifndef VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP
#define VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP 0
#endif

/** This struct represents the GPU-side visibility samples container system.
*/
struct VisibilitySamplesContainer {
    uint2       resolution;
    uint        maxTransparentSamplesCountPP;    ///< Maximum number of transparent surface per pixel.
    uint        maxTransparentSamplesCount; 
    bool        limitTransparentSamplesCountPP;
    float       alphaThresholdMin;
    float       alphaThresholdMax;

    uint32_t    flags;                      
    
    // Opaque samples data
    RWTexture2D<PackedHitInfo>                      opaqueVisibilitySamplesBuffer;
    RWStructuredBuffer<uint>                        opaqueVisibilitySamplesPositionBuffer;

    // Transparent samples data
    RWTexture2D<uint>                               rootTransparentSampleOffsetBufferPP;
    RWTexture2D<uint>                               transparentVisibilitySamplesCountBuffer;
    RWStructuredBuffer<TransparentVisibilitySample> transparentVisibilitySamplesBuffer;

    // Opaque depth buffer data
    Texture2D<uint>                                 depthBuffer;

    RWStructuredBuffer<uint>                        infoBuffer;

    uint fetchSortedTransaprentSamples(uint2 pixelPos, out SortedTransparentVisibilitySample samples[VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP]) {
        if(!hasTransparentSamples()) return 0;

        // Fetch first transparent sample
        TransparentVisibilitySample rootTransparentSample;
        
        if(!readRootTransparentVisibilitySample(pixelPos, rootTransparentSample)) return 0;
        
        samples[0].hitInfo = rootTransparentSample.hitInfo;
        samples[0].alpha = rootTransparentSample.alpha;

        // Fetch remaining transparent samples
        uint nextSampleOffset = rootTransparentSample.nextSampleOffset;

        uint samplesCount = 1;
        while(nextSampleOffset != UINT32_MAX && samplesCount < transparentSamplesCount(pixelPos)) {            
            samples[samplesCount].hitInfo = transparentVisibilitySamplesBuffer[nextSampleOffset].hitInfo;
            samples[samplesCount++].alpha = transparentVisibilitySamplesBuffer[nextSampleOffset].alpha;
            nextSampleOffset = transparentVisibilitySamplesBuffer[nextSampleOffset].nextSampleOffset;
        }

        if(!isTransparentSorted()) {
            // TODO: Sort samples in-place
        }

        return samplesCount;
    }

    HitInfo readOpaqueHit(uint2 pixelPos) {
        return HitInfo(opaqueVisibilitySamplesBuffer[pixelPos]);
    }

    bool readRootTransparentVisibilitySample(uint2 pixelPos, inout TransparentVisibilitySample sample) {
        uint sampleOffset = rootTransparentSampleOffsetBufferPP[pixelPos];
        if(sampleOffset == UINT32_MAX) return false;

        #if VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP
            sampleOffset += __pixelBucketOffset(pixelPos);
        #endif

        sample = transparentVisibilitySamplesBuffer[sampleOffset];
        return true;
    }

    TransparentVisibilitySample readTransparentVisibilitySample(uint offset) {
        return transparentVisibilitySamplesBuffer[offset];
    }

    void writeOpaqueHit(uint2 pixelPos, const HitInfo hit) {
        if(isOpaqueSorted()) return;

        InterlockedAdd(infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_OPAQUE_SAMPLES_COUNT_LOCATION], 1);

        opaqueVisibilitySamplesBuffer[pixelPos] = hit.getData();

        uint tmp;
        InterlockedExchange(opaqueVisibilitySamplesPositionBuffer[pixelPos.x + pixelPos.y * resolution.y], u2x16to32(uint16_t2(pixelPos)), tmp);
    }

    void writeTransparentHit(uint2 pixelPos, const HitInfo hit, float alpha, uint64_t depth) {
        if(pixelPos.x >= resolution.x || pixelPos.y >= resolution.y) return;
        if(!hit.isValid() || alpha < alphaThresholdMin) return;

        if(alpha > alphaThresholdMax) {
            writeOpaqueHit(pixelPos, hit);
            return;
        }

        infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_HAS_TRANSPARENT_SAMPLES_LOCATION] = 1;

        TransparentVisibilitySample transparentSample = {};
        transparentSample.hitInfo = hit.getData();
        transparentSample.depth = depth;
        transparentSample.alpha = alpha;

        uint curSampleOffset, newSampleOffset;
        uint curSamplesCount = 0;
        
        // total transparent samples counter
        InterlockedAdd(infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_SAMPLES_COUNT_LOCATION], 1, curSampleOffset);

    #if VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP
    
        InterlockedAdd(rootTransparentSampleOffsetBufferPP[pixelPos], 1, curSampleOffset);
        newSampleOffset = ((curSampleOffset + 1) % maxTransparentSamplesCountPP) + __pixelBucketOffset(pixelPos);
        if(curSampleOffset != UINT32_MAX) curSampleOffset = (curSampleOffset % maxTransparentSamplesCountPP) + __pixelBucketOffset(pixelPos);

    #else
        if(curSampleOffset >= maxTransparentSamplesCount) return;
        newSampleOffset = curSampleOffset;
        
        InterlockedExchange(rootTransparentSampleOffsetBufferPP[pixelPos], newSampleOffset, curSampleOffset);
        InterlockedAdd(transparentVisibilitySamplesCountBuffer[pixelPos], 1, curSamplesCount);
    
    #endif

        transparentSample.nextSampleOffset = curSampleOffset;
        transparentVisibilitySamplesBuffer[newSampleOffset] = transparentSample;

        InterlockedMax(infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_MAX_TRANSPARENT_LAYERS_COUNT], min((curSamplesCount + 1), maxTransparentSamplesCountPP));
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

    uint maxTranstapertLayersCount() CONST_FUNCTION {
        return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_MAX_TRANSPARENT_LAYERS_COUNT];
    }

    uint transparentSamplesCount(uint2 pixelPos) CONST_FUNCTION {
#if VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP
        // When strict samples count pp is enabled we can use offset buffer as simple samples counter
        return min(rootTransparentSampleOffsetBufferPP[pixelPos] + 1, maxTransparentSamplesCountPP);
#else
        return transparentVisibilitySamplesCountBuffer[pixelPos];
#endif
    }

    float getAlphaThresholdMin() CONST_FUNCTION {
        return alphaThresholdMin;
    }

    float getAlphaThresholdMax() CONST_FUNCTION {
        return alphaThresholdMax;
    }

    uint __pixelBucketOffset(uint2 pixelPos) CONST_FUNCTION {
#if VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP
        return (pixelPos.x + pixelPos.y * resolution.x) * maxTransparentSamplesCountPP;
#else
        return 0;
#endif
    }
};

#ifdef VISIBILITY_CONTAINER_PARAMETER_BLOCK
// Declare parameter block used for shader reflection.
ParameterBlock<VisibilitySamplesContainer> gVisibilitySamplesContainer;
void main() {};
#endif