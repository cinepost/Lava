#include "stdafx.h"

#include <numeric>

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/IndirectCommands.h"

#include "Falcor/Utils/StringUtils.h"
#include "lava_utils_lib/logging.h"
#include "VisibilitySamplesContainer.h"


namespace Falcor {

namespace {
	const std::string kOpaqueSortShaderFilename = "Utils/Sampling/VisibilitySamplesContainer.OpaqueSort.cs.slang";
	const std::string kTransparentRootsOrderShaderFilename = "Utils/Sampling/VisibilitySamplesContainer.TransparentRootsOrder.cs.slang";
	const std::string kTransparentSortShaderFilename = "Utils/Sampling/VisibilitySamplesContainer.TransparentSort.cs.slang";

	const std::string kShaderFilename = "Utils/Sampling/VisibilitySamplesContainer.slang";
	const std::string kOpaqueSamplesDataName = "opaqueVisibilitySamplesBuffer";
	const std::string kVisibilityContainerParameterBlockName = "gVisibilityContainer";

	const bool    kDefaultLimitTransparentSamplesCountPP = false;
	const uint		kDefaultTransparentSamplesCountPP = 4;
	const uint 		kMaxTransparentSamplesCountPP = 64;
	const size_t  kInfoBufferSize = 16;

	const float kAlphaThresholdMin = 0.001f;
	const float kAlphaThresholdMax = 0.996f;
}

VisibilitySamplesContainer::SharedPtr VisibilitySamplesContainer::create(Device::SharedPtr pDevice, uint2 resolution, uint maxTransparentSamplesCountPP) {
	if(resolution[0] == 0 || resolution[1] == 0) {
		LLOG_ERR << "VisibilitySamplesContainer resolution must be greater than 0 !!!";
		return nullptr;
	}

	if(maxTransparentSamplesCountPP > kMaxTransparentSamplesCountPP) {
		LLOG_WRN << "VisibilitySamplesContainer maxTransparentSamplesCountPP " << maxTransparentSamplesCountPP << " should be less than " << kMaxTransparentSamplesCountPP << " !";
	}

	return SharedPtr(new VisibilitySamplesContainer(pDevice, resolution, std::min(maxTransparentSamplesCountPP, kMaxTransparentSamplesCountPP)));
}

VisibilitySamplesContainer::VisibilitySamplesContainer(Device::SharedPtr pDevice, uint2 resolution, uint maxTransparentSamplesCountPP): mpDevice(pDevice), mResolution(resolution), mMaxTransparentSamplesCountPP(maxTransparentSamplesCountPP) {
	mFlags = VisibilitySamplesContainerFlags::None;

	mAlphaThresholdMin = kAlphaThresholdMin;
	mAlphaThresholdMax = kAlphaThresholdMax;
	mMaxTransparentSamplesCountPP = kDefaultTransparentSamplesCountPP;
	mLimitTransparentSamplesCountPP = kDefaultLimitTransparentSamplesCountPP;

	mpInfoBuffer = Buffer::createStructured(mpDevice, sizeof(uint32_t), kInfoBufferSize, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
	mpInfoBuffer->setName("VisibilitySamplesContainer::infoBuffer");

	mShadingThreadGroupSize = {256, 1, 1};

	static const DispatchArguments baseIndirectArgs = { 0, 1, 1 };

	mpOpaquePassIndirectionArgsBuffer = Buffer::create(mpDevice, sizeof(DispatchArguments), ResourceBindFlags::IndirectArg, Buffer::CpuAccess::None, &baseIndirectArgs);
  mpTransparentPassIndirectionArgsBuffer = Buffer::create(mpDevice, sizeof(DispatchArguments), ResourceBindFlags::IndirectArg, Buffer::CpuAccess::None, &baseIndirectArgs);
}

bool VisibilitySamplesContainer::hasTransparentSamples() const {
	
	return mMaxTransparentSamplesCountPP > 0;
}

void VisibilitySamplesContainer::setScene(const Scene::SharedPtr& pScene) {
	if(mpScene == pScene) return;
	mpScene = pScene;

	mHitInfoFormat = mpScene ? mpScene->getHitInfo().getFormat() : HitInfo::kDefaultFormat;

	mpParameterBlock = nullptr;
}

void VisibilitySamplesContainer::sort() {
	auto pRenderContext = mpDevice->getRenderContext();
	assert(pRenderContext);

	//orderTransparentRoots(pRenderContext);
	sortOpaqueSamples(pRenderContext);
	sortTransparentSamples(pRenderContext);
}

void VisibilitySamplesContainer::sortOpaqueSamples(RenderContext* pRenderContext) {
	if(is_set(mFlags, VisibilitySamplesContainerFlags::OpaqueSamplesSorted)) return;
	
	if(!mpOpaqueSortingPass) {
		Program::DefineList defines;
		defines.add(getDefines());

		//uint3 indirect->getProgram()->getReflector()->getThreadGroupSize();

		mpOpaqueSortingPass = ComputePass::create(mpDevice, kOpaqueSortShaderFilename, "main", defines);
		mpOpaqueSortingPass["gOpaqueIndirectionBuffer"] = mpOpaquePassIndirectionArgsBuffer;
		mpOpaqueSortingPass["gTransparentIndirectionBuffer"] = mpTransparentPassIndirectionArgsBuffer;
	
		mpOpaqueSortingPass[kVisibilityContainerParameterBlockName].setParameterBlock(getParameterBlock());
	}

	auto cb = mpOpaqueSortingPass["CB"];
  cb["shadingThreadGroupSize"] = mShadingThreadGroupSize;

	LLOG_WRN << "VisibilitySamplesContainer::sortOpaqueSamples()";
	mpOpaqueSortingPass->execute(pRenderContext, mOpaqueSamplesBufferSize, 1, 1);

	mFlags |= VisibilitySamplesContainerFlags::OpaqueSamplesSorted;
}

void VisibilitySamplesContainer::sortTransparentSamples(RenderContext* pRenderContext) {
	return;

	if(is_set(mFlags, VisibilitySamplesContainerFlags::TransparentSamplesSorted)) return;

	if(!mpTransparentSortingPass) {
		Program::DefineList defines;
		mpTransparentSortingPass = ComputePass::create(mpDevice, kTransparentSortShaderFilename, "main", defines);
	}

	mpTransparentSortingPass->executeIndirect(pRenderContext, mpTransparentPassIndirectionArgsBuffer.get());
	
	mFlags |= VisibilitySamplesContainerFlags::TransparentSamplesSorted;
}

void VisibilitySamplesContainer::orderTransparentRoots(RenderContext* pRenderContext) {
	return;

	if(is_set(mFlags, VisibilitySamplesContainerFlags::TransparentSamplesSorted)) return;

	if(!mpTransparentRootsOrderingPass) {
		Program::DefineList defines;
		mpTransparentRootsOrderingPass = ComputePass::create(mpDevice, kTransparentRootsOrderShaderFilename, "main", defines);
	}

	mpTransparentSortingPass->execute(pRenderContext, mResolution.x * mResolution.y, 1, 1);
}

Shader::DefineList VisibilitySamplesContainer::getDefaultDefines() {
	Shader::DefineList defines;
	defines.add("VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP", std::to_string(kDefaultTransparentSamplesCountPP));
	defines.add("VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP", kDefaultLimitTransparentSamplesCountPP ? "1" : "0");

	return defines;
}

Shader::DefineList VisibilitySamplesContainer::getDefines() const {
	Shader::DefineList defines;
	defines.add("VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP", mLimitTransparentSamplesCountPP ? std::to_string(mMaxTransparentSamplesCountPP) : std::to_string(kMaxTransparentSamplesCountPP));
	defines.add("VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP", mLimitTransparentSamplesCountPP ? "1" : "0");

	return defines;
}

void VisibilitySamplesContainer::createBuffers() {
	assert(mpParameterBlock);

	static const DispatchArguments baseIndirectArgs = { 0, 1, 1 };

	if (!mpOpaquePassIndirectionArgsBuffer) {
    mpOpaquePassIndirectionArgsBuffer = Buffer::create(mpDevice, sizeof(DispatchArguments), ResourceBindFlags::IndirectArg, Buffer::CpuAccess::None, &baseIndirectArgs);
  }

  if (!mpTransparentPassIndirectionArgsBuffer) {
    mpTransparentPassIndirectionArgsBuffer = Buffer::create(mpDevice, sizeof(DispatchArguments), ResourceBindFlags::IndirectArg, Buffer::CpuAccess::None, &baseIndirectArgs);
  }

	if(!mpOpaqueSamplesBuffer || mpOpaqueSamplesBuffer->getElementCount() != mOpaqueSamplesBufferSize) {
		mpOpaqueSamplesBuffer = Buffer::createStructured(mpDevice, mpParameterBlock[kOpaqueSamplesDataName], mOpaqueSamplesBufferSize, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
		mpOpaqueSamplesBuffer->setName("VisibilitySamplesContainer::opaqueSamplesBuffer");	
	}

	if(!mpOpaqueVisibilitySamplesPositionBuffer || mpOpaqueVisibilitySamplesPositionBuffer->getElementCount() != mOpaqueSamplesBufferSize) {
		mpOpaqueVisibilitySamplesPositionBuffer = Buffer::createStructured(mpDevice, sizeof(uint32_t), mOpaqueSamplesBufferSize, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
		mpOpaqueVisibilitySamplesPositionBuffer->setName("VisibilitySamplesContainer::opaqueVisibilitySamplePositionBuffer");	
	}

	if(!mpRootTransparentSampleOffsetBufferPP || mpRootTransparentSampleOffsetBufferPP->getElementCount() != mOpaqueSamplesBufferSize) {
		mpRootTransparentSampleOffsetBufferPP = Buffer::createStructured(mpDevice, sizeof(uint32_t), mOpaqueSamplesBufferSize, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
		mpRootTransparentSampleOffsetBufferPP->setName("VisibilitySamplesContainer::rootTransparentSampleOffsetBufferPP");	
	}

	if(!mpTransparentVisibilitySamplesBuffer || mpTransparentVisibilitySamplesBuffer->getElementCount() != mTransparentSamplesBufferSize) {
		mpTransparentVisibilitySamplesBuffer = Buffer::createStructured(mpDevice, sizeof(TransparentVisibilitySample), mTransparentSamplesBufferSize, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
		mpTransparentVisibilitySamplesBuffer->setName("VisibilitySamplesContainer::transparentVisibilitySamplesBuffer");
	}

	if(!mpVisibilitySamplesCountBufferPP || mpVisibilitySamplesCountBufferPP->getElementCount() != mOpaqueSamplesBufferSize) {
		mpVisibilitySamplesCountBufferPP = Buffer::createStructured(mpDevice, sizeof(ResourceFormat::R32Uint), mOpaqueSamplesBufferSize, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
		mpVisibilitySamplesCountBufferPP->setName("VisibilitySamplesContainer::visibilitySamplesCountBuffer");	
	}
}

void VisibilitySamplesContainer::resize(uint width, uint height) {
	resize(width, height, mMaxTransparentSamplesCountPP);
}

void VisibilitySamplesContainer::setMaxTransparencySamplesCountPP(uint maxTransparentSamplesCountPP) {
	resize(mResolution.x, mResolution.y, maxTransparentSamplesCountPP);
}

void VisibilitySamplesContainer::resize(uint width, uint height, uint maxTransparentSamplesCountPP) {
	maxTransparentSamplesCountPP = std::min(maxTransparentSamplesCountPP, kMaxTransparentSamplesCountPP);
	if((mResolution == uint2({width, height})) && (mMaxTransparentSamplesCountPP == maxTransparentSamplesCountPP)) return;

	mResolution.x = width;
	mResolution.y = height;
	mOpaqueSamplesBufferSize = width * height;
	mMaxTransparentSamplesCountPP = maxTransparentSamplesCountPP;
	mTransparentSamplesBufferSize = mOpaqueSamplesBufferSize * mMaxTransparentSamplesCountPP;

	mpParameterBlock = nullptr;
}

void VisibilitySamplesContainer::createParameterBlock() {
	// Create parameter block.
	Program::DefineList defines = getDefines();
	defines.add("VISIBILITY_CONTAINER_PARAMETER_BLOCK");
	
	auto pPass = ComputePass::create(mpDevice, kShaderFilename, "main", defines);
	auto pReflector = pPass->getProgram()->getReflector()->getParameterBlock("gVisibilitySamplesContainer");
	assert(pReflector);

	mpParameterBlock = ParameterBlock::create(mpDevice, pReflector);
	assert(mpParameterBlock);

	// Create / re-create buffers.
	createBuffers();

	// Bind resources to parameter block.
	mpParameterBlock["resolution"] = mResolution;
	mpParameterBlock["maxTransparentSamplesCount"] = mTransparentSamplesBufferSize;
	mpParameterBlock["maxTransparentSamplesCountPP"] = mLimitTransparentSamplesCountPP ? mMaxTransparentSamplesCountPP : kMaxTransparentSamplesCountPP;
	mpParameterBlock["limitTransparentSamplesCountPP"] = mLimitTransparentSamplesCountPP;

	LLOG_DBG << "Max transparent samples count PP " << mMaxTransparentSamplesCountPP;

	mpParameterBlock["flags"] = static_cast<uint32_t>(mFlags);

	mpParameterBlock["infoBuffer"] = mpInfoBuffer;

	mpParameterBlock["opaqueVisibilitySamplesBuffer"] = mpOpaqueSamplesBuffer;
	mpParameterBlock["opaqueVisibilitySamplesPositionBuffer"] = mpOpaqueVisibilitySamplesPositionBuffer;
	mpParameterBlock["rootTransparentSampleOffsetBufferPP"] = mpRootTransparentSampleOffsetBufferPP;

	mpParameterBlock["transparentVisibilitySamplesCountBufferPP"] = mpVisibilitySamplesCountBufferPP;
	mpParameterBlock["transparentVisibilitySamplesBuffer"]  = mpTransparentVisibilitySamplesBuffer;

	mpParameterBlock["alphaThresholdMin"] = mAlphaThresholdMin;
	mpParameterBlock["alphaThresholdMax"] = mAlphaThresholdMax;

	mpParameterBlock["opaqueSamplesBufferSize"] = mOpaqueSamplesBufferSize;
}

void VisibilitySamplesContainer::setDepthBufferTexture(Texture::SharedPtr pTexture) {
	if(mpDepthTexture == pTexture) return;
	mpDepthTexture = pTexture;
	mpTransparentSortingPass = nullptr;
}

void VisibilitySamplesContainer::beginFrame() {
	if(!mpParameterBlock) createParameterBlock();

	auto pRenderContext = mpDevice->getRenderContext();

	pRenderContext->clearUAV(mpInfoBuffer->getUAV().get(), uint4(0));
	mpInfoBufferData.clear();

	if(mpVisibilitySamplesCountBufferPP) pRenderContext->clearUAV(mpVisibilitySamplesCountBufferPP->getUAV().get(), uint4(0));

	if(mpOpaqueSamplesBuffer) pRenderContext->clearUAV(mpOpaqueSamplesBuffer->getUAV().get(), uint4(0));
	if(mpTransparentVisibilitySamplesBuffer) pRenderContext->clearUAV(mpTransparentVisibilitySamplesBuffer->getUAV().get(), uint4(0));

	if(mpOpaqueVisibilitySamplesPositionBuffer) pRenderContext->clearUAV(mpOpaqueVisibilitySamplesPositionBuffer->getUAV().get(), uint4(0));
	if(mpRootTransparentSampleOffsetBufferPP) pRenderContext->clearUAV(mpRootTransparentSampleOffsetBufferPP->getUAV().get(), uint4(UINT32_MAX));

	if(mpOpaquePassIndirectionArgsBuffer) pRenderContext->clearUAV(mpOpaquePassIndirectionArgsBuffer->getUAV().get(), uint4(0, 1, 1, 0));
  if(mpTransparentPassIndirectionArgsBuffer) pRenderContext->clearUAV(mpTransparentPassIndirectionArgsBuffer->getUAV().get(), uint4(0, 1, 1, 0));

	mFlags = VisibilitySamplesContainerFlags::None;

	if(!mpParameterBlock) createParameterBlock();

	mpParameterBlock["flags"] = static_cast<uint32_t>(mFlags);
}

void VisibilitySamplesContainer::beginFrame() const {
	if(!mpParameterBlock) return;

	mpParameterBlock["flags"] = static_cast<uint32_t>(mFlags);
}

void VisibilitySamplesContainer::endFrame() {
	sort();
}

void VisibilitySamplesContainer::endFrame() const {

}

void VisibilitySamplesContainer::setLimitTransparentSamplesCountPP(bool limit) {
	if(mLimitTransparentSamplesCountPP == limit) return;
	mLimitTransparentSamplesCountPP = limit;
	
	mpParameterBlock = nullptr;
}

void VisibilitySamplesContainer::readInfoBufferData() const {
	if(!mpInfoBufferData.empty()) return;

	mpDevice->getRenderContext()->flush();
	const uint32_t* pInfoBufferData = reinterpret_cast<const uint32_t*>(mpInfoBuffer->map(Buffer::MapType::Read));

	mpInfoBufferData.resize(kInfoBufferSize);
	for(size_t i = 0; i < kInfoBufferSize; ++i) mpInfoBufferData[i] = pInfoBufferData[i];

	mpInfoBuffer->unmap();
}

uint VisibilitySamplesContainer::opaqueSamplesCount() const {
	readInfoBufferData();
	return mpInfoBufferData[VISIBILITY_CONTAINER_INFOBUFFER_OPAQUE_SAMPLES_COUNT_LOCATION];
}

uint VisibilitySamplesContainer::transparentSamplesCount() const {
	readInfoBufferData();
	return mpInfoBufferData[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_SAMPLES_COUNT_LOCATION];
}

uint VisibilitySamplesContainer::maxTransparentLayersCount() const {
	readInfoBufferData();
	return mpInfoBufferData[VISIBILITY_CONTAINER_INFOBUFFER_MAX_TRANSPARENT_LAYERS_COUNT_LOCATION];
}

void VisibilitySamplesContainer::printStats() const {

}

}  // namespace Falcor

