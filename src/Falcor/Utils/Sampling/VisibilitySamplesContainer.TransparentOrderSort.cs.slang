#include "Utils/Math/MathConstants.slangh"

import Utils.Math.PackedFormats;
import Utils.Math.FormatConversion;
import Utils.Sampling.VisibilitySamplesContainer;

ParameterBlock<VisibilitySamplesContainer> gVisibilityContainer;

cbuffer CB {

};

StructuredBuffer<uint> gOpaqueIndirectionBuffer;
StructuredBuffer<uint> gTransparentIndirectionBuffer;

static TransparentVisibilitySampleData samplesList[VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP];
static uint offsetsList[VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP];

#if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
static uint4 combinedNormalsList[VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP];
#endif

[numthreads(GROUP_SIZE_X, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint pixelOffset = dispatchThreadId.x;
    if(pixelOffset >= gVisibilityContainer.transparentListsCount()) return;

#if VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP
    if(gVisibilityContainer.maxTransparentSamplesCountPP < 2) return;
#endif

    if(!gVisibilityContainer.readRootTransparentVisibilitySample(pixelOffset, samplesList[0], offsetsList[0])) return;

#if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
    combinedNormalsList[0] = gVisibilityContainer.transparentCombinedNormalsBuffer[offsetsList[0]];
#endif

    // Fetch remaining transparent samples
    uint nextSampleOffset = samplesList[0].nextSampleOffset;

    uint samplesCount = 1;
    while(nextSampleOffset != UINT32_MAX && samplesCount < gVisibilityContainer.maxTransparentSamplesCountPP) {
        offsetsList[samplesCount] = nextSampleOffset;         
        samplesList[samplesCount] = gVisibilityContainer.transparentVisibilitySamplesBuffer[nextSampleOffset];

        #if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
            combinedNormalsList[samplesCount] = gVisibilityContainer.transparentCombinedNormalsBuffer[nextSampleOffset];
        #endif

        nextSampleOffset = samplesList[samplesCount].nextSampleOffset;
        samplesCount++;
    }

    //GroupMemoryBarrierWithGroupSync();

    if(samplesCount < 2) return;

    // Sort list
    bool unsorted = true;
    while(unsorted) {
        uint swaps = 0;
        for(uint i = 0; i < (samplesCount - 1); ++i) {
            if(samplesList[i].depth > samplesList[i+1].depth) {
                TransparentVisibilitySampleData tmp_sample1 = samplesList[i];
                TransparentVisibilitySampleData tmp_sample2 = samplesList[i+1];

                #if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
                    uint4 tmpN = combinedNormalsList[i];
                    combinedNormalsList[i] = combinedNormalsList[i+1];
                    combinedNormalsList[i+1] = tmpN;
                #endif

                samplesList[i] = tmp_sample2;
                samplesList[i+1] = tmp_sample1;
                swaps++;
            }
        }
        unsorted = swaps > 0;
    }

    // Fix offsets
    for(uint i = 0; i < (samplesCount - 1); ++i) {
        samplesList[i].nextSampleOffset = offsetsList[i + 1];
    }
    samplesList[samplesCount-1].nextSampleOffset = UINT32_MAX;

    // Store sorted list back into visibility container
    for(uint i = 0; i < samplesCount; ++i) {
        gVisibilityContainer.transparentVisibilitySamplesBuffer[offsetsList[i]] = samplesList[i];

        #if VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS
            gVisibilityContainer.transparentCombinedNormalsBuffer[offsetsList[i]] = combinedNormalsList[i];
        #endif
    }

    //GroupMemoryBarrierWithGroupSync();
}
