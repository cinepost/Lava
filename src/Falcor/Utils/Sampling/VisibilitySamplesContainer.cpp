#include "stdafx.h"

#include <numeric>

#include "Falcor/Core/API/RenderContext.h"

#include "Falcor/Utils/StringUtils.h"
#include "lava_utils_lib/logging.h"
#include "VisibilitySamplesContainer.h"

#include "VisibilitySamplesContainer.slangh"


namespace Falcor {

namespace {
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

	const uint8_t kDefaultTransparentSamplesCount = 4;
	const uint8_t kMaxTransparentSamplesCount = 256;
}

VisibilitySamplesContainer::SharedPtr VisibilitySamplesContainer::create(Device::SharedPtr pDevice, uint2 resolution, uint8_t maxTransparentSamplesCount) {
	if(resolution[0] == 0 || resolution[1] == 0) {
		LLOG_ERR << "VisibilitySamplesContainer resolution must be greater than 0 !!!";
		return nullptr;
	}

	if(maxTransparentSamplesCount > kMaxTransparentSamplesCount) {
		LLOG_WRN << "VisibilitySamplesContainer maxTransparentSamplesCount should be less than " << kMaxTransparentSamplesCount << " !";
	}

	return SharedPtr(new VisibilitySamplesContainer(pDevice, resolution, std::min(maxTransparentSamplesCount, kMaxTransparentSamplesCount)));
}

VisibilitySamplesContainer::VisibilitySamplesContainer(Device::SharedPtr pDevice, uint2 resolution, uint8_t maxTransparentSamplesCount): mpDevice(pDevice), mResolution(resolution), mMaxTransparentSamplesCount(maxTransparentSamplesCount) {

}

bool VisibilitySamplesContainer::hasTransparentSamples() const {
	
	return mMaxTransparentSamplesCount > 0;
}

void VisibilitySamplesContainer::sort() {
	
}

Shader::DefineList VisibilitySamplesContainer::getDefaultDefines() {
	Shader::DefineList defines;
	defines.add("VISIBILITY_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT", std::to_string(kDefaultTransparentSamplesCount));

	return defines;
}

Shader::DefineList VisibilitySamplesContainer::getDefines() const {
	Shader::DefineList defines;
	defines.add("VISIBILITY_CONTAINER_MAX_TRANSPARENT_SAMPLES_COUNT", std::to_string(mMaxTransparentSamplesCount));

	return defines;
}

void VisibilitySamplesContainer::createBuffers() {
	const uint32_t opaqueSamplesCount = mResolution[0] * mResolution[1];
	const uint32_t transparentSamplesCount = opaqueSamplesCount * mMaxTransparentSamplesCount;

	if(!mpOpaqueSamplesBuffer || (mpOpaqueSamplesBuffer->getWidth(0) * mpOpaqueSamplesBuffer->getHeight(0)) != opaqueSamplesCount) {
		mpOpaqueSamplesBuffer = Texture::create2D(mpDevice, mResolution[0], mResolution[1], mOpaqueSampleDataFormat, 1, 1, nullptr, Texture::BindFlags::ShaderResource | Texture::BindFlags::UnorderedAccess);
		mpOpaqueSamplesBuffer->setName("VisibilitySamplesContainer::opaqueSamplesBuffer");	
	}

	if(!mpOpaqueSamplesExtraDataBuffer || (mpOpaqueSamplesExtraDataBuffer->getWidth(0) * mpOpaqueSamplesExtraDataBuffer->getHeight(0)) != opaqueSamplesCount) {
		mpOpaqueSamplesExtraDataBuffer = Texture::create2D(mpDevice, mResolution[0], mResolution[1], mOpaqueSampleExtraDataFormat, 1, 1, nullptr, Texture::BindFlags::ShaderResource | Texture::BindFlags::UnorderedAccess);
		mpOpaqueSamplesExtraDataBuffer->setName("VisibilitySamplesContainer::opaqueSamplesExtraDataBuffer");	
	}

	if(!mpTransparentVisibilitySamplesBuffer || mpTransparentVisibilitySamplesBuffer->getElementCount() != transparentSamplesCount) {
		mpTransparentVisibilitySamplesBuffer = Buffer::createStructured(mpDevice, sizeof(TransparentVisibilitySample), transparentSamplesCount, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
		mpTransparentVisibilitySamplesBuffer->setName("VisibilitySamplesContainer::transparentVisibilitySamplesBuffer");
	}
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

	// Verify that the material data struct size on the GPU matches the host-side size.
	//auto reflVar = mpParameterBlock->getReflection()->findMember(kMaterialDataName);
	//assert(reflVar);
	//auto reflResType = reflVar->getType()->asResourceType();
	//assert(reflResType && reflResType->getType() == ReflectionResourceType::Type::StructuredBuffer);
	//auto byteSize = reflResType->getStructType()->getByteSize();
	
	//if (byteSize != sizeof(MaterialDataBlob)) {
	//	throw std::runtime_error("VisibilitySamplesContainer material data buffer has unexpected struct size");
	//}

	// Create / re-create buffers.
	createBuffers();

	// Bind resources to parameter block.
	mpParameterBlock["resolution"] = mResolution;

	mpParameterBlock["opaqueVisibilityBuffer"] = mpOpaqueSamplesBuffer ? mpOpaqueSamplesBuffer : nullptr;
	mpParameterBlock["opaqueVisibilityExtraDataBuffer"] = mpOpaqueSamplesExtraDataBuffer ? mpOpaqueSamplesExtraDataBuffer : nullptr;

	mpParameterBlock["transparentVisibilitySamplesBuffer"]  = mpTransparentVisibilitySamplesBuffer ? mpTransparentVisibilitySamplesBuffer : nullptr;
}

}  // namespace Falcor

