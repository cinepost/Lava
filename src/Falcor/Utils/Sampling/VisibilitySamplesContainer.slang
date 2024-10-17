#include "Utils/Math/MathConstants.slangh"
#include "Utils/HostDeviceShared.slangh"
#include "Utils/Sampling/VisibilitySamplesContainer.slangh"

import Utils.Math.FormatConversion;

__exported import Scene.SceneTypes;
__exported import Scene.HitInfo;


#ifndef VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP
#define VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP 0
#endif

#ifndef VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
#define VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS 0
#endif

#ifndef VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_SAMPLES_TEXTURE
#define VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_SAMPLES_TEXTURE 0
#endif

/** This struct represents the GPU-side visibility samples container system.
*/
struct VisibilitySamplesContainer {
    uint2       resolution;
    uint        resolution1D;
    uint        maxTransparentSamplesCountPP;    ///< Maximum number of transparent surface per pixel.
    uint        maxTransparentSamplesCount; 
    bool        limitTransparentSamplesCountPP;
    float       alphaThresholdMin;
    float       alphaThresholdMax;    
    uint32_t    flags;                      
    
    // Opaque samples data
    RWStructuredBuffer<PackedHitInfo>                   opaqueVisibilitySamplesBuffer;
    RWTexture2D<PackedHitInfo>                          opaqueVisibilitySamplesExternalTexture;

    RWStructuredBuffer<uint4>                           opaqueCombinedNormalsBuffer;
    RWTexture2D<uint3>                                  opaqueCombinedNormalsExternalTexture;

    RWStructuredBuffer<uint>                            opaqueVisibilitySamplesPositionBufferPP;
    RWTexture2D<uint>                                   opaqueExternalDepthTexture;

    // Transparent samples data
    RWStructuredBuffer<uint>                            rootTransparentSampleOffsetBufferPP;
    RWStructuredBuffer<uint>                            transparentVisibilitySamplesPositionBufferPP;
    RWStructuredBuffer<uint>                            transparentVisibilitySamplesCountBufferPP;
    RWStructuredBuffer<TransparentVisibilitySampleData> transparentVisibilitySamplesBuffer;
    RWStructuredBuffer<uint4>                           transparentCombinedNormalsBuffer;

    RWStructuredBuffer<uint>                            infoBuffer;

