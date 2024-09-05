#include "Utils/Math/MathConstants.slangh"
#include "Utils/Sampling/VisibilitySamplesContainer.slangh"

__exported import Scene.SceneTypes;
__exported import Scene.HitInfo;


/** This struct represents the GPU-side visibility samples container system.
*/
struct VisibilitySamplesContainer {
    uint2       resolution;
    uint        opaqueSamplesCount;            ///< Total number of opaque surface samples.
    uint        transparentSamplesCount;       ///< Total number of transparent surface samples.

    uint32_t    flags;                      
    
    RWTexture2D<PackedHitInfo>                      opaqueVisibilityBuffer;
    RWTexture2D<PackedOpaqueSampleExtraData>        opaqueVisibilityExtraDataBuffer;

    RWStructuredBuffer<TransparentVisibilitySample> transparentVisibilityBuffer;

    void writeOpaqueHit(uint2 pos, hitInfo hit) {
        if(pos.x >= resolution.x || pos.y  >= resoluton.y) return;
    }

    void writeTransparentHit(uint2 pos, hitInfo hit) {
        if(pos.x >= resolution.x || pos.y  >= resoluton.y) return;
    }

    bool isOpaqueSorted() CONST_FUNCTION {
        return (flags & (uint)VisibilitySamplesContainerFlags::OpaqueSamplesSorted) != 0;
    }

    bool isTransparentSorted() CONST_FUNCTION {
        return (flags & (uint)VisibilitySamplesContainerFlags::TransparentSamplesSorted) != 0;
    }

};

#ifdef VISIBILITY_CONTAINER_PARAMETER_BLOCK
// Declare parameter block used for shader reflection.
ParameterBlock<VisibilitySamplesContainer> gVisibilitySamplesContainer;
void main() {};
#endif