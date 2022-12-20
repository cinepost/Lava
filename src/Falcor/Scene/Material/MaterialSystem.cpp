/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "stdafx.h"

#include <numeric>

#include "Falcor/Core/API/RenderContext.h"

#include "Falcor/Utils/StringUtils.h"
#include "StandardMaterial.h"

#include "MaterialSystem.h"

namespace Falcor {

namespace {
	const std::string kShaderFilename = "Scene/Material/MaterialSystem.slang";
	const std::string kMaterialDataName = "materialData";
	const std::string kMaterialSamplersName = "materialSamplers";
	const std::string kMaterialTexturesName = "materialTextures";
	const std::string kMaterialUDIMTilesTableName = "udimTextureTilesTable";
	const std::string kMaterialBuffersName = "materialBuffers";

	const size_t kMaxSamplerCount = 1ull << MaterialHeader::kSamplerIDBits;
	const size_t kMaxTextureCount = 1ull << TextureHandle::kTextureIDBits;
	const size_t kMaxBufferCountPerMaterial = 1; // This is a conservative estimation of how many buffer descriptors to allocate per material. Most materials don't use any auxiliary data buffers.

	// Helper to check if a material is a standard material using the SpecGloss shading model.
	// We keep track of these as an optimization because most scenes do not use this shading model.
	bool isSpecGloss(const Material::SharedPtr& pMaterial) {
		if (pMaterial->getType() == MaterialType::Standard) {
			return std::static_pointer_cast<StandardMaterial>(pMaterial)->getShadingModel() == ShadingModel::SpecGloss;
		}
		return false;
	}
}

MaterialSystem::SharedPtr MaterialSystem::create(Device::SharedPtr pDevice) {
	return SharedPtr(new MaterialSystem(pDevice));
}

MaterialSystem::MaterialSystem(Device::SharedPtr pDevice): mpDevice(pDevice) {
	mpFence = GpuFence::create(mpDevice);
	mMaterialCountByType.resize((size_t)MaterialType::Count, 0);

	// Create a default texture sampler.
	Sampler::Desc desc;
	desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
	desc.setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap);
	//desc.setBorderColor({0.0f, 1.0f, 0.0f, 1.0f});
	desc.setMaxAnisotropy(16);
	desc.setLodParams(-1000.0f, 1000.0f, 0.0f);
	mpDefaultTextureSampler = Sampler::create(mpDevice, desc);

	desc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
	mpUDIMTileSampler = Sampler::create(mpDevice, desc);
}

void MaterialSystem::finalize() {
	// Pre-allocate texture and buffer descriptors based on final material count. This count will be reported by getDefines().
	// This is needed just because Falcor currently has no mechanism for reporting to user code that scene defines have changed.
	// Note we allocate more descriptors here than is typically needed as many materials do not use all texture slots,
	// so there is some room for adding materials at runtime after scene creation until running into this limit.
	// TODO: Remove this when unbounded descriptor arrays are supported (#1321).

	mTextureDescCount = textureManager()->getUDIMTextureTilesCount();;
	for(const auto& pMaterial: mMaterials) {
		mTextureDescCount += pMaterial->getTextureCount();
	}

	mTextureDescCount = textureManager()->getTextureDescCount(); // TODO: make it scene dependent!

	//mTextureDescCount = getMaterialCount() * (size_t)Material::TextureSlot::Count + textureManager()->getUDIMTextureTilesCount();
	mBufferDescCount = getMaterialCount() * kMaxBufferCountPerMaterial;
	mUDIMTextureCount = textureManager()->getUDIMTexturesCount();

	LLOG_DBG << "MaterialSystem::finalize-------------";

	LLOG_DBG << "MaterialSystem: materials count " << std::to_string(getMaterialCount());
	LLOG_DBG << "MaterialSystem: udim texture count " << std::to_string(mUDIMTextureCount);
	LLOG_DBG << "MaterialSystem: udimTilesCount: " << std::to_string(textureManager()->getUDIMTextureTilesCount());
	LLOG_DBG << "MaterialSystem: mTextureDescCount: " << std::to_string(mTextureDescCount);
}

void MaterialSystem::setDefaultTextureSampler(const Sampler::SharedPtr& pSampler) {
	mpDefaultTextureSampler = pSampler;
	for (const auto& pMaterial : mMaterials) {
		pMaterial->setDefaultTextureSampler(pSampler);
	}
}

