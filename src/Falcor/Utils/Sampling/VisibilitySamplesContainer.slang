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
    uint        opaqueSamplesBufferSize;
    
    uint32_t    flags;                      
    
    // Opaque samples data
    RWStructuredBuffer<PackedHitInfo>               opaqueVisibilitySamplesBuffer;
    RWStructuredBuffer<uint>                        opaqueVisibilitySamplesPositionBuffer;

    // Transparent samples data
    RWStructuredBuffer<uint>                        rootTransparentSampleOffsetBufferPP;
    RWStructuredBuffer<uint>                        transparentVisibilitySamplesCountBufferPP;
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

    HitInfo readOpaqueHit(uint pixelOffset) {
        return HitInfo(opaqueVisibilitySamplesBuffer[pixelOffset]);
    }

    HitInfo readOpaqueHit2D(uint2 pixelPos) {
        uint pixelOffset = pixelPos.x + pixelPos.y * resolution.x;
        if(isOpaqueSorted()) {
            uint2 pixelPos = (uint2)u32to2x16(opaqueVisibilitySamplesPositionBuffer[pixelOffset]);
            pixelOffset = pixelPos.x + pixelPos.y * resolution.x;
        }
        return readOpaqueHit(pixelOffset);
    }

    void invalidateOpaqueSample(uint pixelOffset) {
        opaqueVisibilitySamplesBuffer[pixelOffset] = {};
    }

    void writeOpaqueHit2D(uint2 pixelPos, const HitInfo hit) {
        if(isOpaqueSorted()) return;

        uint pixelOffset = pixelPos.x + pixelPos.y * resolution.x;
        opaqueVisibilitySamplesBuffer[pixelOffset] = hit.getData();

        uint tmp;
        InterlockedExchange(opaqueVisibilitySamplesPositionBuffer[pixelOffset], u2x16to32(uint16_t2(pixelPos)), tmp);
    }

    bool readRootTransparentVisibilitySample(uint2 pixelPos, inout TransparentVisibilitySample sample) {
        uint sampleOffset = rootTransparentSampleOffsetBufferPP[pixelPos.x + pixelPos.y * resolution.x];
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

    void writeTransparentHit(uint2 pixelPos, const HitInfo hit, float alpha, uint64_t depth) {
        if(pixelPos.x >= resolution.x || pixelPos.y >= resolution.y) return;
        if(!hit.isValid() || alpha < alphaThresholdMin) return;

        if(alpha > alphaThresholdMax) {
            writeOpaqueHit2D(pixelPos, hit);
            return;
        }

        infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_HAS_TRANSPARENT_SAMPLES_LOCATION] = 1;

        TransparentVisibilitySample transparentSample = {};
        transparentSample.hitInfo = hit.getData();
        transparentSample.depth = depth;
        transparentSample.alpha = alpha;

        uint pixelOffset = pixelPos.x + pixelPos.y * resolution.x;
        uint curSampleOffset, newSampleOffset;
        uint curSamplesCount = 0;
        
        // total transparent samples counter
        InterlockedAdd(infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_SAMPLES_COUNT_LOCATION], 1, curSampleOffset);

    #if VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP
    
        InterlockedAdd(rootTransparentSampleOffsetBufferPP[pixelOffset], 1, curSampleOffset);
        newSampleOffset = ((curSampleOffset + 1) % maxTransparentSamplesCountPP) + __pixelBucketOffset(pixelPos);
        if(curSampleOffset != UINT32_MAX) curSampleOffset = (curSampleOffset % maxTransparentSamplesCountPP) + __pixelBucketOffset(pixelPos);

    #else
        if(curSampleOffset >= maxTransparentSamplesCount) return;
        newSampleOffset = curSampleOffset;
        
        InterlockedExchange(rootTransparentSampleOffsetBufferPP[pixelOffset], newSampleOffset, curSampleOffset);
        InterlockedAdd(transparentVisibilitySamplesCountBufferPP[pixelOffset], 1, curSamplesCount);
    
    #endif

        transparentSample.nextSampleOffset = curSampleOffset;
        transparentVisibilitySamplesBuffer[newSampleOffset] = transparentSample;

        InterlockedMax(infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_MAX_TRANSPARENT_LAYERS_COUNT_LOCATION], min((curSamplesCount + 1), maxTransparentSamplesCountPP));
    }

    bool isOpaqueSorted() CONST_FUNCTION {
        return (flags & (uint)VisibilitySamplesContainerFlags::OpaqueSamplesSorted) != 0;
        //return (infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_FLAGS_LOCATION] & (uint)VisibilitySamplesContainerFlags::OpaqueSamplesSorted) != 0;
    }

    bool isTransparentSorted() CONST_FUNCTION {
        return (flags & (uint)VisibilitySamplesContainerFlags::TransparentSamplesSorted) != 0;
        //return (infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_FLAGS_LOCATION] & (uint)VisibilitySamplesContainerFlags::TransparentSamplesSorted) != 0;
    }

    bool hasTransparentSamples() CONST_FUNCTION {
        return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_HAS_TRANSPARENT_SAMPLES_LOCATION];
    }

    uint opaqueSamplesCount() CONST_FUNCTION {
        if(isOpaqueSorted()) {
            return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_OPAQUE_SAMPLES_COUNT_LOCATION];
        }
        return opaqueSamplesBufferSize;
    }

    uint transparentSamplesCount() CONST_FUNCTION {
        return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_SAMPLES_COUNT_LOCATION];
    }

    uint maxTranstapertLayersCount() CONST_FUNCTION {
        return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_MAX_TRANSPARENT_LAYERS_COUNT_LOCATION];
    }

    //uint2 pixelPos(uint2 pos) CONST_FUNCTION {
    //    if(isOpaqueSorted()) {
    //        return (uint2)u32to2x16(opaqueVisibilitySamplesPositionBuffer[pos.x + pos.y * resolution.x]);
    //    }
    //    return pos;
    //}

    uint2 pixelPos(uint pixelOffset) CONST_FUNCTION {
        if(isOpaqueSorted()) {
            return (uint2)u32to2x16(opaqueVisibilitySamplesPositionBuffer[pixelOffset]);
        }
        return { pixelOffset % resolution.x, pixelOffset / resolution.x };
    }

    uint transparentSamplesCount(uint2 pixelPos) CONST_FUNCTION {
        uint pixelOffset = pixelPos.x + pixelPos.y * resolution.x;
#if VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP
        // When strict samples count pp is enabled we can use offset buffer as simple samples counter
        return min(rootTransparentSampleOffsetBufferPP[pixelOffset] + 1, maxTransparentSamplesCountPP);
#else
        return transparentVisibilitySamplesCountBufferPP[pixelOffset];
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