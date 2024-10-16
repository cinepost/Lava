#include "stdafx.h"

#include <numeric>

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/IndirectCommands.h"

#include "Falcor/Utils/StringUtils.h"
#include "Falcor/Utils/Timing/SimpleProfiler.h"

#include "lava_utils_lib/logging.h"
#include "VisibilitySamplesContainer.h"


namespace Falcor {

namespace {
	const std::string kOpaqueSortShaderFilename = "Utils/Sampling/VisibilitySamplesContainer.OpaqueSort.cs.slang";
	const std::string kTransparentRootsSortShaderFilename = "Utils/Sampling/VisibilitySamplesContainer.TransparentRootsSort.cs.slang";
	const std::string kTransparentOrderSortShaderFilename = "Utils/Sampling/VisibilitySamplesContainer.TransparentOrderSort.cs.slang";
	const std::string kFinalizeIndirectArgsShaderFilename = "Utils/Sampling/VisibilitySamplesContainer.FinalizeIndirectArgs.cs.slang";

	const std::string kShaderFilename = "Utils/Sampling/VisibilitySamplesContainer.slang";
	const std::string kOpaqueSamplesDataName = "opaqueVisibilitySamplesBuffer";
	const std::string kVisibilityContainerParameterBlockName = "gVisibilityContainer";

	const uint		kDefaultTransparentSamplesCountPP = 4;
	const uint 		kMaxTransparentSamplesCountPP = 64;
	const size_t  kInfoBufferSize = 16;

	const float kAlphaThresholdMin = 0.001f;
	const float kAlphaThresholdMax = 0.996f;

	const DispatchArguments kBaseIndirectArgs = { 0, 1, 1 };
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
	mLimitTransparentSamplesCountPP = kDefaultLimitTransparentSamplesCountPP;

	mpOpaqueSamplesExternalTexture = nullptr;
	mpOpaqueCombinedNormalsExternalTexture = nullptr;
	mpOpaqueDepthExternalTexture = nullptr;
	mpOpaqueDepthExternalBuffer = nullptr;