uint32_t MaterialSystem::addTextureSampler(const Sampler::SharedPtr& pSampler) {
	assert(pSampler);
	auto isEqual = [&pSampler](const Sampler::SharedPtr& pOther) {
		return pSampler->getDesc() == pOther->getDesc();
	};

	// Reuse previously added samplers. We compare by sampler desc.
	if (auto it = std::find_if(mTextureSamplers.begin(), mTextureSamplers.end(), isEqual); it != mTextureSamplers.end()) {
		return (uint32_t)std::distance(mTextureSamplers.begin(), it);
	}

	// Add sampler.
	if (mTextureSamplers.size() >= kMaxSamplerCount) {
		throw std::runtime_error("Too many samplers");
	}
	const uint32_t samplerID = static_cast<uint32_t>(mTextureSamplers.size());

	mTextureSamplers.push_back(pSampler);
	mSamplersChanged = true;

	return samplerID;
}

uint32_t MaterialSystem::addBuffer(const Buffer::SharedPtr& pBuffer) {
	assert(pBuffer);

	// Reuse previously added buffers. We compare by pointer as the contents of the buffers is unknown.
	if (auto it = std::find_if(mBuffers.begin(), mBuffers.end(), [&](auto pOther) { return pBuffer == pOther; }); it != mBuffers.end()) {
		return (uint32_t)std::distance(mBuffers.begin(), it);
	}

	// Add buffer.
	if (mBuffers.size() >= mBufferDescCount) {
		throw std::runtime_error("Too many buffers");
	}
	const uint32_t bufferID = static_cast<uint32_t>(mBuffers.size());

	mBuffers.push_back(pBuffer);
	mBuffersChanged = true;

	return bufferID;
}

uint32_t MaterialSystem::addMaterial(const Material::SharedPtr& pMaterial) {
	assert(pMaterial);

	// Reuse previously added materials.
	if (auto it = std::find(mMaterials.begin(), mMaterials.end(), pMaterial); it != mMaterials.end()) {
		return (uint32_t)std::distance(mMaterials.begin(), it);
	}

	// Add material.
	if (mMaterials.size() >= std::numeric_limits<uint32_t>::max()) {
		throw std::runtime_error("Too many materials");
	}
	const uint32_t materialID = static_cast<uint32_t>(mMaterials.size());

	if (pMaterial->getDefaultTextureSampler() == nullptr) {
		pMaterial->setDefaultTextureSampler(mpDefaultTextureSampler);
	}

	pMaterial->registerUpdateCallback([this](auto flags) { mMaterialUpdates |= flags; });
	mMaterials.push_back(pMaterial);
	mMaterialsChanged = true;

	// Update metadata.
	mMaterialTypes.insert(pMaterial->getType());
	if (isSpecGloss(pMaterial)) mSpecGlossMaterialCount++;

	return materialID;
}

uint32_t MaterialSystem::getMaterialCountByType(const MaterialType type) const {
	size_t index = (size_t)type;
	assert(index < mMaterialCountByType.size());
	return mMaterialCountByType[index];
}

const Material::SharedPtr& MaterialSystem::getMaterial(const uint32_t materialID) const {
	assert(materialID < mMaterials.size());
	return mMaterials[materialID];
}

Material::SharedPtr MaterialSystem::getMaterialByName(const std::string& name) const {
	for (const auto& pMaterial : mMaterials) {
		if (pMaterial->getName() == name) return pMaterial;
	}
	return nullptr;
}

bool MaterialSystem::getMaterialIDByName(const std::string& name, uint32_t& materialID) const {
	uint32_t id = 0;
	for (const auto& pMaterial : mMaterials) {
		if (pMaterial->getName() == name) {
			materialID = id;
			return true;
		}
		id++;
	}
	return false;
}

size_t MaterialSystem::removeDuplicateMaterials(std::vector<uint32_t>& idMap) {
	std::vector<Material::SharedPtr> uniqueMaterials;
	idMap.resize(mMaterials.size());

	// Find unique set of materials.
	for (uint32_t id = 0; id < mMaterials.size(); ++id) {
		const auto& pMaterial = mMaterials[id];
		auto it = std::find_if(uniqueMaterials.begin(), uniqueMaterials.end(), [&pMaterial](const auto& m) { return m->isEqual(pMaterial); });
		if (it == uniqueMaterials.end()) {
			idMap[id] = (uint32_t)uniqueMaterials.size();
			uniqueMaterials.push_back(pMaterial);
		} else {
			LLOG_INF << "Removing duplicate material '" << pMaterial->getName() << "' (duplicate of '" << (*it)->getName() << "').";
			idMap[id] = (uint32_t)std::distance(uniqueMaterials.begin(), it);

			// Update metadata.
			if (isSpecGloss(pMaterial)) mSpecGlossMaterialCount--;
		}
	}

	size_t removed = mMaterials.size() - uniqueMaterials.size();
	if (removed > 0) {
		mMaterials = uniqueMaterials;
		mMaterialsChanged = true;
	}

	return removed;
}