    uint fetchTransparentSamplesList(uint pixelOffset, inout TransparentVisibilitySample samples[VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP]) {
        if(!hasTransparentSamples()) return 0;

        // Fetch first transparent sample
        TransparentVisibilitySampleData rootTransparentSample;
        
        uint rootSampleOffset;
        if(!readRootTransparentVisibilitySample(pixelOffset, rootTransparentSample, rootSampleOffset)) return 0;
    
        samples[0].packedHitInfo = rootTransparentSample.packedHitInfo;
        samples[0].alpha = rootTransparentSample.alpha;

        #if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
            samples[0].packedCombinedNormal = transparentCombinedNormalsBuffer[rootSampleOffset];
        #else
            samples[0].packedCombinedNormal = uint4(0);
        #endif

        // Fetch remaining transparent samples
        uint nextSampleOffset = rootTransparentSample.nextSampleOffset;
    
        uint samplesCount = 1;
        
        #if VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP
        while(nextSampleOffset != UINT32_MAX && samplesCount < maxTransparentSamplesCountPP) {
        #else
        while(nextSampleOffset != UINT32_MAX) {            
        #endif
            samples[samplesCount].packedHitInfo = transparentVisibilitySamplesBuffer[nextSampleOffset].packedHitInfo;
            #if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
                samples[samplesCount].packedCombinedNormal.xyz = transparentCombinedNormalsBuffer[nextSampleOffset].xyz;
            #else
                samples[samplesCount].packedCombinedNormal = uint4(0);
            #endif

            samples[samplesCount++].alpha = transparentVisibilitySamplesBuffer[nextSampleOffset].alpha;
            nextSampleOffset = transparentVisibilitySamplesBuffer[nextSampleOffset].nextSampleOffset;
        }

        return samplesCount;
    }

    uint fetchSortedTransparentSamplesList(uint pixelOffset, inout TransparentVisibilitySample samples[VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP]) {
        uint samplesCount = fetchTransparentSamplesList(pixelOffset, samples);

        if(!isTransparentListsSorted()) {
            // TODO: Sort samples list in-place
        }

        return samplesCount;
    }

    uint fetchSortedTransparentSamplesList2D(uint2 pixelPos, inout TransparentVisibilitySample samples[VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP]) {
        if(!hasTransparentSamples()) return 0;

        // Fetch first transparent sample
        TransparentVisibilitySampleData rootTransparentSample;
        
        if(!readRootTransparentVisibilitySample2D(pixelPos, rootTransparentSample)) return 0;
        
        samples[0].packedHitInfo = rootTransparentSample.packedHitInfo;
        samples[0].alpha = rootTransparentSample.alpha;

        // Fetch remaining transparent samples
        uint nextSampleOffset = rootTransparentSample.nextSampleOffset;

        uint samplesCount = 1;
        while(nextSampleOffset != UINT32_MAX && samplesCount < maxTransparentSamplesCountPP) {            
            samples[samplesCount].packedHitInfo = transparentVisibilitySamplesBuffer[nextSampleOffset].packedHitInfo;
            samples[samplesCount++].alpha = transparentVisibilitySamplesBuffer[nextSampleOffset].alpha;
            nextSampleOffset = transparentVisibilitySamplesBuffer[nextSampleOffset].nextSampleOffset;
        }

        if(!isTransparentListsSorted()) {
            // TODO: Sort samples in-place
        }

        return samplesCount;
    }

    uint4 __readOpaqueCombinedNormals(uint pixelOffset) {
        #if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
            #if VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_NORMALS_TEXTURE
                uint2 pixelPos = uint2(pixelOffset % resolution.x, pixelOffset / resolution.x);
                return opaqueCombinedNormalsExternalTexture[pixelPos];
            #else
                return opaqueCombinedNormalsBuffer[pixelOffset];
            #endif
        #else
            return uint4(0);
        #endif
    }

    OpaqueVisibilitySample readOpaqueSample(uint pixelOffset) {
        #if VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_SAMPLES_TEXTURE
            uint2 pixelPos = uint2(pixelOffset % resolution.x, pixelOffset / resolution.x);
            return {opaqueVisibilitySamplesExternalTexture[pixelPos], __readOpaqueCombinedNormals(pixelOffset)};
        #else
            return {opaqueVisibilitySamplesBuffer[pixelOffset], __readOpaqueCombinedNormals(pixelOffset)};
        #endif
    }

    OpaqueVisibilitySample readOpaqueSample2D(uint2 pixelPos) {
        if(isOpaqueSorted()) return {};

        uint pixelOffset = pixelPos.x + pixelPos.y * resolution.x;
        return readOpaqueSample(pixelOffset);
    }

    void invalidateOpaqueSample(uint pixelOffset) {
        opaqueVisibilitySamplesBuffer[pixelOffset] = {};
    }

    void writeOpaqueHit2D(uint2 pixelPos, const HitInfo hit) {
        if(isOpaqueSorted()) return;

        uint pixelOffset = pixelPos.x + pixelPos.y * resolution.x;

        #if VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_SAMPLES_TEXTURE
            opaqueVisibilitySamplesExternalTexture[pixelPos] = hit.getData();
        #else
            opaqueVisibilitySamplesBuffer[pixelOffset] = hit.getData();
        #endif
    }

    void writeOpaqueHit2D(uint2 pixelPos, const HitInfo hit, const uint4 packedCombinedNormal) {
        if(isOpaqueSorted()) return;

        uint pixelOffset = pixelPos.x + pixelPos.y * resolution.x;

        #if VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_SAMPLES_TEXTURE
            opaqueVisibilitySamplesExternalTexture[pixelPos] = hit.getData();
        #else
            opaqueVisibilitySamplesBuffer[pixelOffset] = hit.getData();
        #endif

        #if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
            if(any(packedCombinedNormal != uint4(0))) {
            #if VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_NORMALS_TEXTURE
                opaqueCombinedNormalsExternalTexture[pixelPos] = packedCombinedNormal;
            #else
                opaqueCombinedNormalsBuffer[pixelOffset] = packedCombinedNormal;
            #endif
            }
        #endif
    }

    bool readRootTransparentVisibilitySample(uint pixelOffset, inout TransparentVisibilitySampleData sample) {
        uint rootSampleOffset;
        return readRootTransparentVisibilitySample(pixelOffset, sample, rootSampleOffset);
    }


    bool readRootTransparentVisibilitySample(uint pixelOffset, inout TransparentVisibilitySampleData sample, out uint rootSampleOffset) {
        rootSampleOffset = rootTransparentSampleOffsetBufferPP[pixelOffset];
        if(rootSampleOffset == UINT32_MAX) return false;

        #if VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP
            if(isTransparentRootsSorted()) {
                uint2 rootSamplePos = transparentPixelPos(pixelOffset);
                pixelOffset = rootSamplePos.x + rootSamplePos.y * resolution.x;
            } 
            rootSampleOffset = (rootSampleOffset % maxTransparentSamplesCountPP) + pixelOffset * maxTransparentSamplesCountPP;
        #endif

        sample = transparentVisibilitySamplesBuffer[rootSampleOffset];
        return true;
    }

    bool readRootTransparentVisibilitySample2D(uint2 pixelPos, inout TransparentVisibilitySampleData sample) {
        if(isTransparentRootsSorted()) {
            return false;
        }

        uint pixelOffset = pixelPos.x + pixelPos.y * resolution.x;
        return readRootTransparentVisibilitySample(pixelOffset, sample);
    }

    TransparentVisibilitySampleData readTransparentVisibilitySample(uint offset) {
        return transparentVisibilitySamplesBuffer[offset];
    }

    void writeTransparentHit(uint2 pixelPos, const HitInfo hit, float alpha, uint64_t depth, const uint4 packedCombinedNormal = uint4(0)) {
        if(pixelPos.x >= resolution.x || pixelPos.y >= resolution.y) return;
        if(!hit.isValid() || alpha < alphaThresholdMin) return;

        if(alpha > alphaThresholdMax) {
            writeOpaqueHit2D(pixelPos, hit);
            return;
        }

        infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_HAS_TRANSPARENT_SAMPLES_LOCATION] = 1;

        TransparentVisibilitySampleData transparentSample = {};
        transparentSample.packedHitInfo = hit.getData();
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

        #if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
            if(any(packedCombinedNormal != uint4(0))) transparentCombinedNormalsBuffer[newSampleOffset] = packedCombinedNormal;
        #endif

        InterlockedMax(infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_MAX_TRANSPARENT_LAYERS_COUNT_LOCATION], min((curSamplesCount + 1), maxTransparentSamplesCountPP));
    }

    bool isOpaqueSorted() CONST_FUNCTION {
        return (flags & (uint)VisibilitySamplesContainerFlags::OpaqueSamplesSorted) != 0;
        //return (infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_FLAGS_LOCATION] & (uint)VisibilitySamplesContainerFlags::OpaqueSamplesSorted) != 0;
    }

    void setOpaqueSorted() {
        infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_FLAGS_LOCATION] |= (uint32_t)VisibilitySamplesContainerFlags::OpaqueSamplesSorted;
    }

    bool isTransparentRootsSorted() CONST_FUNCTION {
        return (flags & (uint)VisibilitySamplesContainerFlags::TransparentRootsSorted) != 0;
        //return (infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_FLAGS_LOCATION] & (uint)VisibilitySamplesContainerFlags::TransparentRootsSorted) != 0;
    }

    void setTransparentRootsSorted() {
        infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_FLAGS_LOCATION] |= (uint32_t)VisibilitySamplesContainerFlags::TransparentRootsSorted;
    }

    bool isTransparentListsSorted() CONST_FUNCTION {
        return (flags & (uint)VisibilitySamplesContainerFlags::TransparentListsSorted) != 0;
        //return (infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_FLAGS_LOCATION] & (uint)VisibilitySamplesContainerFlags::TransparentListsSorted) != 0;
    }

    void setTransparentListsSorted() {
        infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_FLAGS_LOCATION] |= (uint32_t)VisibilitySamplesContainerFlags::TransparentListsSorted;
    }

    bool hasTransparentSamples() CONST_FUNCTION {
        return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_HAS_TRANSPARENT_SAMPLES_LOCATION];
    }

    bool hasCombinedNormals() CONST_FUNCTION {
        #if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
            return (flags & (uint)VisibilitySamplesContainerFlags::HasCombinedNormals) != 0;
        #else
            return false;
        #endif
    }

    uint opaqueSamplesCount() CONST_FUNCTION {
        if(isOpaqueSorted()) {
            return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_OPAQUE_SAMPLES_COUNT_LOCATION];
        }
        return resolution1D;
    }

    uint transparentListsCount() CONST_FUNCTION {
        if(isTransparentRootsSorted()) {
            return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_LISTS_COUNT_LOCATION];
        }
        return resolution1D;
    }

    uint transparentSamplesCount() CONST_FUNCTION {
        return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_SAMPLES_COUNT_LOCATION];
    }

    uint maxTransparentLayersCount() CONST_FUNCTION {
        return infoBuffer[VISIBILITY_CONTAINER_INFOBUFFER_MAX_TRANSPARENT_LAYERS_COUNT_LOCATION];
    }

    uint2 offsetToPixelPos(uint pixelOffset) CONST_FUNCTION {
        return { pixelOffset % resolution.x, pixelOffset / resolution.x };
    }

    uint2 opaquePixelPos(uint pixelOffset) CONST_FUNCTION {
        if(isOpaqueSorted()) {
            return (uint2)u32to2x16(opaqueVisibilitySamplesPositionBufferPP[pixelOffset]);
        }
        return { pixelOffset % resolution.x, pixelOffset / resolution.x };
    }

    uint2 transparentPixelPos(uint pixelOffset) CONST_FUNCTION {
        if(isTransparentRootsSorted()) {
            return (uint2)u32to2x16(transparentVisibilitySamplesPositionBufferPP[pixelOffset]);
        }
        return { pixelOffset % resolution.x, pixelOffset / resolution.x };
    }

    uint transparentSamplesCount(uint pixelOffset) CONST_FUNCTION {
#if VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP
        // When strict samples count pp is enabled we can use offset buffer as simple samples counter
        return min(rootTransparentSampleOffsetBufferPP[pixelOffset] + 1, maxTransparentSamplesCountPP);
#else
        if(isTransparentRootsSorted()) {
            uint2 samplePos = transparentPixelPos(pixelOffset);
            pixelOffset = samplePos.x + samplePos.y * resolution.x;
        } 
        return transparentVisibilitySamplesCountBufferPP[pixelOffset];
#endif
    }

    uint transparentSamplesCount2D(uint2 pixelPos) CONST_FUNCTION {
        uint pixelOffset = pixelPos.x + pixelPos.y * resolution.x;
        return transparentSamplesCount(pixelOffset);
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