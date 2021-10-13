/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#include "Material.h"

#include "Falcor/Core/API/ResourceManager.h"
#include "Core/Program/GraphicsProgram.h"
#include "Core/Program/ProgramVars.h"
#include "Utils/Color/ColorHelpers.slang"

namespace Falcor {

namespace {
    // Constants.
    const float kMaxVolumeAnisotropy = 0.99f;
}

static_assert(sizeof(MaterialData) % 16 == 0, "Material::MaterialData size should be a multiple of 16");
static_assert((MATERIAL_FLAGS_BITS) <= 32, "Material::MaterialData flags should be maximum 32 bits");
static_assert(static_cast<uint32_t>(MaterialType::Count) <= (1u << kMaterialTypeBits), "MaterialType count exceeds the maximum");

Material::UpdateFlags Material::sGlobalUpdates = Material::UpdateFlags::None;

Material::Material(std::shared_ptr<Device> pDevice, const std::string& name) : mpDevice(pDevice), mName(name) {
    // Call update functions to ensure a valid initial state based on default material parameters.
    updateBaseColorType();
    updateSpecularType();
    updateRoughnessType();
    updateEmissiveType();
    updateTransmissionType();
    updateAlphaMode();
    updateNormalMapMode();
    updateDisplacementFlag();
}

Material::SharedPtr Material::create(std::shared_ptr<Device> pDevice, const std::string& name)
{
    assert(pDevice);
    Material* pMaterial = new Material(pDevice, name);
    return SharedPtr(pMaterial);
}

Material::~Material() = default;

void Material::setType(MaterialType type)
{
    if (mData.type != static_cast<uint32_t>(type))
    {
        mData.type = static_cast<uint32_t>(type);
        markUpdates(UpdateFlags::DataChanged);
    }
}

void Material::setShadingModel(uint32_t model)
{
    setFlags(PACK_SHADING_MODEL(mData.flags, model));
}

void Material::setAlphaMode(uint32_t alphaMode)
{
    setFlags(PACK_ALPHA_MODE(mData.flags, alphaMode));
}

void Material::setDoubleSided(bool doubleSided)
{
    setFlags(PACK_DOUBLE_SIDED(mData.flags, doubleSided ? 1 : 0));
}

void Material::setAlphaThreshold(float alpha)
{
    if (mData.alphaThreshold != alpha)
    {
        mData.alphaThreshold = alpha;
        markUpdates(UpdateFlags::DataChanged);
    }
}

void Material::setIndexOfRefraction(float IoR) {
    if (mData.IoR != IoR) {
        mData.IoR = IoR;
        markUpdates(UpdateFlags::DataChanged);
    }
}

void Material::setReflectivity(float reflectivity) {
    if (mData.reflectivity != reflectivity) {
        mData.reflectivity = reflectivity;
        markUpdates(UpdateFlags::DataChanged);
    }
}

void Material::setNestedPriority(uint32_t priority)
{
    const uint32_t maxPriority = (1U << NESTED_PRIORITY_BITS) - 1;
    if (priority > maxPriority)
    {
        logWarning("Requested nested priority " + std::to_string(priority) + " for material '" + mName + "' is out of range. Clamping to " + std::to_string(maxPriority) + ".");
        priority = maxPriority;
    }
    setFlags(PACK_NESTED_PRIORITY(mData.flags, priority));
}

void Material::setSampler(Sampler::SharedPtr pSampler)
{
    if (pSampler != mResources.samplerState)
    {
        mResources.samplerState = pSampler;
        markUpdates(UpdateFlags::ResourcesChanged);
    }
}

void Material::setTexture(TextureSlot slot, Texture::SharedPtr pTexture)
{
    if (pTexture == getTexture(slot)) return;

    switch (slot)
    {
    case TextureSlot::BaseColor:
        // Assume the texture is non-constant and has full alpha range.
        // This may be changed later by optimizeTexture().
        if (pTexture)
        {
            mAlphaRange = float2(0.f, 1.f);
            mIsTexturedBaseColorConstant = mIsTexturedAlphaConstant = false;
        }
        mResources.baseColor = pTexture;
        updateBaseColorType();
        updateAlphaMode();
        break;
        case TextureSlot::Specular:
            mResources.specular = pTexture;
            updateSpecularType();
            break;
        case TextureSlot::Roughness:
            mResources.roughness = pTexture;
            updateRoughnessType();
            break;
        case TextureSlot::Emissive:
            mResources.emissive = pTexture;
            updateEmissiveType();
            break;
        case TextureSlot::Normal:
            mResources.normalMap = pTexture;
            updateNormalMapMode();
            break;
        case TextureSlot::Displacement:
            mResources.displacementMap = pTexture;
            updateDisplacementFlag();
            updateDoubleSidedFlag();
            break;
        case TextureSlot::Transmission:
            mResources.transmission = pTexture;
            updateTransmissionType();
            updateDoubleSidedFlag();
            break;
    default:
        should_not_get_here();
    }
}

Texture::SharedPtr Material::getTexture(TextureSlot slot) const
{
    switch (slot)
    {
        case TextureSlot::BaseColor:
            return mResources.baseColor;
        case TextureSlot::Specular:
            return mResources.specular;
        case TextureSlot::Roughness:
            return mResources.roughness;
        case TextureSlot::Emissive:
            return mResources.emissive;
        case TextureSlot::Normal:
            return mResources.normalMap;
        case TextureSlot::Displacement:
            return mResources.displacementMap;
        case TextureSlot::Transmission:
            return mResources.transmission;
        default:
            should_not_get_here();
    }
    return nullptr;
}

void Material::optimizeTexture(TextureSlot slot, const TextureAnalyzer::Result& texInfo, TextureOptimizationStats& stats)
{

}

void Material::prepareDisplacementMapForRendering() {
    if (getTexture(TextureSlot::Displacement) != nullptr)
    {
        // Creates RGBA texture with MIP pyramid containing average, min, max values.
        Falcor::ResourceFormat oldFormat = mResources.displacementMap->getFormat();

        // Replace texture with a 4 component one if necessary.
        if (getFormatChannelCount(oldFormat) < 4)
        {
            Falcor::ResourceFormat newFormat = ResourceFormat::RGBA16Float;
            Resource::BindFlags bf = mResources.displacementMap->getBindFlags() | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::RenderTarget;
            Texture::SharedPtr newDisplacementTex = Texture::create2D(mpDevice, mResources.displacementMap->getWidth(), mResources.displacementMap->getHeight(), newFormat, mResources.displacementMap->getArraySize(), Resource::kMaxPossible, nullptr, bf);

            // Copy base level.
            RenderContext* pContext = mpDevice->getRenderContext();
            uint32_t arraySize = mResources.displacementMap->getArraySize();
            for (uint32_t a = 0; a < arraySize; a++)
            {
                auto srv = mResources.displacementMap->getSRV(0, 1, a, 1);
                auto rtv = newDisplacementTex->getRTV(0, a, 1);
                const Sampler::ReductionMode redModes[] = { Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard };
                const float4 componentsTransform[] = { float4(1.0f, 0.0f, 0.0f, 0.0f), float4(1.0f, 0.0f, 0.0f, 0.0f), float4(1.0f, 0.0f, 0.0f, 0.0f), float4(1.0f, 0.0f, 0.0f, 0.0f) };
                //pContext->blit(srv, rtv, uint4(-1), uint4(-1), Sampler::Filter::Linear, redModes, componentsTransform);
            }

            mResources.displacementMap = newDisplacementTex;
        }

        // Build min/max MIPS.
        mResources.displacementMap->generateMips(mpDevice->getRenderContext());
    }
}

uint2 Material::getMaxTextureDimensions() const
{
    uint2 dim = uint2(0);
    for (uint32_t i = 0; i < (uint32_t)TextureSlot::Count; i++)
    {
        const auto& t = getTexture((TextureSlot)i);
        if (t) dim = max(dim, uint2(t->getWidth(), t->getHeight()));
    }
    return dim;
}

void Material::loadTexture(TextureSlot slot, const std::string& filename, bool useSrgb) {
    assert(mpDevice);
    std::string fullpath;
    if (findFileInDataDirectories(filename, fullpath)) {
        auto pTexture = mpDevice->resourceManager()->createSparseTextureFromFile(fullpath, true, useSrgb && isSrgbTextureRequired(slot));
        if (pTexture) {
            setTexture(slot, pTexture);
            // Flush and sync in order to prevent the upload heap from growing too large. Doing so after
            // every texture creation is overly conservative, and will likely lead to performance issues
            // due to the forced CPU/GPU sync.
            mpDevice->flushAndSync();
        }
    }
}

void Material::clearTexture(TextureSlot slot) {
    setTexture(slot, nullptr);
}

bool Material::isSrgbTextureRequired(TextureSlot slot) {
    uint32_t shadingModel = getShadingModel();

    switch (slot) {
        case TextureSlot::Specular:
            return (shadingModel == ShadingModelSpecGloss);
        case TextureSlot::BaseColor:
        case TextureSlot::Emissive:
        case TextureSlot::Transmission:
            return true;
        case TextureSlot::Normal:
        case TextureSlot::Displacement:
            return false;
        default:
            should_not_get_here();
            return false;
    }
}

void Material::setDisplacementScale(float scale) {
    if (mData.displacementScale != scale) {
        mData.displacementScale = scale;
        markUpdates(UpdateFlags::DataChanged | UpdateFlags::DisplacementChanged);
    }
}

void Material::setDisplacementOffset(float offset) {
    if (mData.displacementOffset != offset) {
        mData.displacementOffset = offset;
        markUpdates(UpdateFlags::DataChanged | UpdateFlags::DisplacementChanged);
    }
}

void Material::setBaseColorTexture(Texture::SharedPtr pBaseColor) {
    if (mResources.baseColor != pBaseColor) {
        mResources.baseColor = pBaseColor;
        markUpdates(UpdateFlags::ResourcesChanged);
        updateBaseColorType();
        bool hasAlpha = pBaseColor && doesFormatHasAlpha(pBaseColor->getFormat());
        setAlphaMode(hasAlpha ? AlphaModeMask : AlphaModeOpaque);
    }
}

void Material::setSpecularTexture(Texture::SharedPtr pSpecular) {
    if (mResources.specular != pSpecular) {
        mResources.specular = pSpecular;
        markUpdates(UpdateFlags::ResourcesChanged);
        updateSpecularType();
    }
}

void Material::setRoughnessTexture(Texture::SharedPtr pRoughness) {
    if (mResources.roughness != pRoughness) {
        mResources.roughness = pRoughness;
        markUpdates(UpdateFlags::ResourcesChanged);
        updateRoughnessType();
    }
}

void Material::setEmissiveTexture(const Texture::SharedPtr& pEmissive) {
    if (mResources.emissive != pEmissive) {
        mResources.emissive = pEmissive;
        markUpdates(UpdateFlags::ResourcesChanged);
        updateEmissiveType();
    }
}

void Material::setBaseColor(const float4& color) {
    if (mData.baseColor != color) {
        mData.baseColor = color;
        markUpdates(UpdateFlags::DataChanged);
        updateBaseColorType();
    }
}

void Material::setSpecularParams(const float4& color) {
    if (mData.specular != color) {
        mData.specular = color;
        markUpdates(UpdateFlags::DataChanged);
        updateSpecularType();
    }
}

void Material::setRoughness(float roughness) {
    if (getShadingModel() != ShadingModelMetalRough) {
        logWarning("Ignoring setRoughness(). Material '" + mName + "' does not use the metallic/roughness shading model.");
        return;
    }

    if (mData.roughness != roughness) {
        mData.roughness = roughness;
        markUpdates(UpdateFlags::DataChanged);
        updateRoughnessType();
    }
}

void Material::setMetallic(float metallic) {
    if (getShadingModel() != ShadingModelMetalRough) {
        logWarning("Ignoring setMetallic(). Material '" + mName + "' does not use the metallic/roughness shading model.");
        return;
    }

    if (mData.specular.b != metallic) {
        mData.specular.b = metallic;
        markUpdates(UpdateFlags::DataChanged);
        updateSpecularType();
    }
}

void Material::setDiffuseTransmission(float diffuseTransmission) {
    if (mData.diffuseTransmission != diffuseTransmission) {
        mData.diffuseTransmission = diffuseTransmission;
        markUpdates(UpdateFlags::DataChanged);
        updateDoubleSidedFlag();
    }
}


void Material::setSpecularTransmission(float specularTransmission) {
    if (mData.specularTransmission != specularTransmission) {
        mData.specularTransmission = specularTransmission;
        markUpdates(UpdateFlags::DataChanged);
        updateDoubleSidedFlag();
    }
}

void Material::setTransmissionColor(const float3& transmissionColor) {
    if (mData.transmission != transmissionColor) {
        mData.transmission = transmissionColor;
        markUpdates(UpdateFlags::DataChanged);
        updateTransmissionType();
    }
}

void Material::setVolumeAbsorption(const float3& volumeAbsorption) {
    if (mData.volumeAbsorption != volumeAbsorption) {
        mData.volumeAbsorption = volumeAbsorption;
        markUpdates(UpdateFlags::DataChanged);
    }
}

void Material::setEmissiveColor(const float3& color) {
    if (mData.emissive != color) {
        mData.emissive = color;
        markUpdates(UpdateFlags::DataChanged);
        updateEmissiveType();
    }
}

void Material::setEmissiveFactor(float factor) {
    if (mData.emissiveFactor != factor) {
        mData.emissiveFactor = factor;
        markUpdates(UpdateFlags::DataChanged);
        updateEmissiveType();
    }
}

void Material::setNormalMap(Texture::SharedPtr pNormalMap) {
    if (mResources.normalMap != pNormalMap) {
        mResources.normalMap = pNormalMap;
        markUpdates(UpdateFlags::ResourcesChanged);
        uint32_t normalMode = NormalMapUnused;
        if (pNormalMap) {
            switch(getFormatChannelCount(pNormalMap->getFormat())) {
                case 2:
                    normalMode = NormalMapRG;
                    break;
                case 3:
                case 4: // Some texture formats don't support RGB, only RGBA. We have no use for the alpha channel in the normal map.
                    normalMode = NormalMapRGB;
                    break;
                default:
                    should_not_get_here();
                    logWarning("Unsupported normal map format for material " + mName);
            }
        }
        setFlags(PACK_NORMAL_MAP_TYPE(mData.flags, normalMode));
    }
}


bool Material::operator==(const Material& other) const
{
#define compare_field(_a) if (mData._a != other.mData._a) return false
        compare_field(baseColor);
        compare_field(specular);
        compare_field(emissive);
        compare_field(roughness);
        compare_field(emissiveFactor);
        compare_field(alphaThreshold);
        compare_field(IoR);
        compare_field(diffuseTransmission);
        compare_field(specularTransmission);
        compare_field(transmission);
        compare_field(volumeAbsorption);
        compare_field(volumeAnisotropy);
        compare_field(volumeScattering);
        compare_field(flags);
        compare_field(type);
        compare_field(displacementScale);
        compare_field(displacementOffset);
#undef compare_field

#define compare_texture(_a) if (mResources._a != other.mResources._a) return false
        compare_texture(baseColor);
        compare_texture(specular);
        compare_texture(emissive);
        compare_texture(roughness);
        compare_texture(normalMap);
        compare_texture(transmission);
        compare_texture(displacementMap);
#undef compare_texture
    if (mResources.samplerState != other.mResources.samplerState) return false;
    return true;
}

void Material::markUpdates(UpdateFlags updates)
{
    mUpdates |= updates;
    sGlobalUpdates |= updates;
}

void Material::setFlags(uint32_t flags)
{
    if (mData.flags != flags)
    {
        mData.flags = flags;
        markUpdates(UpdateFlags::DataChanged);
    }
}

template<typename vec>
static uint32_t getChannelMode(bool hasTexture, const vec& color) {
    if (hasTexture) return ChannelTypeTexture;
    if (luminance(color) == 0) return ChannelTypeUnused;
    return ChannelTypeConst;
}

void Material::updateBaseColorType() {
    bool useTexture = mResources.baseColor != nullptr && !mIsTexturedBaseColorConstant;
    setFlags(PACK_BASE_COLOR_TYPE(mData.flags, getChannelMode(useTexture, mData.baseColor.rgb)));
}

void Material::updateSpecularType() {
    setFlags(PACK_SPECULAR_TYPE(mData.flags, getChannelMode(mResources.specular != nullptr, mData.specular)));
}

void Material::updateRoughnessType() {
    setFlags(PACK_ROUGHNESS_TYPE(mData.flags, getChannelMode(mResources.roughness != nullptr, mData.roughness)));
}

void Material::updateEmissiveType() {
    setFlags(PACK_EMISSIVE_TYPE(mData.flags, getChannelMode(mResources.emissive != nullptr, mData.emissive * mData.emissiveFactor)));
}

void Material::updateTransmissionType() {
    setFlags(PACK_TRANS_TYPE(mData.flags, getChannelMode(mResources.transmission != nullptr, mData.transmission)));
}

void Material::updateAlphaMode() {
    // Decide how alpha channel should be accessed.
    bool hasAlpha = mResources.baseColor && doesFormatHasAlpha(mResources.baseColor->getFormat());
    bool useTexture = hasAlpha && !mIsTexturedAlphaConstant;
    setFlags(PACK_ALPHA_TYPE(mData.flags, getChannelMode(useTexture, mData.baseColor.a)));

    // Set alpha range to the fixed alpha value if non-textured.
    if (!hasAlpha) mAlphaRange = float2(mData.baseColor.a);

    // Decide if we need to run the alpha test.
    // This is derived from the current alpha threshold and conservative alpha range.
    // If the test will never fail we disable it. This optimization assumes basic alpha thresholding.
    // TODO: Update the logic if other alpha modes are added.
    bool useAlpha = mAlphaRange.x < mData.alphaThreshold;
    setAlphaMode(useAlpha ? AlphaModeMask : AlphaModeOpaque);
}

void Material::updateNormalMapMode() {
    uint32_t normalMode = NormalMapUnused;
    if (mResources.normalMap) {
        switch(getFormatChannelCount(mResources.normalMap->getFormat())) {
            case 2:
                normalMode = NormalMapRG;
                break;
            case 3:
            case 4: // Some texture formats don't support RGB, only RGBA. We have no use for the alpha channel in the normal map.
                normalMode = NormalMapRGB;
                break;
            default:
                should_not_get_here();
                logWarning("Unsupported normal map format for material " + mName);
        }
    }
    setFlags(PACK_NORMAL_MAP_TYPE(mData.flags, normalMode));
}

void Material::updateDoubleSidedFlag() {
    bool doubleSided = mDoubleSided;
    // Make double sided if diffuse or specular transmission is used.
    if (mData.diffuseTransmission > 0.f || mData.specularTransmission > 0.f) doubleSided = true;
    // Make double sided if a dispacement map is used since backfacing surfaces can become frontfacing.
    if (mResources.displacementMap != nullptr) doubleSided = true;
    setFlags(PACK_DOUBLE_SIDED(mData.flags, doubleSided ? 1 : 0));
}

void Material::updateDisplacementFlag() {
    bool hasMap = (mResources.displacementMap != nullptr);
    setFlags(PACK_DISPLACEMENT_MAP(mData.flags, hasMap ? 1 : 0));
}

#ifdef SCRIPTING
SCRIPT_BINDING(Material)
{
    pybind11::enum_<Material::TextureSlot> textureSlot(m, "MaterialTextureSlot");
    textureSlot.value("BaseColor", Material::TextureSlot::BaseColor);
    textureSlot.value("Specular", Material::TextureSlot::Specular);
    textureSlot.value("Roughness", Material::TextureSlot::Roughness);
    textureSlot.value("Emissive", Material::TextureSlot::Emissive);
    textureSlot.value("Normal", Material::TextureSlot::Normal);
    textureSlot.value("Occlusion", Material::TextureSlot::Occlusion);
    textureSlot.value("SpecularTransmission", Material::TextureSlot::SpecularTransmission);

    pybind11::class_<Material, Material::SharedPtr> material(m, "Material");
    material.def_property_readonly("name", &Material::getName);
    material.def_property("baseColor", &Material::getBaseColor, &Material::setBaseColor);
    material.def_property("specularParams", &Material::getSpecularParams, &Material::setSpecularParams);
    material.def_property("roughness", &Material::getRoughness, &Material::setRoughness);
    material.def_property("metallic", &Material::getMetallic, &Material::setMetallic);
    material.def_property("specularTransmission", &Material::getSpecularTransmission, &Material::setSpecularTransmission);
    material.def_property("volumeAbsorption", &Material::getVolumeAbsorption, &Material::setVolumeAbsorption);
    material.def_property("indexOfRefraction", &Material::getIndexOfRefraction, &Material::setIndexOfRefraction);
    material.def_property("emissiveColor", &Material::getEmissiveColor, &Material::setEmissiveColor);
    material.def_property("emissiveFactor", &Material::getEmissiveFactor, &Material::setEmissiveFactor);
    material.def_property("alphaMode", &Material::getAlphaMode, &Material::setAlphaMode);
    material.def_property("alphaThreshold", &Material::getAlphaThreshold, &Material::setAlphaThreshold);
    material.def_property("doubleSided", &Material::isDoubleSided, &Material::setDoubleSided);
    material.def_property("nestedPriority", &Material::getNestedPriority, &Material::setNestedPriority);

    material.def("loadTexture", &Material::loadTexture, "slot"_a, "filename"_a, "useSrgb"_a = true);
    material.def("clearTexture", &Material::clearTexture, "slot"_a);
}
#endif

}  // namespace Falcor