bool MaterialSystem::hasUDIMTextures() const {
	return textureManager()->hasUDIMTextures(); // TODO: Calculate udim textures for this system only
}

bool MaterialSystem::hasSparseTextures() const {
	return textureManager()->hasSparseTextures(); // TODO: Calculate sparse textures for this system only
}

void MaterialSystem::optimizeMaterials() {
	// Gather a list of all textures to analyze.
	std::vector<std::pair<Material::SharedPtr, Material::TextureSlot>> materialSlots;
	std::vector<Texture::SharedPtr> textures;
	size_t maxCount = mMaterials.size() * (size_t)Material::TextureSlot::Count;
	materialSlots.reserve(maxCount);
	textures.reserve(maxCount);

	for (const auto& pMaterial : mMaterials) {
		for (uint32_t i = 0; i < (uint32_t)Material::TextureSlot::Count; i++) {
			auto slot = (Material::TextureSlot)i;
			if (auto pTexture = pMaterial->getTexture(slot)) {
				materialSlots.push_back({ pMaterial, slot });
				textures.push_back(pTexture);
			}
		}
	}

	if (textures.empty()) return;

	// Analyze the textures.
	LLOG_INF << "Analyzing " << std::to_string(textures.size()) << " material textures.";

	TextureAnalyzer::SharedPtr pAnalyzer = TextureAnalyzer::create(mpDevice);
	auto pResults = Buffer::create(mpDevice, textures.size() * TextureAnalyzer::getResultSize(), ResourceBindFlags::UnorderedAccess);
	pAnalyzer->analyze(mpDevice->getRenderContext(), textures, pResults);

	// Copy result to staging buffer for readback.
	// This is mostly to avoid a full flush and the associated perf warning.
	// We do not have any other useful GPU work, but unrelated GPU tasks can be in flight.
	auto pResultsStaging = Buffer::create(mpDevice, textures.size() * TextureAnalyzer::getResultSize(), ResourceBindFlags::None, Buffer::CpuAccess::Read);
	mpDevice->getRenderContext()->copyResource(pResultsStaging.get(), pResults.get());
	mpDevice->getRenderContext()->flush(false);
	mpFence->gpuSignal(mpDevice->getRenderContext()->getLowLevelData()->getCommandQueue());

	// Wait for results to become available. Then optimize the materials.
	mpFence->syncCpu();
	const TextureAnalyzer::Result* results = static_cast<const TextureAnalyzer::Result*>(pResultsStaging->map(Buffer::MapType::Read));
	Material::TextureOptimizationStats stats = {};

	for (size_t i = 0; i < textures.size(); i++) {
		materialSlots[i].first->optimizeTexture(materialSlots[i].second, results[i], stats);
	}

	pResultsStaging->unmap();

	// Log optimization stats.
	if (size_t totalRemoved = std::accumulate(stats.texturesRemoved.begin(), stats.texturesRemoved.end(), 0ull); totalRemoved > 0) {
		LLOG_INF << "Optimized materials by removing " << std::to_string(totalRemoved) << " constant textures.";
		for (size_t slot = 0; slot < (size_t)Material::TextureSlot::Count; slot++) {
			LLOG_INF << padStringToLength("  " + to_string((Material::TextureSlot)slot) + ":", 26) + std::to_string(stats.texturesRemoved[slot]);
		}
	}

	if (stats.disabledAlpha > 0) {
		LLOG_INF << "Optimized materials by disabling alpha test for " << std::to_string(stats.disabledAlpha) << " materials.";
	}
	if (stats.constantBaseColor > 0) {
		LLOG_WRN << "Scene has " << std::to_string(stats.constantBaseColor) << " base color maps of constant value with non-constant alpha channel.";
	}
	if (stats.constantNormalMaps > 0) {
		LLOG_WRN << "Scene has " << std::to_string(stats.constantNormalMaps) << " normal maps of constant value. Please update the asset to optimize performance.";
	}
}