	mpInfoBuffer = Buffer::createStructured(mpDevice, sizeof(uint32_t), kInfoBufferSize, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
	mpInfoBuffer->setName("VisibilitySamplesContainer::infoBuffer");

	mShadingThreadGroupSize = {1024, 1, 1};

	resize(resolution.x, resolution.y, maxTransparentSamplesCountPP);
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

	sortOpaqueSamples(pRenderContext);
	sortTransparentSamplesRoots(pRenderContext);
	sortFinalizeIndirectArgs(pRenderContext);

	sortTransparentSamplesOrder(pRenderContext);
}

void VisibilitySamplesContainer::sortOpaqueSamples(RenderContext* pRenderContext) {
	if(!mSortingEnabled || is_set(mFlags, VisibilitySamplesContainerFlags::OpaqueSamplesSorted)) return;
	
	LLOG_TRC << "VisibilitySamplesContainer::sortOpaqueSamples()";

	if(!mpOpaqueSortingPass) {
		Program::DefineList defines;
		defines.add(getDefines());
		mpOpaqueSortingPass = ComputePass::create(mpDevice, kOpaqueSortShaderFilename, "main", defines);
		mpOpaqueSortingPass[kVisibilityContainerParameterBlockName].setParameterBlock(getParameterBlock());
	}

	mpOpaqueSortingPass[kVisibilityContainerParameterBlockName]["flags"] = static_cast<uint32_t>(mFlags);
	mpOpaqueSortingPass->execute(pRenderContext, mResolution.x * mResolution.y, 1, 1);

	mFlags |= VisibilitySamplesContainerFlags::OpaqueSamplesSorted;
}

void VisibilitySamplesContainer::sortTransparentSamplesRoots(RenderContext* pRenderContext) {
	if(!mSortingEnabled || is_set(mFlags, VisibilitySamplesContainerFlags::TransparentRootsSorted)) return;

	LLOG_TRC << "VisibilitySamplesContainer::sortTransparentSamplesRoots()";

	if(!mpTransparentRootsSortingPass) {
		Program::DefineList defines;
		defines.add(getDefines());
		mpTransparentRootsSortingPass = ComputePass::create(mpDevice, kTransparentRootsSortShaderFilename, "main", defines);
		mpTransparentRootsSortingPass[kVisibilityContainerParameterBlockName].setParameterBlock(getParameterBlock());
	}

	mpTransparentRootsSortingPass[kVisibilityContainerParameterBlockName]["flags"] = static_cast<uint32_t>(mFlags);
	mpTransparentRootsSortingPass->execute(pRenderContext, mResolution.x * mResolution.y, 1, 1);
	
	mFlags |= VisibilitySamplesContainerFlags::TransparentRootsSorted;
}

void VisibilitySamplesContainer::sortTransparentSamplesOrder(RenderContext* pRenderContext) {
	if(!mSortingEnabledPP || is_set(mFlags, VisibilitySamplesContainerFlags::TransparentListsSorted)) return;

	LLOG_TRC << "VisibilitySamplesContainer::sortTransparentSamplesOrder()";

	if(!mpTransparentOrderSortingPass) {
		Program::DefineList defines;
		defines.add(getDefines());
		defines.add("GROUP_SIZE_X", to_string(getShadingThreadGroupSize().x));
		mpTransparentOrderSortingPass = ComputePass::create(mpDevice, kTransparentOrderSortShaderFilename, "main", defines);
		mpTransparentOrderSortingPass[kVisibilityContainerParameterBlockName].setParameterBlock(getParameterBlock());
	}

	mpTransparentOrderSortingPass[kVisibilityContainerParameterBlockName]["flags"] = static_cast<uint32_t>(mFlags);
	mpTransparentOrderSortingPass->executeIndirect(pRenderContext, mpTransparentPassIndirectionArgsBuffer.get());

	mFlags |= VisibilitySamplesContainerFlags::TransparentListsSorted;
}

void VisibilitySamplesContainer::sortFinalizeIndirectArgs(RenderContext* pRenderContext) {
	LLOG_TRC << "VisibilitySamplesContainer::sortFinalizeIndirectArgs()";

	if(!mpFinalizeSortingPass) {
		Program::DefineList defines;
		defines.add(getDefines());
		mpFinalizeSortingPass = ComputePass::create(mpDevice, kFinalizeIndirectArgsShaderFilename, "main", defines);
		mpFinalizeSortingPass[kVisibilityContainerParameterBlockName].setParameterBlock(getParameterBlock());

		mpFinalizeSortingPass["gOpaqueIndirectionBuffer"] = mpOpaquePassIndirectionArgsBuffer;
		mpFinalizeSortingPass["gTransparentIndirectionBuffer"] = mpTransparentPassIndirectionArgsBuffer;
	}

	mpFinalizeSortingPass[kVisibilityContainerParameterBlockName]["flags"] = static_cast<uint32_t>(mFlags);

	auto cb = mpFinalizeSortingPass["CB"];
  cb["shadingThreadGroupSize"] = mShadingThreadGroupSize;

	mpFinalizeSortingPass->execute(pRenderContext, 1, 1, 1);
}

Shader::DefineList VisibilitySamplesContainer::getDefaultDefines() {
	Shader::DefineList defines;
	defines.add("VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP", std::to_string(kDefaultTransparentSamplesCountPP));
	defines.add("VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP", kDefaultLimitTransparentSamplesCountPP ? "1" : "0");
	defines.add("VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS", kDefaultStoreNormals ? "1" : "0");

	defines.add("VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_SAMPLES_TEXTURE", "0");
	defines.add("VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_NORMALS_TEXTURE", "0");
	defines.add("VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_DEPTH_TEXTURE", "0");
	defines.add("VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_DEPTH_BUFFER", "0");

	return defines;
}

Shader::DefineList VisibilitySamplesContainer::getDefines() const {
	Shader::DefineList defines;
	defines.add("VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP", mLimitTransparentSamplesCountPP ? std::to_string(mMaxTransparentSamplesCountPP) : std::to_string(kMaxTransparentSamplesCountPP));
	defines.add("VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP", mLimitTransparentSamplesCountPP ? "1" : "0");
	defines.add("VISIBILITY_SAMPLES_CONTAINER_STORE_NORMALS", mStoreCombinedNormals ? "1" : "0");

	defines.add("VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_SAMPLES_TEXTURE", mpOpaqueSamplesExternalTexture ? "1" : "0");
	defines.add("VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_NORMALS_TEXTURE", mpOpaqueCombinedNormalsExternalTexture ? "1" : "0");
	defines.add("VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_DEPTH_TEXTURE", mpOpaqueDepthExternalTexture ? "1" : "0");
	defines.add("VISIBILITY_SAMPLES_CONTAINER_USE_OPAQUE_DEPTH_BUFFER", mpOpaqueDepthExternalBuffer ? "1" : "0");

	return defines;
}

void VisibilitySamplesContainer::createBuffers() {
	SimpleProfiler profile("VisibilitySamplesContainer::createBuffers");

	assert(mpParameterBlock);

	if (!mpOpaquePassIndirectionArgsBuffer) {
    mpOpaquePassIndirectionArgsBuffer = Buffer::create(mpDevice, sizeof(DispatchArguments), ResourceBindFlags::IndirectArg, Buffer::CpuAccess::None, &kBaseIndirectArgs);
  }

  if (!mpTransparentPassIndirectionArgsBuffer) {
    mpTransparentPassIndirectionArgsBuffer = Buffer::create(mpDevice, sizeof(DispatchArguments), ResourceBindFlags::IndirectArg, Buffer::CpuAccess::None, &kBaseIndirectArgs);
  }

  if(!mpOpaqueSamplesExternalTexture) {
		if(!mpOpaqueSamplesBuffer || mpOpaqueSamplesBuffer->getElementCount() != mResolution1D) {
			mpOpaqueSamplesBuffer = Buffer::createStructured(mpDevice, mpParameterBlock[kOpaqueSamplesDataName], mResolution1D, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
			mpOpaqueSamplesBuffer->setName("VisibilitySamplesContainer::opaqueSamplesBuffer");	
		}
	} else {
		mpOpaqueSamplesBuffer = nullptr;
	}

	if(!mpOpaqueVisibilitySamplesPositionBufferPP || mpOpaqueVisibilitySamplesPositionBufferPP->getElementCount() != mResolution1D) {
		mpOpaqueVisibilitySamplesPositionBufferPP = Buffer::createStructured(mpDevice, sizeof(uint32_t), mResolution1D, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
		mpOpaqueVisibilitySamplesPositionBufferPP->setName("VisibilitySamplesContainer::opaqueVisibilitySamplePositionBuffer");	
	}

	if(!mpRootTransparentSampleOffsetBufferPP || mpRootTransparentSampleOffsetBufferPP->getElementCount() != mResolution1D) {
		mpRootTransparentSampleOffsetBufferPP = Buffer::createStructured(mpDevice, sizeof(uint32_t), mResolution1D, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
		mpRootTransparentSampleOffsetBufferPP->setName("VisibilitySamplesContainer::rootTransparentSampleOffsetBufferPP");	
	}

	if(!mpTransparentVisibilitySamplesBuffer || mpTransparentVisibilitySamplesBuffer->getElementCount() != mTransparentSamplesBufferSize) {
		mpTransparentVisibilitySamplesBuffer = Buffer::createStructured(mpDevice, sizeof(TransparentVisibilitySampleData), mTransparentSamplesBufferSize, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
		mpTransparentVisibilitySamplesBuffer->setName("VisibilitySamplesContainer::transparentVisibilitySamplesBuffer");
	}

	if(!mpTransparentVisibilitySamplesCountBufferPP || mpTransparentVisibilitySamplesCountBufferPP->getElementCount() != mResolution1D) {
		mpTransparentVisibilitySamplesCountBufferPP = Buffer::createStructured(mpDevice, sizeof(uint32_t), mResolution1D, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
		mpTransparentVisibilitySamplesCountBufferPP->setName("VisibilitySamplesContainer::transparentVisibilitySamplesCountBuffer");	
	}

	if(!mpTransparentVisibilitySamplesPositionBufferPP || mpTransparentVisibilitySamplesPositionBufferPP->getElementCount() != mResolution1D) {
		mpTransparentVisibilitySamplesPositionBufferPP = Buffer::createStructured(mpDevice, sizeof(uint32_t), mResolution1D, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
		mpTransparentVisibilitySamplesPositionBufferPP->setName("VisibilitySamplesContainer::transparentVisibilitySamplePositionBuffer");	
	}

	// Optional combined normals
	if(mStoreCombinedNormals) {
		if(!mpOpaqueCombinedNormalsExternalTexture) {
			if (!mpOpaqueCombinedNormalsBuffer || mpOpaqueCombinedNormalsBuffer->getElementCount() != mResolution1D) {
				mpOpaqueCombinedNormalsBuffer = Buffer::createStructured(mpDevice, sizeof(uint4), mResolution1D, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
				mpOpaqueCombinedNormalsBuffer->setName("VisibilitySamplesContainer::opaqueCombinedNormalsBuffer");	
			}
		} else {
			mpOpaqueCombinedNormalsBuffer = nullptr;
		}

		if(!mpTransparentCombinedNormalsBuffer || mpTransparentCombinedNormalsBuffer->getElementCount() != mTransparentSamplesBufferSize) {
			mpTransparentCombinedNormalsBuffer = Buffer::createStructured(mpDevice, sizeof(uint4), mTransparentSamplesBufferSize, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
			mpTransparentCombinedNormalsBuffer->setName("VisibilitySamplesContainer::transparentCombinedNormalsBuffer");
		} else {
			mpTransparentCombinedNormalsBuffer = nullptr;
		}
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
	mResolution1D = width * height;
	mMaxTransparentSamplesCountPP = maxTransparentSamplesCountPP;
	mTransparentSamplesBufferSize = mResolution1D * mMaxTransparentSamplesCountPP;

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

	mpParameterBlock["flags"] = static_cast<uint32_t>(mFlags);

	mpParameterBlock["infoBuffer"] = mpInfoBuffer;

	mpParameterBlock["opaqueVisibilitySamplesBuffer"] = mpOpaqueSamplesBuffer;
	mpParameterBlock["opaqueCombinedNormalsBuffer"] = mpOpaqueCombinedNormalsBuffer;
	mpParameterBlock["opaqueVisibilitySamplesPositionBufferPP"] = mpOpaqueVisibilitySamplesPositionBufferPP;
	mpParameterBlock["rootTransparentSampleOffsetBufferPP"] = mpRootTransparentSampleOffsetBufferPP;

	mpParameterBlock["transparentVisibilitySamplesPositionBufferPP"] = mpTransparentVisibilitySamplesPositionBufferPP;
	mpParameterBlock["transparentVisibilitySamplesCountBufferPP"] = mpTransparentVisibilitySamplesCountBufferPP;
	mpParameterBlock["transparentVisibilitySamplesBuffer"]  = mpTransparentVisibilitySamplesBuffer;
	mpParameterBlock["transparentCombinedNormalsBuffer"]  = mpTransparentCombinedNormalsBuffer;

	mpParameterBlock["opaqueVisibilitySamplesExternalTexture"] = mpOpaqueSamplesExternalTexture;
	mpParameterBlock["opaqueCombinedNormalsExternalTexture"] = mpOpaqueCombinedNormalsBuffer;
	mpParameterBlock["opaqueExternalDepthTexture"] = mpOpaqueDepthExternalTexture;

	mpParameterBlock["alphaThresholdMin"] = mAlphaThresholdMin;
	mpParameterBlock["alphaThresholdMax"] = mAlphaThresholdMax;

	mpParameterBlock["resolution1D"] = mResolution1D;
}

void VisibilitySamplesContainer::setExternalOpaqueSamplesTexture(const Texture::SharedPtr& pTexture) {
	if(!pTexture || mpOpaqueSamplesExternalTexture == pTexture) return;
	mpOpaqueSamplesExternalTexture = pTexture;
	mpOpaqueSamplesBuffer = nullptr;
	mpParameterBlock = nullptr;
}

void VisibilitySamplesContainer::setExternalOpaqueCombinedNormalsTexture(const Texture::SharedPtr& pTexture) {
	if(!pTexture || mpOpaqueCombinedNormalsExternalTexture == pTexture) return;
	mpOpaqueCombinedNormalsExternalTexture = pTexture;
	mpOpaqueCombinedNormalsBuffer = nullptr;
	mpParameterBlock = nullptr;
}

void VisibilitySamplesContainer::setExternalOpaqueDepthTexture(const Texture::SharedPtr& pTexture) {
	if(!pTexture || mpOpaqueDepthExternalTexture == pTexture) return;
	mpOpaqueDepthExternalTexture = pTexture;
	mpOpaqueDepthExternalBuffer = nullptr;
	mpTransparentOrderSortingPass = nullptr;
}

void VisibilitySamplesContainer::setExternalOpaqueDepthBuffer(const Buffer::SharedPtr& pBuffer) {
	if(!pBuffer || mpOpaqueDepthExternalBuffer == pBuffer) return;
	mpOpaqueDepthExternalBuffer = pBuffer;
	mpOpaqueDepthExternalTexture = nullptr;
	mpTransparentOrderSortingPass = nullptr;
}

void VisibilitySamplesContainer::beginFrame() {
	SimpleProfiler profile("VisibilitySamplesContainer::beginFrame");

	if(!mpParameterBlock) createParameterBlock();

	mFlags = VisibilitySamplesContainerFlags::None;
	
	if(hasCombinedNormals()) mFlags |= VisibilitySamplesContainerFlags::HasCombinedNormals;

	mpParameterBlock["flags"] = static_cast<uint32_t>(mFlags);

	auto pRenderContext = mpDevice->getRenderContext();

	pRenderContext->clearUAV(mpInfoBuffer->getUAV().get(), uint4(0));
	mpInfoBufferData.clear();

	if(mpOpaquePassIndirectionArgsBuffer) pRenderContext->clearUAV(mpOpaquePassIndirectionArgsBuffer->getUAV().get(), uint4(0));
	if(mpTransparentPassIndirectionArgsBuffer) pRenderContext->clearUAV(mpTransparentPassIndirectionArgsBuffer->getUAV().get(), uint4(0));

	// TODO: clear in shader ?
	if(!mpOpaqueSamplesExternalTexture && mpOpaqueSamplesBuffer) pRenderContext->clearUAV(mpOpaqueSamplesBuffer->getUAV().get(), uint4(0));
	if(mpRootTransparentSampleOffsetBufferPP) pRenderContext->clearUAV(mpRootTransparentSampleOffsetBufferPP->getUAV().get(), uint4(UINT32_MAX));

	// TODO: don't need this ?
	if(mpTransparentVisibilitySamplesCountBufferPP) pRenderContext->clearUAV(mpTransparentVisibilitySamplesCountBufferPP->getUAV().get(), uint4(0));
	//if(mpTransparentVisibilitySamplesBuffer) pRenderContext->clearUAV(mpTransparentVisibilitySamplesBuffer->getUAV().get(), uint4(UINT32_MAX));
	//if(mpOpaqueVisibilitySamplesPositionBufferPP) pRenderContext->clearUAV(mpOpaqueVisibilitySamplesPositionBufferPP->getUAV().get(), uint4(0));
	//if(mpTransparentVisibilitySamplesPositionBufferPP) pRenderContext->clearUAV(mpTransparentVisibilitySamplesPositionBufferPP->getUAV().get(), uint4(0));

	//pRenderContext->flush(true);
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
	LLOG_WRN << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Do not call VisibilitySamplesContainer::readInfoBufferData()";
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

uint VisibilitySamplesContainer::transparentListsCount() const {
	readInfoBufferData();
	return mpInfoBufferData[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_LISTS_COUNT_LOCATION];
}

uint VisibilitySamplesContainer::transparentSamplesCount() const {
	readInfoBufferData();
	return mpInfoBufferData[VISIBILITY_CONTAINER_INFOBUFFER_TRANSPARENT_SAMPLES_COUNT_LOCATION];
}

uint VisibilitySamplesContainer::maxTransparentLayersCount() const {
	readInfoBufferData();
	return mpInfoBufferData[VISIBILITY_CONTAINER_INFOBUFFER_MAX_TRANSPARENT_LAYERS_COUNT_LOCATION];
}

void VisibilitySamplesContainer::enableSorting(bool enabled) {
	if(mSortingEnabled == enabled) return;
	mSortingEnabled = enabled;
}

void VisibilitySamplesContainer::enableSortingPP(bool enabled) {
	if(mSortingEnabledPP == enabled) return;
	mSortingEnabledPP = enabled;
}

void VisibilitySamplesContainer::storeCombinedNormals(bool enabled) {
	if(mStoreCombinedNormals == enabled) return;
	mStoreCombinedNormals = enabled;

	if(!mStoreCombinedNormals) {
		mpOpaqueCombinedNormalsBuffer = nullptr;
		mpTransparentCombinedNormalsBuffer = nullptr;
	}

	mpParameterBlock = nullptr;
}

bool VisibilitySamplesContainer::hasCombinedNormals() const {
	if(!mStoreCombinedNormals) return false;
	if(!mpTransparentCombinedNormalsBuffer) return false;
	if(!mpOpaqueCombinedNormalsExternalTexture && !mpOpaqueCombinedNormalsBuffer) return false;
	
	return true;
}

void VisibilitySamplesContainer::printStats() const {
	size_t memUsageOpaque = 0;
	size_t memUsageTransparent = 0;
	size_t memUsage = 0;

	memUsageOpaque += mpOpaqueSamplesBuffer ? mpOpaqueSamplesBuffer->getSize() : 0;
	memUsageOpaque += mpOpaqueCombinedNormalsBuffer ? mpOpaqueCombinedNormalsBuffer->getSize() : 0;
	memUsageOpaque += mpOpaqueVisibilitySamplesPositionBufferPP ? mpOpaqueVisibilitySamplesPositionBufferPP->getSize() : 0;
	
	memUsageTransparent += mpRootTransparentSampleOffsetBufferPP ? mpRootTransparentSampleOffsetBufferPP->getSize() : 0;
	memUsageTransparent += mpTransparentVisibilitySamplesCountBufferPP ? mpTransparentVisibilitySamplesCountBufferPP->getSize() : 0;
	memUsageTransparent += mpTransparentVisibilitySamplesPositionBufferPP ? mpTransparentVisibilitySamplesPositionBufferPP->getSize() : 0;
	memUsageTransparent += mpTransparentVisibilitySamplesBuffer ? mpTransparentVisibilitySamplesBuffer->getSize() : 0;
	memUsageTransparent += mpTransparentCombinedNormalsBuffer ? mpTransparentCombinedNormalsBuffer->getSize() : 0;

	memUsage = memUsageOpaque + memUsageTransparent;

	LLOG_INF << "VisibilitySamplesContainer device opaque data memory usage: " << uint32_t(memUsageOpaque >> 20) << " MB.";
	LLOG_INF << "VisibilitySamplesContainer device transparent data memory usage: " << uint32_t(memUsageTransparent >> 20) << " MB.";
	LLOG_INF << "VisibilitySamplesContainer device total memory usage: " << uint32_t(memUsage >> 20) << " MB.";

	LLOG_INF << "VisibilitySamplesContainer used " << (mpOpaqueSamplesExternalTexture ? "external opaque samples texture" : "internal opaque samples buffer");

	if(mpOpaqueCombinedNormalsBuffer || mpOpaqueCombinedNormalsExternalTexture) {
		LLOG_INF << "VisibilitySamplesContainer used " << (mpOpaqueCombinedNormalsExternalTexture ? "external opaque normals texture" : "internal opaque normals buffer");
	}

	if(mpOpaqueDepthExternalTexture || mpOpaqueDepthExternalBuffer) {
		LLOG_INF << "VisibilitySamplesContainer used " << (mpOpaqueDepthExternalTexture ? "external depth texture" : "external depth buffer");
	} else {
		LLOG_INF << "VisibilitySamplesContainer used internal depth buffer";
	}

	LLOG_INF << "---- VisibilitySamplesContainer detailed resource usage list ------";
	
	if(mpOpaqueSamplesBuffer) {
		LLOG_INF << "VisibilitySamplesContainer device opaque samples buffer memory usage:  " << (uint32_t(mpOpaqueSamplesBuffer->getSize()) >> 20) << "MB.";
	}

	if(mpOpaqueCombinedNormalsBuffer) {
		LLOG_INF << "VisibilitySamplesContainer device opaque normals buffer memory usage:  " << (uint32_t(mpOpaqueCombinedNormalsBuffer->getSize()) >> 20) << "MB.";
	}

/*
	printf("mpOpaqueSamplesBuffer size %zu\n", mpOpaqueSamplesBuffer ? mpOpaqueSamplesBuffer->getSize() : zero);
	printf("mpOpaqueCombinedNormalsBuffer size %zu\n", mpOpaqueCombinedNormalsBuffer ? mpOpaqueCombinedNormalsBuffer->getSize() : zero);
	printf("mpOpaqueVisibilitySamplesPositionBufferPP size %zu\n", mpOpaqueVisibilitySamplesPositionBufferPP ? mpOpaqueVisibilitySamplesPositionBufferPP->getSize() : zero);
	printf("mpRootTransparentSampleOffsetBufferPP size %zu\n", mpRootTransparentSampleOffsetBufferPP ? mpRootTransparentSampleOffsetBufferPP->getSize() : zero);
	printf("mpTransparentVisibilitySamplesCountBufferPP size %zu\n", mpTransparentVisibilitySamplesCountBufferPP ? mpTransparentVisibilitySamplesCountBufferPP->getSize() : zero);
	printf("mpTransparentVisibilitySamplesPositionBufferPP size %zu\n", mpTransparentVisibilitySamplesPositionBufferPP ? mpTransparentVisibilitySamplesPositionBufferPP->getSize() : zero);
	printf("mpTransparentVisibilitySamplesBuffer size %zu\n", mpTransparentVisibilitySamplesBuffer ? mpTransparentVisibilitySamplesBuffer->getSize() : zero);
	printf("mpTransparentCombinedNormalsBuffer size %zu\n", mpTransparentCombinedNormalsBuffer ? mpTransparentCombinedNormalsBuffer->getSize() : zero);
*/
}

VisibilitySamplesContainer::~VisibilitySamplesContainer() {
	printStats();
}

}  // namespace Falcor

