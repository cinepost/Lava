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
#include "MaterialTextureLoader.h"

namespace Falcor {

MaterialTextureLoader::MaterialTextureLoader(const Device::SharedPtr& pDevice, bool useSrgb)
	: mpDevice(pDevice)
	, mUseSrgb(useSrgb)
{
}

MaterialTextureLoader::~MaterialTextureLoader() {
	assignTextures();
}

void MaterialTextureLoader::loadTexture(const Material::SharedPtr& pMaterial, Material::TextureSlot slot, const fs::path& path, bool loadAsSparse) {
	assert(pMaterial);
	if (!pMaterial->hasTextureSlot(slot)) {
		LLOG_WRN << "MaterialTextureLoader::loadTexture() - Material '" << pMaterial->getName() << "' does not have texture slot '" << to_string(slot) << "'. Ignoring call.";
		return;
	}

	bool generateMipLevels = true;
	bool loadAsSRGB = mUseSrgb && pMaterial->getTextureSlotInfo(slot).srgb;
	Resource::BindFlags bindFlags = Resource::BindFlags::ShaderResource;
	std::string udim_mask = "<UDIM>";
	bool async = true;

	// Request texture to be loaded.
	TextureManager::TextureHandle handle = mpDevice->textureManager()->loadTexture(path, generateMipLevels, loadAsSRGB, bindFlags, async, udim_mask, loadAsSparse);

	// Store assignment to material for later.
	mTextureAssignments.emplace_back(TextureAssignment{ pMaterial, slot, handle });

	LLOG_DBG << (loadAsSparse ? "Sparse" : "Simple") << " texture " << path.string() << " with handle mode " << to_string(handle.mode()) << " in assignment";
}

void MaterialTextureLoader::assignTextures() {
	mpDevice->textureManager()->waitForAllTexturesLoading();

	// Assign textures to materials.
	for (const auto& assignment : mTextureAssignments) {
	  // Assign generic handle
		auto pTexture = mpDevice->textureManager()->getTexture(assignment.handle);
		assignment.pMaterial->setTexture(assignment.textureSlot, pTexture);
	}
}

}  // namespace Falcor