Material::UpdateFlags MaterialSystem::update(bool forceUpdate) {
	Material::UpdateFlags flags = Material::UpdateFlags::None;

	// Update metadata if materials changed.
	if (mMaterialsChanged) {
		std::fill(mMaterialCountByType.begin(), mMaterialCountByType.end(), 0);
		for (const auto& pMaterial : mMaterials) {
			size_t index = (size_t)pMaterial->getType();
			assert(index < mMaterialCountByType.size());
			mMaterialCountByType[index]++;
		}
	}

	// Create parameter block if needed.
	if (!mpMaterialsBlock || mMaterialsChanged) {
		createParameterBlock();

		// Set update flags if parameter block changes.
		// TODO: We may want to introduce MaterialSystem::UpdateFlags instead of re-using the material flags.
		flags |= Material::UpdateFlags::DataChanged | Material::UpdateFlags::ResourcesChanged;

		forceUpdate = true; // Trigger full upload of all materials
	}

	// Update all materials.
	if (forceUpdate || mMaterialUpdates != Material::UpdateFlags::None) {
		for (uint32_t materialID = 0; materialID < (uint32_t)mMaterials.size(); ++materialID) {
			auto& pMaterial = mMaterials[materialID];
			const auto materialUpdates = pMaterial->update(this);

			if (forceUpdate || materialUpdates != Material::UpdateFlags::None) {
				uploadMaterial(materialID);

				flags |= materialUpdates;
			}
		}
	}

	// Update samplers.
	if (forceUpdate || mSamplersChanged) {
		auto var = mpMaterialsBlock[kMaterialSamplersName];
		for (size_t i = 0; i < mTextureSamplers.size(); i++) var[i] = mTextureSamplers[i];
	}

	// Update textures.
	if (forceUpdate || is_set(flags, Material::UpdateFlags::ResourcesChanged)) {
		textureManager()->finalize();

		std::vector<Texture::SharedPtr> textures;
		for(const auto& pMaterial: mMaterials) {
			pMaterial->getTextures(textures, true); // true to append instead of erasing vector
		}

		//textureManager()->setShaderData(mpMaterialsBlock[kMaterialTexturesName], textures);
		textureManager()->setShaderData(mpMaterialsBlock[kMaterialTexturesName], mTextureDescCount);

		textureManager()->setUDIMTableShaderData(mpMaterialsBlock[kMaterialUDIMTilesTableName], mUDIMTextureCount * 100);
	}

	// Update buffers.
	if (forceUpdate || mBuffersChanged) {
		auto var = mpMaterialsBlock[kMaterialBuffersName];
		for (size_t i = 0; i < mBuffers.size(); i++) var[i] = mBuffers[i];
	}

	mSamplersChanged = false;
	mBuffersChanged = false;
	mMaterialsChanged = false;
	mMaterialUpdates = Material::UpdateFlags::None;

	return flags;
}

MaterialSystem::MaterialStats MaterialSystem::getStats() const {
	MaterialStats s = {};

	s.materialTypeCount = mMaterialTypes.size();
	s.materialCount = mMaterials.size();
	s.materialOpaqueCount = 0;
	s.materialMemoryInBytes += mpMaterialDataBuffer ? mpMaterialDataBuffer->getSize() : 0;

	std::set<Texture::SharedPtr> textures;
	for (const auto& pMaterial : mMaterials) {
		for (uint32_t i = 0; i < (uint32_t)Material::TextureSlot::Count; i++) {
			auto pTexture = pMaterial->getTexture((Material::TextureSlot)i);
			if (pTexture) textures.insert(pTexture);
		}

		if (pMaterial->isOpaque()) s.materialOpaqueCount++;
	}

	s.textureCount = textures.size();
	s.textureCompressedCount = 0;
	s.textureTexelCount = 0;
	s.textureMemoryInBytes = 0;

	for (const auto& t : textures) {
		s.textureTexelCount += t->getTexelCount();
		s.textureMemoryInBytes += t->getTextureSizeInBytes();
		if (isCompressedFormat(t->getFormat())) s.textureCompressedCount++;
	}

	return s;
}

Shader::DefineList MaterialSystem::getDefaultDefines() {
	Shader::DefineList defines;
	defines.add("MATERIAL_SYSTEM_SAMPLER_DESC_COUNT", std::to_string(kMaxSamplerCount));
	defines.add("MATERIAL_SYSTEM_TEXTURE_DESC_COUNT", "0");
	defines.add("MATERIAL_SYSTEM_UDIM_TEXTURE_COUNT", "0");
	defines.add("MATERIAL_SYSTEM_BUFFER_DESC_COUNT", "0");
	defines.add("MATERIAL_SYSTEM_HAS_SPEC_GLOSS_MATERIALS", "0");

	return defines;
}

