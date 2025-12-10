#ifndef SRC_FALCOR_SCENE_RAYTRACING_H_
#define SRC_FALCOR_SCENE_RAYTRACING_H_

#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Core/API/RtAccelerationStructure.h"

namespace Falcor {

// Ray tracing acceleration structure
struct TlasData {
    RtAccelerationStructure::SharedPtr pTlasObject;
    Buffer::SharedPtr pTlasBuffer;
    RtAccelerationStructure::UpdateMode updateMode = RtAccelerationStructure::UpdateMode::Rebuild;    ///< Update mode this TLAS was created with.
};

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_RAYTRACING_H_
