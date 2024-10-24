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
#include "BasicMaterial.h"
#include "Falcor/Experimental/Scene/Materials/LobeType.slang"

#include "MaterialSystem.h"
#include "Material.h"

namespace Falcor {

namespace {

    static_assert(sizeof(TextureHandle) == 4);
    static_assert(sizeof(MaterialHeader) == 16);
    static_assert(sizeof(MaterialPayload) == 112);
    static_assert(sizeof(MaterialDataBlob) == 128);
    static_assert(static_cast<uint32_t>(MaterialType::BuiltinCount) <= (1u << MaterialHeader::kMaterialTypeBits), "MaterialType count exceeds the maximum");
    static_assert(static_cast<uint32_t>(AlphaMode::Count) <= (1u << MaterialHeader::kAlphaModeBits), "AlphaMode bit count exceeds the maximum");
    static_assert(static_cast<uint32_t>(LobeType::All) < (1u << MaterialHeader::kLobeTypeBits), "LobeType bit count exceeds the maximum");
    static_assert(static_cast<uint32_t>(TextureHandle::Mode::Count) <= (1u << TextureHandle::kModeBits), "TextureHandle::Mode bit count exceeds the maximum");
    static_assert(MaterialHeader::kTotalHeaderBitsX <= 32, "MaterialHeader bit count x exceeds the maximum");
    static_assert(MaterialHeader::kTotalHeaderBitsY <= 32, "MaterialHeader bit count y exceeds the maximum");
    static_assert(MaterialHeader::kTotalHeaderBitsZ <= 32, "MaterialHeader bit count z exceeds the maximum");
    static_assert(MaterialHeader::kTotalHeaderBitsW <= 32, "MaterialHeader bit count w exceeds the maximum");
    static_assert(MaterialHeader::kAlphaThresholdBits == 16, "MaterialHeader alpha threshold bit count must be 16");
}

bool operator==(const MaterialHeader& lhs, const MaterialHeader& rhs) {
    return lhs.packedData == rhs.packedData;
}

Material::Material(Device::SharedPtr pDevice, const std::string& name, MaterialType type): mpDevice(pDevice), mName(name) {
    mHeader.setMaterialType(type);
    mHeader.setAlphaMode(AlphaMode::Opaque);
    mHeader.setAlphaThreshold(float16_t(0.5f));
    mHeader.setActiveLobes(static_cast<uint32_t>(LobeType::All));
    mHeader.setIoR(1.5f);
}

std::shared_ptr<BasicMaterial> Material::toBasicMaterial() {
    if (mHeader.isBasicMaterial()) {
        assert(std::dynamic_pointer_cast<BasicMaterial>(shared_from_this()));
        return std::static_pointer_cast<BasicMaterial>(shared_from_this());
    }
    return nullptr;
}

void Material::setDoubleSided(bool doubleSided) {
    if (mHeader.isDoubleSided() != doubleSided) {
        mHeader.setDoubleSided(doubleSided);
        markUpdates(UpdateFlags::DataChanged);
    }
}

void Material::setThinSurface(bool thinSurface) {
    if (mHeader.isThinSurface() != thinSurface) {
        mHeader.setThinSurface(thinSurface);
        markUpdates(UpdateFlags::DataChanged);
    }
}

void Material::setAlphaMode(AlphaMode alphaMode) {
    LLOG_WRN << "Material '" << getName() << "' of type '" << to_string(getType()) << "' does not support alpha. Ignoring call to setAlphaMode().";
}

void Material::setAlphaThreshold(float alphaThreshold) {
    LLOG_WRN << "Material '" << getName() << "' of type '" << to_string(getType()) << "' does not support alpha. Ignoring call to setAlphaThreshold().";
}

void Material::setNestedPriority(uint32_t priority) {
    const uint32_t maxPriority = (1u << MaterialHeader::kNestedPriorityBits) - 1;
    if (priority > maxPriority) {
        LLOG_WRN << "Requested nested priority " << std::to_string(priority) << " for material '" << mName << "' is out of range. Clamping to "<< std::to_string(maxPriority) << ".";
        priority = maxPriority;
    }
    if (mHeader.getNestedPriority() != priority) {
        mHeader.setNestedPriority(priority);
        markUpdates(UpdateFlags::DataChanged);
    }
}

void Material::setIndexOfRefraction(float IoR) {
    if (mHeader.getIoR() != (float16_t)IoR) {
        mHeader.setIoR((float16_t)IoR);
        markUpdates(UpdateFlags::DataChanged);
    }
}

const Material::TextureSlotInfo& Material::getTextureSlotInfo(const TextureSlot slot) const {
    assert((size_t)slot < mTextureSlotInfo.size());
    return mTextureSlotInfo[(size_t)slot];
}

bool Material::hasTextureSlotData(const TextureSlot slot) const {
    assert((size_t)slot < mTextureSlotInfo.size());
    return mTextureSlotData[(size_t)slot].pTexture != nullptr;
}

size_t Material::getTextureCount() const {
    size_t result = 0;
    for(const auto& data: mTextureSlotData) {
        if(data.pTexture != nullptr) result++;
    }
    return result;
}

std::vector<Texture::SharedPtr> Material::getTextures() const {
    std::vector<Texture::SharedPtr> textures;
    for(const auto& data: mTextureSlotData) {
        if(data.pTexture != nullptr) textures.push_back(data.pTexture);
    }
    return textures;
}

void Material::getTextures(std::vector<Texture::SharedPtr>& textures, bool append) const {
    if(!append) textures.clear();
    
    for(const auto& data: mTextureSlotData) {
        if(data.pTexture != nullptr) {
            textures.push_back(data.pTexture);
        }
    }
}

bool Material::setTexture(const TextureSlot slot, const Texture::SharedPtr& pTexture) {
    if(!pTexture) {
        LLOG_WRN << "Null texture provided for material '" << getName() << "' at slot '" << to_string(slot) << "'. Ignoring call to setTexture().";
    }

    if (!hasTextureSlot(slot)) {
        LLOG_WRN << "Material '" << getName() << "' does not have texture slot '" << to_string(slot) << "'. Ignoring call to setTexture().";
        return false;
    }

    if (pTexture == getTexture(slot)) {
        LLOG_WRN << "Material '" << getName() << "' already have texture at slot '" << to_string(slot) << "'. Ignoring call to setTexture().";
        return false;
    }

    assert((size_t)slot < mTextureSlotInfo.size());
    mTextureSlotData[(size_t)slot].pTexture = pTexture;

    markUpdates(UpdateFlags::ResourcesChanged);
    return true;
}

Texture::SharedPtr Material::getTexture(const TextureSlot slot) const {
    if (!hasTextureSlot(slot)) return nullptr;

    assert((size_t)slot < mTextureSlotInfo.size());
    return mTextureSlotData[(size_t)slot].pTexture;
}

void Material::loadTexture(TextureSlot slot, const fs::path& path, bool useSrgb) {
    if (!hasTextureSlot(slot)) {
        LLOG_WRN << "Material '" << getName() << "' does not have texture slot '" << to_string(slot) << "'. Ignoring call to loadTexture().";
        return;
    }

    fs::path fullPath;
    if (findFileInDataDirectories(path, fullPath)) {
        auto texture = Texture::createFromFile(mpDevice, fullPath, true, useSrgb && getTextureSlotInfo(slot).srgb);
        if (texture) {
            setTexture(slot, texture);
            // Flush and sync in order to prevent the upload heap from growing too large. Doing so after
            // every texture creation is overly conservative, and will likely lead to performance issues
            // due to the forced CPU/GPU sync.
            mpDevice->flushAndSync();
        }
    }
}

uint2 Material::getMaxTextureDimensions() const {
    uint2 dim = uint2(0);
    for (uint32_t i = 0; i < (uint32_t)TextureSlot::Count; i++) {
        auto pTexture = getTexture((TextureSlot)i);
        if (pTexture) dim = max(dim, uint2(pTexture->getWidth(), pTexture->getHeight()));
    }
    return dim;
}

void Material::setTextureTransform(const Transform& textureTransform) {
    mTextureTransform = textureTransform;
}

void Material::markUpdates(UpdateFlags updates) {
    // Mark updates locally in this material.
    mUpdates |= updates;

    // Mark updates globally across all materials.
    if (mUpdateCallback) mUpdateCallback(updates);
}

void Material::updateTextureHandle(MaterialSystem* pOwner, const Texture::SharedPtr& pTexture, TextureHandle& handle) {
    TextureHandle prevHandle = handle;

    // Update the given texture handle.
    if (pTexture) {
        auto h = pOwner->textureManager()->addTexture(pTexture);
        assert(h);
        handle.setTextureID(h.getID());
        if (pTexture->isUDIMTexture()) {
            handle.setMode(TextureHandle::Mode::UDIM_Texture);
        } else if (pTexture->isSparse()) {
            handle.setMode(TextureHandle::Mode::Virtual);
        }
    } else {
        handle.setMode(TextureHandle::Mode::Uniform);
    }

    if (handle != prevHandle) mUpdates |= Material::UpdateFlags::DataChanged;
}

void Material::updateTextureHandle(MaterialSystem* pOwner, const TextureSlot slot, TextureHandle& handle) {
    auto pTexture = getTexture(slot);
    updateTextureHandle(pOwner, pTexture, handle);
};

void Material::updateDefaultTextureSamplerID(MaterialSystem* pOwner, const Sampler::SharedPtr& pSampler) {
    const uint32_t samplerID = pOwner->addTextureSampler(pSampler);

    if (mHeader.getDefaultTextureSamplerID() != samplerID) {
        mHeader.setDefaultTextureSamplerID(samplerID);
        mUpdates |= Material::UpdateFlags::DataChanged;
    }
}

bool Material::isBaseEqual(const Material& other) const {
    // This function compares all data in the base class between two materials *except* the name.
    // It's a separate helper to ensure isEqual() is pure virtual and must be implemented in all derived classes.

    if (mHeader != other.mHeader) return false;
    if (mTextureTransform != other.mTextureTransform) return false;

    assert(mTextureSlotInfo.size() == mTextureSlotData.size());
    for (size_t i = 0; i < mTextureSlotInfo.size(); i++) {
        // Compare texture slots.
        // These checks are a bit redundant since identical material types are currently
        // guaranteed to have the same set of slots, but this is future-proof if that changes.
        auto slot = (TextureSlot)i;
        if (hasTextureSlot(slot) != other.hasTextureSlot(slot)) return false;
        if (hasTextureSlot(slot)) {
            if (mTextureSlotInfo[i] != other.mTextureSlotInfo[i]) return false;
            if (mTextureSlotData[i] != other.mTextureSlotData[i]) return false;
        }
    }

    return true;
}

#ifdef SCRIPTING
SCRIPT_BINDING(Material) {
    SCRIPT_BINDING_DEPENDENCY(Transform)

    pybind11::enum_<MaterialType> materialType(m, "MaterialType");
    materialType.value("Standard", MaterialType::Standard);
    materialType.value("Cloth", MaterialType::Cloth);
    materialType.value("Hair", MaterialType::Hair);
    materialType.value("MERL", MaterialType::MERL);

    pybind11::enum_<AlphaMode> alphaMode(m, "AlphaMode");
    alphaMode.value("Opaque", AlphaMode::Opaque);
    alphaMode.value("Mask", AlphaMode::Mask);

    pybind11::enum_<Material::TextureSlot> textureSlot(m, "MaterialTextureSlot");
    textureSlot.value("BaseColor", Material::TextureSlot::BaseColor);
    textureSlot.value("Specular", Material::TextureSlot::Specular);
    textureSlot.value("Emissive", Material::TextureSlot::Emissive);
    textureSlot.value("Normal", Material::TextureSlot::Normal);
    textureSlot.value("Roughness", Material::TextureSlot::Roughness);
    textureSlot.value("Transmission", Material::TextureSlot::Transmission);
    textureSlot.value("Displacement", Material::TextureSlot::Displacement);

    // Register Material base class as IMaterial in python to allow deprecated script syntax.
    // TODO: Remove workaround when all scripts have been updated to create derived Material classes.
    pybind11::class_<Material, Material::SharedPtr> material(m, "IMaterial"); // PYTHONDEPRECATED
    material.def_property_readonly("type", &Material::getType);
    material.def_property("name", &Material::getName, &Material::setName);
    material.def_property("doubleSided", &Material::isDoubleSided, &Material::setDoubleSided);
    material.def_property("thinSurface", &Material::isThinSurface, &Material::setThinSurface);
    material.def_property_readonly("emissive", &Material::isEmissive);
    material.def_property("alphaMode", &Material::getAlphaMode, &Material::setAlphaMode);
    material.def_property("alphaThreshold", &Material::getAlphaThreshold, &Material::setAlphaThreshold);
    material.def_property("nestedPriority", &Material::getNestedPriority, &Material::setNestedPriority);
    material.def_property("textureTransform", pybind11::overload_cast<void>(&Material::getTextureTransform, pybind11::const_), &Material::setTextureTransform);

    material.def("loadTexture", &Material::loadTexture, "slot"_a, "path"_a, "useSrgb"_a = true);
    material.def("clearTexture", &Material::clearTexture, "slot"_a);
}
#endif

}  // namespace Falcor