Shader::DefineList MaterialSystem::getDefines() const {
	Shader::DefineList defines;
	defines.add("MATERIAL_SYSTEM_SAMPLER_DESC_COUNT", std::to_string(kMaxSamplerCount));
	defines.add("MATERIAL_SYSTEM_TEXTURE_DESC_COUNT", std::to_string(mTextureDescCount));
	defines.add("MATERIAL_SYSTEM_UDIM_TEXTURE_COUNT", std::to_string(mUDIMTextureCount));
	defines.add("MATERIAL_SYSTEM_BUFFER_DESC_COUNT", std::to_string(mBufferDescCount));
	defines.add("MATERIAL_SYSTEM_HAS_SPEC_GLOSS_MATERIALS", mSpecGlossMaterialCount > 0 ? "1" : "0");

	return defines;
}

Program::TypeConformanceList MaterialSystem::getTypeConformances() const {
	Program::TypeConformanceList typeConformances;
	for (const auto type : mMaterialTypes) {
		typeConformances.add(getTypeConformances(type));
	}
	return typeConformances;
}

Program::TypeConformanceList MaterialSystem::getTypeConformances(const MaterialType type) const {
	switch (type) {
		case MaterialType::Standard: return Program::TypeConformanceList{ {{"StandardMaterial", "IMaterial"}, (uint32_t)MaterialType::Standard} };
		case MaterialType::Hair: return Program::TypeConformanceList{ {{"HairMaterial", "IMaterial"}, (uint32_t)MaterialType::Hair} };
		case MaterialType::Cloth: return Program::TypeConformanceList{ {{"ClothMaterial", "IMaterial"}, (uint32_t)MaterialType::Cloth} };
		case MaterialType::MERL: return Program::TypeConformanceList{ {{"MERLMaterial", "IMaterial"}, (uint32_t)MaterialType::MERL} };
		default: throw std::runtime_error("Unsupported material type");
	}
}

void MaterialSystem::createParameterBlock() {
	// Create parameter block.
	Program::DefineList defines = getDefines();
	defines.add("MATERIAL_SYSTEM_PARAMETER_BLOCK");
	auto pPass = ComputePass::create(mpDevice, kShaderFilename, "main", defines);
	auto pReflector = pPass->getProgram()->getReflector()->getParameterBlock("gMaterialsBlock");
	assert(pReflector);

	mpMaterialsBlock = ParameterBlock::create(mpDevice, pReflector);
	assert(mpMaterialsBlock);

	// Verify that the material data struct size on the GPU matches the host-side size.
	auto reflVar = mpMaterialsBlock->getReflection()->findMember(kMaterialDataName);
	assert(reflVar);
	auto reflResType = reflVar->getType()->asResourceType();
	assert(reflResType && reflResType->getType() == ReflectionResourceType::Type::StructuredBuffer);
	auto byteSize = reflResType->getStructType()->getByteSize();
	
	if (byteSize != sizeof(MaterialDataBlob)) {
		throw std::runtime_error("MaterialSystem material data buffer has unexpected struct size");
	}

	// Create materials data buffer.
	if (!mMaterials.empty() && (!mpMaterialDataBuffer || mpMaterialDataBuffer->getElementCount() < mMaterials.size())) {
		mpMaterialDataBuffer = Buffer::createStructured(mpDevice, mpMaterialsBlock[kMaterialDataName], (uint32_t)mMaterials.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
		mpMaterialDataBuffer->setName("MaterialSystem::mpMaterialDataBuffer");
	}

	// Bind resources to parameter block.
	mpMaterialsBlock[kMaterialDataName] = !mMaterials.empty() ? mpMaterialDataBuffer : nullptr;
	mpMaterialsBlock["materialCount"] = getMaterialCount();

	// Samplers
	mpMaterialsBlock["udimTileSampler"] = mpUDIMTileSampler;
}

void MaterialSystem::uploadMaterial(const uint32_t materialID) {
	assert(materialID < mMaterials.size());
	const auto& pMaterial = mMaterials[materialID];
	assert(pMaterial);

	// TODO: On initial upload of materials, we could improve this by not having separate calls to setElement()
	// but instead prepare a buffer containing all data.
	assert(mpMaterialDataBuffer);
	mpMaterialDataBuffer->setElement(materialID, pMaterial->getDataBlob());
}

}  // namespace Falcor

