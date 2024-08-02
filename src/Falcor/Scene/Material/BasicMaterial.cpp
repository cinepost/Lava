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

#include "Falcor/Core/API/RenderContext.h"

#include "Core/Program/GraphicsProgram.h"
#include "Core/Program/ProgramVars.h"
#include "Utils/Color/ColorHelpers.slang"

#include "MaterialSystem.h"
#include "BasicMaterial.h"


namespace Falcor {

namespace {
    static_assert((sizeof(MaterialHeader) + sizeof(BasicMaterialData)) <= sizeof(MaterialDataBlob), "Total material data size is too large");
    static_assert(static_cast<uint32_t>(ShadingModel::Count) <= (1u << BasicMaterialData::kShadingModelBits), "ShadingModel bit count exceeds the maximum");
    static_assert(static_cast<uint32_t>(NormalMapType::Count) <= (1u << BasicMaterialData::kNormalMapTypeBits), "NormalMapType bit count exceeds the maximum");
    static_assert(BasicMaterialData::kTotalFlagsBits <= 32, "BasicMaterialData flags bit count exceeds the maximum");

    // Constants.
    const float kMaxVolumeAnisotropy = 0.99f;
    const float kOpacityThreshold = 0.999f;
}

BasicMaterial::BasicMaterial(Device::SharedPtr pDevice, const std::string& name, MaterialType type): Material(pDevice, name, type) {
    mHeader.setIsBasicMaterial(true);

    // Setup common texture slots.
    mTextureSlotInfo[(uint32_t)TextureSlot::Normal] = { "normal", TextureChannelFlags::RGB, false };
    mTextureSlotInfo[(uint32_t)TextureSlot::Displacement] = { "displacement", TextureChannelFlags::RGB, false };

    // Call update functions to ensure a valid initial state based on default material parameters.
    updateAlphaMode();
    updateNormalMapType();
    updateEmissiveFlag();
}

Material::UpdateFlags BasicMaterial::update(MaterialSystem* pOwner) {
    assert(pOwner);

    auto flags = Material::UpdateFlags::None;
    if (mUpdates != Material::UpdateFlags::None) {
        // Adjust material sidedness based on current parameters.
        // TODO: Remove when single-sided transmissive materials are supported.
        adjustDoubleSidedFlag();

        // Prepare displacement maps for rendering.
        prepareDisplacementMapForRendering();

        // Update texture handles.
        updateTextureHandle(pOwner, TextureSlot::BaseColor, mData.texBaseColor);
        updateTextureHandle(pOwner, TextureSlot::Metallic, mData.texMetallic);
        updateTextureHandle(pOwner, TextureSlot::Emissive, mData.texEmissive);
        updateTextureHandle(pOwner, TextureSlot::Roughness, mData.texRoughness);
        updateTextureHandle(pOwner, TextureSlot::Transmission, mData.texTransmission);
        updateTextureHandle(pOwner, TextureSlot::Normal, mData.texNormalMap);
        updateTextureHandle(pOwner, TextureSlot::Displacement, mData.texDisplacementMap);
        updateTextureHandle(pOwner, TextureSlot::Opacity, mData.texOpacity);

        // Update default sampler.
        updateDefaultTextureSamplerID(pOwner, mpDefaultSampler);

        // Update displacement samplers.
        uint prevFlags = mData.flags;
        mData.setDisplacementMinSamplerID(pOwner->addTextureSampler(mpDisplacementMinSampler));
        mData.setDisplacementMaxSamplerID(pOwner->addTextureSampler(mpDisplacementMaxSampler));
        if (mData.flags != prevFlags) mUpdates |= Material::UpdateFlags::DataChanged;

        flags |= mUpdates;
        mUpdates = Material::UpdateFlags::None;
    }

    return flags;
}

void BasicMaterial::update(const Material::SharedPtr& pMaterial) {
    assert(pMaterial);

    auto const& pBasicMaterial = std::dynamic_pointer_cast<BasicMaterial>(pMaterial);
    setAlphaMode(pBasicMaterial->getAlphaMode());
    setAlphaThreshold(pBasicMaterial->getAlphaThreshold());
    setIndexOfRefraction(pBasicMaterial->getIndexOfRefraction());
    setBaseColor(pBasicMaterial->getBaseColor());
    setReflectivity(pBasicMaterial->getReflectivity());
    setTransmissionColor(pBasicMaterial->getTransmissionColor());
    setDiffuseTransmission(pBasicMaterial->getDiffuseTransmission());
    setSpecularTransmission(pBasicMaterial->getSpecularTransmission());
    setNormalBumpMapFactor(pBasicMaterial->getNormalBumpMapFactor());
    setNormalMapMode(pBasicMaterial->getNormalMapMode());
    setNormalMapFlipX(pBasicMaterial->getNormalMapFlipX());
    setNormalMapFlipY(pBasicMaterial->getNormalMapFlipY());
    setOpacityScale(pBasicMaterial->getOpacityScale());
    useBaseColorAlpha(pBasicMaterial->isBaseColorAlphaUsed());
}

const TextureHandle& BasicMaterial::getTextureHandle(const TextureSlot slot) const {
    switch(slot) {
        case TextureSlot::BaseColor:
            return mData.texBaseColor;
        case TextureSlot::Metallic:
            return mData.texMetallic;
        case TextureSlot::Emissive:
            return mData.texEmissive;
        case TextureSlot::Roughness:
            return mData.texRoughness;
        case TextureSlot::Transmission:
            return mData.texTransmission;
        case TextureSlot::Normal:
            return mData.texNormalMap;
        case TextureSlot::Displacement:
            return mData.texDisplacementMap;
        case TextureSlot::Opacity:
            return mData.texOpacity;
        default:
            LLOG_ERR << "Error getting handle for slot " << to_string(slot);
            should_not_get_here();
            return {};

    }
}

bool BasicMaterial::isDisplaced() const {
    return hasTextureSlotData(Material::TextureSlot::Displacement);
}

bool BasicMaterial::hasUDIMTextures() const {
    if(mData.texBaseColor.isUDIMTexture()) return true;
    if(mData.texMetallic.isUDIMTexture()) return true;
    if(mData.texEmissive.isUDIMTexture()) return true;
    if(mData.texRoughness.isUDIMTexture()) return true;
    if(mData.texTransmission.isUDIMTexture()) return true;
    if(mData.texNormalMap.isUDIMTexture()) return true;
    if(mData.texDisplacementMap.isUDIMTexture()) return true;
    if(mData.texOpacity.isUDIMTexture()) return true;

    return false;
}

void BasicMaterial::setAlphaMode(AlphaMode alphaMode) {
    if (!isAlphaSupported()) {
        assert(getAlphaMode() == AlphaMode::Opaque);
        LLOG_DBG << "Alpha is not supported by material type '" << to_string(getType()) << "'. Ignoring call to setAlphaMode() for material '" << getName() << "'.";
        return;
    }
    
    if (mHeader.getAlphaMode() != alphaMode) {
        mHeader.setAlphaMode(alphaMode);
        markUpdates(UpdateFlags::DataChanged);
    }
}

void BasicMaterial::setAlphaThreshold(float alphaThreshold) {
    if (!isAlphaSupported()) {
        LLOG_DBG << "Alpha is not supported by material type '" << to_string(getType()) << "'. Ignoring call to setAlphaMode() for material '" << getName() << "'.";
        return;
    }
    if (mHeader.getAlphaThreshold() != (float16_t)alphaThreshold) {
        mHeader.setAlphaThreshold((float16_t)alphaThreshold);
        markUpdates(UpdateFlags::DataChanged);
        updateAlphaMode();
    }
}

void BasicMaterial::setIndexOfRefraction(float IoR) {
    if (mData.IoR == (float16_t)IoR) return;
    mData.IoR = (float16_t)IoR;
    markUpdates(UpdateFlags::DataChanged);
}

void BasicMaterial::setDefaultTextureSampler(const Sampler::SharedPtr& pSampler) {
    if (pSampler != mpDefaultSampler) {
        mpDefaultSampler = pSampler;

        // Create derived samplers for displacement Min/Max filtering.
        Sampler::Desc desc = pSampler->getDesc();
        desc.setMaxAnisotropy(16); // Set 16x anisotropic filtering for improved min/max precision per triangle.
        desc.setReductionMode(Sampler::ReductionMode::Min);
        mpDisplacementMinSampler = Sampler::create(mpDevice, desc);
        desc.setReductionMode(Sampler::ReductionMode::Max);
        mpDisplacementMaxSampler = Sampler::create(mpDevice, desc);

        markUpdates(UpdateFlags::ResourcesChanged);
    }
}

bool BasicMaterial::setTexture(const TextureSlot slot, const Texture::SharedPtr& pTexture) {
    if (!Material::setTexture(slot, pTexture)) return false;

    // Update additional metadata about texture usage.
    switch (slot) {
        case TextureSlot::BaseColor:
            if (pTexture) {
                // Assume the texture is non-constant and has full alpha range.
                // This may be changed later by optimizeTexture().
                mAlphaRange = float2(0.f, 1.f);
                mIsTexturedBaseColorConstant = mIsTexturedAlphaConstant = false;
            }
            updateAlphaMode();
            break;
        case TextureSlot::Opacity:
            updateAlphaMode();
            break;
        case TextureSlot::Normal:
            updateNormalMapType();
            break;
        case TextureSlot::Emissive:
            updateEmissiveFlag();
            break;
        case TextureSlot::Displacement:
            mDisplacementMapChanged = true;
            markUpdates(UpdateFlags::DisplacementChanged);
            break;
        default:
            break;
    }

    return true;
}

void BasicMaterial::optimizeTexture(const TextureSlot slot, const TextureAnalyzer::Result& texInfo, TextureOptimizationStats& stats) {
    assert(getTexture(slot) != nullptr);
    TextureChannelFlags channelMask = getTextureSlotInfo(slot).mask;

    switch (slot) {
    case TextureSlot::BaseColor: {
        bool previouslyOpaque = isOpaque();

        auto pBaseColor = getBaseColorTexture();
        bool hasAlpha = isAlphaSupported() && pBaseColor && doesFormatHasAlpha(pBaseColor->getFormat());
        bool isColorConstant = texInfo.isConstant(TextureChannelFlags::RGB);
        bool isAlphaConstant = texInfo.isConstant(TextureChannelFlags::Alpha);

        // Update the alpha range.
        if (hasAlpha) mAlphaRange = float2(texInfo.minValue.a, texInfo.maxValue.a);

        // Update base color parameter and texture.
        float3 baseColor = getBaseColor();
        if (isColorConstant) {
            baseColor = float3(texInfo.value.rgb);
            mIsTexturedBaseColorConstant = true;
        }
        if (hasAlpha && isAlphaConstant) {
            mIsTexturedAlphaConstant = true;
        }
        setBaseColor(baseColor);

        if (isColorConstant && (!hasAlpha || isAlphaConstant)) {
            clearTexture(Material::TextureSlot::BaseColor);
            stats.texturesRemoved[(size_t)slot]++;
        }
        else if (isColorConstant)
        {
            // We don't have a way to specify constant base color with non-constant alpha since they share a texture slot.
            // Count number of cases and issue a perf warning later instead.
            stats.constantBaseColor++;
        }

        updateAlphaMode();

        if (!previouslyOpaque && isOpaque()) stats.disabledAlpha++;

        break;
    }
    case TextureSlot::Metallic: {
        if (texInfo.isConstant(channelMask)) {
            clearTexture(Material::TextureSlot::Metallic);
            setMetallic(texInfo.value.r);
            stats.texturesRemoved[(size_t)slot]++;
        }
        break;
    }
    case TextureSlot::Emissive: {
        if (texInfo.isConstant(channelMask)) {
            clearTexture(Material::TextureSlot::Emissive);
            setEmissiveColor(texInfo.value.rgb);
            stats.texturesRemoved[(size_t)slot]++;
        }
        break;
    }
    case TextureSlot::Normal: {
        // Determine which channels of the normal map are used.
        switch (getNormalMapType()) {
            case NormalMapType::RG:
                channelMask = TextureChannelFlags::Red | TextureChannelFlags::Green;
                break;
            case NormalMapType::RGB:
                channelMask = TextureChannelFlags::RGB;
                break;
            default:
                LLOG_WRN << "BasicMaterial::optimizeTexture() - Unsupported normal map mode";
                channelMask = TextureChannelFlags::RGBA;
                break;
        }

        if (texInfo.isConstant(channelMask)) {
            // We don't have a way to specify constant normal map value.
            // Count number of cases and issue a perf warning later instead.
            stats.constantNormalMaps++;
        }
        break;
    }
    case TextureSlot::Transmission: {
        if (texInfo.isConstant(channelMask)) {
            clearTexture(Material::TextureSlot::Transmission);
            setTransmissionColor(texInfo.value.rgb);
            stats.texturesRemoved[(size_t)slot]++;
        }
        break;
    }
    case TextureSlot::Displacement: {
        // Nothing to do here, displacement texture is prepared when calling prepareDisplacementMap().
        break;
    }
    default:
        throw std::runtime_error("'slot' refers to unexpected texture slot " + std::to_string((uint32_t)slot));
    }
}

bool BasicMaterial::isAlphaSupported() const {
    bool enabledAlphaChannel = getTextureSlotInfo(TextureSlot::BaseColor).hasChannel(TextureChannelFlags::Alpha);
    bool hasAlphaMap = enabledAlphaChannel && getTexture(TextureSlot::BaseColor) && doesFormatHasAlpha(getBaseColorTexture()->getFormat()) && mData.isBaseColorAlphaUsed();
    bool hasOpacityMap = hasTextureSlot(TextureSlot::Opacity) && getTexture(TextureSlot::Opacity);
    return hasAlphaMap || hasOpacityMap || (static_cast<float>(mData.opacityScale) <= kOpacityThreshold);
}

void BasicMaterial::prepareDisplacementMapForRendering() {
    if (auto pDisplacementMap = getDisplacementMap(); pDisplacementMap && mDisplacementMapChanged) {
        // Creates RGBA texture with MIP pyramid containing average, min, max values.
        Falcor::ResourceFormat oldFormat = pDisplacementMap->getFormat();

        // Replace texture with a 4 component one if necessary.
        if (getFormatChannelCount(oldFormat) < 4) {
            Falcor::ResourceFormat newFormat = ResourceFormat::RGBA16Float;
            Resource::BindFlags bf = pDisplacementMap->getBindFlags() | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::RenderTarget;
            Texture::SharedPtr newDisplacementTex = Texture::create2D(mpDevice, pDisplacementMap->getWidth(), pDisplacementMap->getHeight(), newFormat, pDisplacementMap->getArraySize(), Resource::kMaxPossible, nullptr, bf);

            // Copy base level.
            RenderContext* pContext = mpDevice->getRenderContext();
            uint32_t arraySize = pDisplacementMap->getArraySize();
            for (uint32_t a = 0; a < arraySize; a++) {
                auto srv = pDisplacementMap->getSRV(0, 1, a, 1);
                auto rtv = newDisplacementTex->getRTV(0, a, 1);
                const Sampler::ReductionMode redModes[] = { Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard };
                const float4 componentsTransform[] = { float4(1.0f, 0.0f, 0.0f, 0.0f), float4(1.0f, 0.0f, 0.0f, 0.0f), float4(1.0f, 0.0f, 0.0f, 0.0f), float4(1.0f, 0.0f, 0.0f, 0.0f) };
                pContext->blit(srv, rtv, RenderContext::kMaxRect, RenderContext::kMaxRect, Sampler::Filter::Linear, redModes, componentsTransform);
            }

            pDisplacementMap = newDisplacementTex;
            setDisplacementMap(newDisplacementTex);
        }

        // Build min/max MIPS.
        pDisplacementMap->generateMips(mpDevice->getRenderContext(), true);
    }
    mDisplacementMapChanged = false;
}

void BasicMaterial::setDisplacementScale(float scale) {
    float16_t _scale = static_cast<float16_t>(scale);
    if (mData.displacementScale == _scale) return;
    mData.displacementScale = _scale;
    markUpdates(UpdateFlags::DataChanged | UpdateFlags::DisplacementChanged);
}

void BasicMaterial::setDisplacementOffset(float offset) {
    float16_t _offset = static_cast<float16_t>(offset);
    if (mData.displacementOffset != _offset) {
        mData.displacementOffset = _offset;
        markUpdates(UpdateFlags::DataChanged | UpdateFlags::DisplacementChanged);
    }
}

void BasicMaterial::setAODistance(float distance) {
    //if (mData.ao_distance != (float16_t)distance) {
    //    mData.ao_distance = (float16_t)distance;
    //    markUpdates(UpdateFlags::DataChanged);
    //}
}

void BasicMaterial::setBaseColor(const float3& color) {
    if (mData.baseColor != (float16_t3)color) {
        mData.baseColor = (float16_t3)color;
        markUpdates(UpdateFlags::DataChanged);
        updateAlphaMode();
    }
}

void BasicMaterial::setOpacityScale(float opacityScale) {
    const float16_t _opacityScale = static_cast<float16_t>(opacityScale);
    if (mData.opacityScale == _opacityScale) return;
    mData.opacityScale = _opacityScale;
    markUpdates(UpdateFlags::DataChanged);
    updateAlphaMode();
}

bool BasicMaterial::isOpaque() const {
    return Material::isOpaque() && !isAlphaSupported();
}

void BasicMaterial::setReflectivity(const float& reflectivity) {
    const float16_t r = static_cast<float16_t>(reflectivity);
    if (mData.reflectivity == r) return;
    mData.reflectivity = r;
    markUpdates(UpdateFlags::DataChanged);
}

void BasicMaterial::setTransmissionColor(const float3& transmissionColor) {
    const float16_t3 t = static_cast<float16_t3>(transmissionColor);
    if (mData.transmission == t) return;
    mData.transmission = t;
    markUpdates(UpdateFlags::DataChanged);
}

void BasicMaterial::setNormalBumpMapFactor(float factor) { 
    const float16_t f = static_cast<float16_t>(factor);
    if(mData.bumpNormalFactor == f) return;
    mData.bumpNormalFactor = f;
    markUpdates(UpdateFlags::DataChanged); 
};

void BasicMaterial::setNormalMapMode(NormalMapMode mode) { 
    if(mData.getNormalMapMode() == mode) return;
    mData.setNormalMapMode(mode);
    markUpdates(UpdateFlags::DataChanged); 
};

void BasicMaterial::setNormalMapFlipX(bool flip) { 
    if(getNormalMapFlipX() == flip) return;
    mData.setNormalMapXFlip(flip);
    markUpdates(UpdateFlags::DataChanged); 
};

void BasicMaterial::setNormalMapFlipY(bool flip) {
    if(getNormalMapFlipY() == flip) return; 
    mData.setNormalMapYFlip(flip);
    markUpdates(UpdateFlags::DataChanged); 
};

void BasicMaterial::setDiffuseTransmission(float diffuseTransmission) {
    if (mData.diffuseTransmission != (float16_t)diffuseTransmission) {
        mData.diffuseTransmission = (float16_t)diffuseTransmission;
        markUpdates(UpdateFlags::DataChanged);
    }
}

void BasicMaterial::setSpecularTransmission(float specularTransmission) {
    if (mData.specularTransmission != (float16_t)specularTransmission) {
        mData.specularTransmission = (float16_t)specularTransmission;
        markUpdates(UpdateFlags::DataChanged);
    }
}

void BasicMaterial::setVolumeAbsorption(const float3& volumeAbsorption) {
    if (mData.volumeAbsorption != (float16_t3)volumeAbsorption) {
        mData.volumeAbsorption = (float16_t3)volumeAbsorption;
        markUpdates(UpdateFlags::DataChanged);
    }
}

void BasicMaterial::setVolumeScattering(const float3& volumeScattering) {
    if (mData.volumeScattering != (float16_t3)volumeScattering) {
        mData.volumeScattering = (float16_t3)volumeScattering;
        markUpdates(UpdateFlags::DataChanged);
    }
}

void BasicMaterial::setVolumeAnisotropy(float volumeAnisotropy) {
    auto clampedAnisotropy = clamp(volumeAnisotropy, -kMaxVolumeAnisotropy, kMaxVolumeAnisotropy);
    if (mData.volumeAnisotropy != (float16_t)clampedAnisotropy) {
        mData.volumeAnisotropy = (float16_t)clampedAnisotropy;
        markUpdates(UpdateFlags::DataChanged);
    }
}

bool BasicMaterial::isEqual(const Material::SharedPtr& pOther) const {
    auto other = std::dynamic_pointer_cast<BasicMaterial>(pOther);
    if (!other) return false;

    return (*this) == (*other);
}

bool BasicMaterial::operator==(const BasicMaterial& other) const {
    if (!isBaseEqual(other)) return false;

#define compare_field(_a) if (mData._a != other.mData._a) return false
    compare_field(flags);
    compare_field(displacementScale);
    compare_field(displacementOffset);
    compare_field(baseColor);
    compare_field(metallic);
    compare_field(roughness);
    compare_field(reflectivity);
    compare_field(emissive);
    compare_field(emissiveFactor);
    compare_field(bumpNormalFactor);
    compare_field(IoR);
    compare_field(diffuseTransmission);
    compare_field(specularTransmission);
    compare_field(transmission);
    compare_field(volumeAbsorption);
    compare_field(volumeAnisotropy);
    compare_field(volumeScattering);
    compare_field(volumeScattering);
    compare_field(opacityScale);
#undef compare_field

    // Compare the sampler descs directly to identify functional differences.
    if (mpDefaultSampler->getDesc() != other.mpDefaultSampler->getDesc()) return false;
    if (mpDisplacementMinSampler->getDesc() != other.mpDisplacementMinSampler->getDesc()) return false;
    if (mpDisplacementMaxSampler->getDesc() != other.mpDisplacementMaxSampler->getDesc()) return false;

    return true;
}

void BasicMaterial::updateAlphaMode() {
    setAlphaMode(isAlphaSupported() ? AlphaMode::Mask : AlphaMode::Opaque);
}

void BasicMaterial::updateNormalMapType() {
    NormalMapType type = NormalMapType::None;

    if (mData.getNormalMapMode() == NormalMapMode::Bump) {
        type = NormalMapType::RGB;
    } else if (auto pNormalMap = getNormalMap()) {
        switch (getFormatChannelCount(pNormalMap->getFormat())) {
            case 2:
                type = NormalMapType::RG;
                break;
            case 3:
            case 4: // Some texture formats don't support RGB, only RGBA. We have no use for the alpha channel in the normal map.
                type = NormalMapType::RGB;
                break;
            default:
                assert(false);
                LLOG_WRN << "Unsupported normal map format for material " << mName;
        }
    }

    if (mData.getNormalMapType() != type) {
        mData.setNormalMapType(type);
        markUpdates(UpdateFlags::DataChanged);
    }
}

void BasicMaterial::updateEmissiveFlag() {
    bool isEmissive = false;
    if (mData.emissiveFactor > 0.f) {
        isEmissive = hasTextureSlotData(Material::TextureSlot::Emissive) || (float3)mData.emissive != float3(0.f);
    }
    if (mHeader.isEmissive() != isEmissive) {
        mHeader.setEmissive(isEmissive);
        markUpdates(UpdateFlags::DataChanged);
    }
}

void BasicMaterial::adjustDoubleSidedFlag() {
    bool doubleSided = isDoubleSided();

    // Make double sided if diffuse or specular transmission is used.
    // Note this convention will eventually change to allow single-sided transmissive materials.
    if ((float)mData.diffuseTransmission > 0.f || (float)mData.specularTransmission > 0.f) doubleSided = true;

    // Make double sided if displaced since backfacing surfaces can become frontfacing.
    if (isDisplaced()) doubleSided = true;

    setDoubleSided(doubleSided);
}

#ifdef SCRIPTING
SCRIPT_BINDING(BasicMaterial) {
    SCRIPT_BINDING_DEPENDENCY(Material)

    pybind11::class_<BasicMaterial, Material, BasicMaterial::SharedPtr> material(m, "BasicMaterial");
    material.def_property("baseColor", &BasicMaterial::getBaseColor, &BasicMaterial::setBaseColor);
    material.def_property("specularParams", &BasicMaterial::getSpecularParams, &BasicMaterial::setSpecularParams);
    material.def_property("transmissionColor", &BasicMaterial::getTransmissionColor, &BasicMaterial::setTransmissionColor);
    material.def_property("diffuseTransmission", &BasicMaterial::getDiffuseTransmission, &BasicMaterial::setDiffuseTransmission);
    material.def_property("specularTransmission", &BasicMaterial::getSpecularTransmission, &BasicMaterial::setSpecularTransmission);
    material.def_property("volumeAbsorption", &BasicMaterial::getVolumeAbsorption, &BasicMaterial::setVolumeAbsorption);
    material.def_property("volumeScattering", &BasicMaterial::getVolumeScattering, &BasicMaterial::setVolumeScattering);
    material.def_property("volumeAnisotropy", &BasicMaterial::getVolumeAnisotropy, &BasicMaterial::setVolumeAnisotropy);
    material.def_property("indexOfRefraction", &BasicMaterial::getIndexOfRefraction, &BasicMaterial::setIndexOfRefraction);
    material.def_property("displacementScale", &BasicMaterial::getDisplacementScale, &BasicMaterial::setDisplacementScale);
    material.def_property("displacementOffset", &BasicMaterial::getDisplacementOffset, &BasicMaterial::setDisplacementOffset);
}
#endif

}  // namespace Falcor
