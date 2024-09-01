#include "Utils/Math/MathConstants.slangh"

__exported import Scene.SceneTypes;
__exported import Scene.HitInfo;


struct OpaqueVisibilitySample {
    uint16_t posX;
    uint16_t posY;
    uint     transparentSampleOffset;
    PackedHitInfo hitInfo;
};


struct TransparentVisibilitySample {
    float16_t alpha;
    uint16_t posY;
    uint     nextSampleOffset;
    PackedHitInfo hitInfo;
};


/** This struct represents the GPU-side visibility samples container system.
*/
struct VisibilitySamplesContainer {
    uint2   resolution;
    uint    opaqueSamplesCount;            ///< Total number of opaque surface samples.
    uint    transparentSamplesCount;       ///< Total number of transparent surface samples.                      
    
    StructuredBuffer<OpaqueVisibilitySample>        opaqueVisibilityBuffer;
    StructuredBuffer<TransparentVisibilitySample>   transparentVisibilityBuffer;

    void writeOpaqueHit(uint2 pos, hitInfo hit) {
        if(pos.x >= resolution.x || pos.y  >= resoluton.y) return;
    }

    void writeTransparentHit(uint2 pos, hitInfo hit) {
        if(pos.x >= resolution.x || pos.y  >= resoluton.y) return;
    }

};
