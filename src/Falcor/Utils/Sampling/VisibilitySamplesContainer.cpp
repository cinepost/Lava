#include "stdafx.h"

#include <numeric>

#include "Falcor/Core/API/RenderContext.h"

#include "Falcor/Utils/StringUtils.h"
#include "lava_utils_lib/logging.h"
#include "VisibilitySamplesContainer.h"


namespace Falcor {

namespace {
	const std::string kOpaqueSortShaderFilename = "Utils/Sampling/VisibilitySamplesContainer.OpaqueSort.cs.slang";
	const std::string kTransparentSortShaderFilename = "Utils/Sampling/VisibilitySamplesContainer.TransparentSort.cs.slang";

	const std::string kShaderFilename = "Utils/Sampling/VisibilitySamplesContainer.slang";
	const std::string kMaterialDataName = "materialData";
	const std::string kMaterialSamplersName = "materialSamplers";
	const std::string kMaterialTexturesName = "materialTextures";
	const std::string kExtendedTexturesDataName = "extendedTexturesData";
	const std::string kVirtualTexturesDataName = "virtualTexturesData";
	const std::string kVirtualPagesResidencyDataName = "virtualPagesResidencyData";
	const std::string kMaterialUDIMTilesTableName = "udimTextureTilesTable";
	const std::string kMaterialUDIMTilesTableBufferName = "udimTextureTilesTableBuffer";
	const std::string kMaterialBuffersName = "materialBuffers";

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
}

bool VisibilitySamplesContainer::hasTransparentSamples() const {
	
	return mMaxTransparentSamplesCountPP > 0;
}

void VisibilitySamplesContainer::sort() {
	sortOpaqueSamples();
	sortTransparentSamples();
}

void VisibilitySamplesContainer::sortOpaqueSamples() {
	if(is_set(mFlags, VisibilitySamplesContainerFlags::OpaqueSamplesSorted)) return;
	
	mFlags |= VisibilitySamplesContainerFlags::OpaqueSamplesSorted;
}

void VisibilitySamplesContainer::sortTransparentSamples() {
	if(is_set(mFlags, VisibilitySamplesContainerFlags::TransparentSamplesSorted)) return;
	
	mFlags |= VisibilitySamplesContainerFlags::TransparentSamplesSorted;
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
	if(!mpOpaqueSamplesBuffer || (mpOpaqueSamplesBuffer->getWidth(0) * mpOpaqueSamplesBuffer->getHeight(0)) != mMaxOpaqueSamplesCount) {
		mpOpaqueSamplesBuffer = Texture::create2D(mpDevice, mResolution.x, mResolution.y, mOpaqueSampleDataFormat, 1, 1, nullptr, Texture::BindFlags::ShaderResource | Texture::BindFlags::UnorderedAccess);
		mpOpaqueSamplesBuffer->setName("VisibilitySamplesContainer::opaqueSamplesBuffer");	
	}

	if(!mpOpaqueVisibilitySamplesPositionBuffer || mpOpaqueVisibilitySamplesPositionBuffer->getElementCount() != mMaxOpaqueSamplesCount) {
		mpOpaqueVisibilitySamplesPositionBuffer = Buffer::createStructured(mpDevice, sizeof(uint32_t), mMaxOpaqueSamplesCount, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
		mpOpaqueVisibilitySamplesPositionBuffer->setName("VisibilitySamplesContainer::opaqueVisibilitySamplePositionBuffer");	
	}

	if(!mpRootTransparentSampleOffsetBufferPP || (mpRootTransparentSampleOffsetBufferPP->getWidth(0) * mpRootTransparentSampleOffsetBufferPP->getHeight(0)) != mMaxOpaqueSamplesCount) {
		mpRootTransparentSampleOffsetBufferPP = Texture::create2D(mpDevice, mResolution.x, mResolution.y, ResourceFormat::R32Uint, 1, 1, nullptr, Texture::BindFlags::ShaderResource | Texture::BindFlags::UnorderedAccess);
		mpRootTransparentSampleOffsetBufferPP->setName("VisibilitySamplesContainer::rootTransparentSampleOffsetBufferPP");	
	}

	if(!mpTransparentVisibilitySamplesBuffer || mpTransparentVisibilitySamplesBuffer->getElementCount() != mMaxTransparentSamplesCount) {
		mpTransparentVisibilitySamplesBuffer = Buffer::createStructured(mpDevice, sizeof(TransparentVisibilitySample), mMaxTransparentSamplesCount, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
		mpTransparentVisibilitySamplesBuffer->setName("VisibilitySamplesContainer::transparentVisibilitySamplesBuffer");
	}

	if(!mpVisibilitySamplesCountBuffer || (mpVisibilitySamplesCountBuffer->getWidth(0) * mpVisibilitySamplesCountBuffer->getHeight(0)) != mMaxOpaqueSamplesCount) {
		mpVisibilitySamplesCountBuffer = Texture::create2D(mpDevice, mResolution.x, mResolution.y, ResourceFormat::R32Uint, 1, 1, nullptr, Texture::BindFlags::ShaderResource | Texture::BindFlags::UnorderedAccess);
		mpVisibilitySamplesCountBuffer->setName("VisibilitySamplesContainer::visibilitySamplesCountBuffer");	
	}
}

void VisibilitySamplesContainer::resize(uint width, uint height) {
	resize(width, height, mMaxTransparentSamplesCountPP);
}

void VisibilitySamplesContainer::setMaxTransparencySamplesCountPP(uint maxTransparentSamplesCountPP) {
	LLOG_WRN << "VisibilitySamplesContainer::setMaxTransparencySamplesCountPP " << maxTransparentSamplesCountPP;
	resize(mResolution.x, mResolution.y, maxTransparentSamplesCountPP);
}

void VisibilitySamplesContainer::resize(uint width, uint height, uint maxTransparentSamplesCountPP) {
	maxTransparentSamplesCountPP = std::min(maxTransparentSamplesCountPP, kMaxTransparentSamplesCountPP);
	if((mResolution == uint2({width, height})) && (mMaxTransparentSamplesCountPP == maxTransparentSamplesCountPP)) return;

	mResolution.x = width;
	mResolution.y = height;
	mMaxOpaqueSamplesCount = width * height;
	mMaxTransparentSamplesCountPP = maxTransparentSamplesCountPP;
	mMaxTransparentSamplesCount = mMaxOpaqueSamplesCount * mMaxTransparentSamplesCountPP;

	mpParameterBlock = nullptr;
	//createParameterBlock();
}

void VisibilitySamplesContainer::createParameterBlock() {
	// Create / re-create buffers.
	createBuffers();

	// Create parameter block.
	Program::DefineList defines = getDefines();
	defines.add("VISIBILITY_CONTAINER_PARAMETER_BLOCK");
	//defines.add("VISIBILITY_SAMPLES_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT_PP", std::to_string(kDefaultTransparentSamplesCountPP));
	//defines.add("VISIBILITY_SAMPLES_CONTAINER_LIMIT_TRANSPARENT_SAMPLES_COUNT_PP", mLimitTransparentSamplesCountPP ? "1" : "0");

	auto pPass = ComputePass::create(mpDevice, kShaderFilename, "main", defines);
	auto pReflector = pPass->getProgram()->getReflector()->getParameterBlock("gVisibilitySamplesContainer");
	assert(pReflector);

	mpParameterBlock = ParameterBlock::create(mpDevice, pReflector);
	assert(mpParameterBlock);

	// Bind resources to parameter block.
	mpParameterBlock["resolution"] = mResolution;
	mpParameterBlock["maxTransparentSamplesCount"] = mMaxTransparentSamplesCount;
	mpParameterBlock["maxTransparentSamplesCountPP"] = mMaxTransparentSamplesCountPP;
	mpParameterBlock["limitTransparentSamplesCountPP"] = mLimitTransparentSamplesCountPP;

	LLOG_WRN << "Max transparent samples count PP " << mMaxTransparentSamplesCountPP;

	mpParameterBlock["flags"] = static_cast<uint32_t>(mFlags);

	mpParameterBlock["infoBuffer"] = mpInfoBuffer;

	mpParameterBlock["opaqueVisibilitySamplesBuffer"] = mpOpaqueSamplesBuffer ? mpOpaqueSamplesBuffer : nullptr;
	mpParameterBlock["opaqueVisibilitySamplesPositionBuffer"] = mpOpaqueVisibilitySamplesPositionBuffer ? mpOpaqueVisibilitySamplesPositionBuffer : nullptr;
	mpParameterBlock["rootTransparentSampleOffsetBufferPP"] = mpRootTransparentSampleOffsetBufferPP ? mpRootTransparentSampleOffsetBufferPP : nullptr;

	mpParameterBlock["transparentVisibilitySamplesCountBuffer"] = mpVisibilitySamplesCountBuffer;
	mpParameterBlock["transparentVisibilitySamplesBuffer"]  = mpTransparentVisibilitySamplesBuffer;

	mpParameterBlock["alphaThresholdMin"] = mAlphaThresholdMin;
	mpParameterBlock["alphaThresholdMax"] = mAlphaThresholdMax;
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

	if(mpVisibilitySamplesCountBuffer) pRenderContext->clearUAV(mpVisibilitySamplesCountBuffer->getUAV().get(), uint4(0));

	if(mpOpaqueSamplesBuffer) pRenderContext->clearUAV(mpOpaqueSamplesBuffer->getUAV().get(), uint4(0));
	if(mpTransparentVisibilitySamplesBuffer) pRenderContext->clearUAV(mpTransparentVisibilitySamplesBuffer->getUAV().get(), uint4(0));

	if(mpOpaqueVisibilitySamplesPositionBuffer) pRenderContext->clearUAV(mpOpaqueVisibilitySamplesPositionBuffer->getUAV().get(), uint4(0));
	if(mpRootTransparentSampleOffsetBufferPP) pRenderContext->clearUAV(mpRootTransparentSampleOffsetBufferPP->getUAV().get(), uint4(UINT32_MAX));

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
	const int32_t* pInfoBufferData = reinterpret_cast<const int32_t*>(mpInfoBuffer->map(Buffer::MapType::Read));

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
	return mpInfoBufferData[VISIBILITY_CONTAINER_INFOBUFFER_MAX_TRANSPARENT_LAYERS_COUNT];
}

void VisibilitySamplesContainer::printStats() const {

}

}  // namespace Falcor

