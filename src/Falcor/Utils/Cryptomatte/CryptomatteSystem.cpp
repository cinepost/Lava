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
#include "Falcor/Scene/Material/MaterialSystem.h"

#include "MurmurHash.h"
#include "CryptomatteSystem.h"

namespace Falcor {

namespace {
	static const uint32_t gSeed = 0;
	const std::string kShaderFilename = "Utils/Cryptomatte/CryptomatteSystem.slang";
	const std::string kMaterialHashDataName = "materialHashData";
	const std::string kInstanceHashDataName = "instanceHashData";
}

CryptomatteSystem::SharedPtr CryptomatteSystem::create(Device::SharedPtr pDevice) {
	return SharedPtr(new CryptomatteSystem(pDevice));
}

CryptomatteSystem::CryptomatteSystem(Device::SharedPtr pDevice): mpDevice(pDevice) {
	// Reserve and resize for the bare minimum of hashes...
	mMaterialHashes.reserve(512);
	mMaterialHashes.resize(64, 0);

	mInstanceHashes.reserve(1024);
	mInstanceHashes.resize(128, 0);

	mCustattrHashes.reserve(1024);
	mCustattrHashes.resize(128, 0);
}

void CryptomatteSystem::finalize() {

}

void CryptomatteSystem::addMaterial(const std::string& name, uint32_t materialID) {
	if(mMaterialHashes.size() <= materialID) mMaterialHashes.resize(materialID);
	mMaterialHashes[materialID] = util_murmur_hash3(static_cast<const void *>(name.data()), name.size(), gSeed);
}

void CryptomatteSystem::addInstance(const std::string& name, uint32_t instanceID) {
	if(mInstanceHashes.size() <= instanceID) mInstanceHashes.resize(instanceID);
	mInstanceHashes[instanceID] = util_murmur_hash3(static_cast<const void *>(name.data()), name.size(), gSeed);
}

void CryptomatteSystem::addCustattr(const std::string& name, uint32_t instanceID) {
	if(mCustattrHashes.size() <= instanceID) mCustattrHashes.resize(instanceID);
	mCustattrHashes[instanceID] = util_murmur_hash3(static_cast<const void *>(name.data()), name.size(), gSeed);
}

void CryptomatteSystem::addMaterials(const MaterialSystem* pMaterials) {
	assert(pMaterials);
	mMaterialHashes.resize(pMaterials->getMaterialCount());
	const auto& materials = pMaterials->getMaterials();

	for(uint32_t materialID = 0; materialID < materials.size(); materialID++) {
		const auto& pMaterial = materials[materialID];
		const std::string& name = pMaterial->getName();
		addMaterial(name, materialID);
	}
}

Shader::DefineList CryptomatteSystem::getDefaultDefines() {
	Shader::DefineList defines;
	defines.add("CRYPTOMATTE_SYSTEM_MATERIALS_COUNT", "0");
	defines.add("CRYPTOMATTE_SYSTEM_INSTANCES_COUNT", "0");

	return defines;
}

Shader::DefineList CryptomatteSystem::getDefines() const {
	Shader::DefineList defines;
	defines.add("CRYPTOMATTE_SYSTEM_MATERIALS_COUNT", std::to_string(mMaterialHashes.size()));
	defines.add("CRYPTOMATTE_SYSTEM_INSTANCES_COUNT", std::to_string(mInstanceHashes.size()));
	return defines;
}

void CryptomatteSystem::createParameterBlock() {
/*
	// Create parameter block.
	Program::DefineList defines = getDefines();
	defines.add("CRYPTOMATTE_SYSTEM_PARAMETER_BLOCK");
	auto pPass = ComputePass::create(mpDevice, kShaderFilename, "main", defines);
	auto pReflector = pPass->getProgram()->getReflector()->getParameterBlock("gCryptomatteBlock");
	assert(pReflector);

	mpCryptomatteBlock = ParameterBlock::create(mpDevice, pReflector);
	assert(mpCryptomatteBlock);

	// Verify that the material data struct size on the GPU matches the host-side size.
	auto reflVar = mpCryptomatteBlock->getReflection()->findMember(kMaterialDataName);
	assert(reflVar);
	auto reflResType = reflVar->getType()->asResourceType();
	assert(reflResType && reflResType->getType() == ReflectionResourceType::Type::StructuredBuffer);
	auto byteSize = reflResType->getStructType()->getByteSize();
	
	if (byteSize != sizeof(MaterialDataBlob)) {
		throw std::runtime_error("CryptomatteSystem material data buffer has unexpected struct size");
	}

	// Create materials data buffer.
	//if (!mMaterials.empty() && (!mpMaterialDataBuffer || mpMaterialDataBuffer->getElementCount() < mMaterials.size())) {
	//	mpMaterialDataBuffer = Buffer::createStructured(mpDevice, mpCryptomatteBlock[kMaterialDataName], (uint32_t)mMaterials.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
	//	mpMaterialDataBuffer->setName("CryptomatteSystem::mpMaterialDataBuffer");
	//}

	// Bind resources to parameter block.
	mpCryptomatteBlock["materialCount"] = mMaterialHashes.size();
*/
}

}  // namespace Falcor

