#ifndef SRC_FALCOR_CORE_API_EMBREE_EMBREERAYTRACECONTEXT_H_
#define SRC_FALCOR_CORE_API_EMBREE_EMBREERAYTRACECONTEXT_H_

#include <memory>
#include "embree/include/embree3/rtcore.h"


namespace Falcor {

class EmbreeDevice;

class EmbreeRaytraceContext {

public:
	using SharedPtr = std::shared_ptr<EmbreeRaytraceContext>;
	static SharedPtr create(std::shared_ptr<EmbreeDevice> pDevice);

	~EmbreeRaytraceContext();

	//void raytrace(RtProgram* pProgram, RtProgramVars* pVars, uint32_t width, uint32_t height, uint32_t depth);

protected:
	EmbreeRaytraceContext(std::shared_ptr<EmbreeDevice> pDevice);

	std::shared_ptr<EmbreeDevice> mpDevice;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_EMBREE_EMBREERAYTRACECONTEXT_H_