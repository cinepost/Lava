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
#include "StandardMaterial.h"

namespace Falcor {

    StandardMaterial::SharedPtr StandardMaterial::create(Device::SharedPtr pDevice, const std::string& name, ShadingModel shadingModel) {
        return SharedPtr(new StandardMaterial(pDevice, name, shadingModel));
    }

    StandardMaterial::StandardMaterial(Device::SharedPtr pDevice, const std::string& name, ShadingModel shadingModel)
        : BasicMaterial(pDevice, name, MaterialType::Standard)
    {
        setShadingModel(shadingModel);
        bool specGloss = getShadingModel() == ShadingModel::SpecGloss;

        // Setup additional texture slots.
        mTextureSlotInfo[(uint32_t)TextureSlot::BaseColor] = { specGloss ? "diffuse" : "baseColor", TextureChannelFlags::RGB, true };
        mTextureSlotInfo[(uint32_t)TextureSlot::Metallic] = specGloss ? TextureSlotInfo{ "metallic", TextureChannelFlags::Red, true } : TextureSlotInfo{ "metallic", TextureChannelFlags::Green | TextureChannelFlags::Blue, false };
        mTextureSlotInfo[(uint32_t)TextureSlot::Normal] = { "normal", TextureChannelFlags::RGB, false };
        mTextureSlotInfo[(uint32_t)TextureSlot::Emissive] = { "emissive", TextureChannelFlags::RGB, true };
        mTextureSlotInfo[(uint32_t)TextureSlot::Roughness] = { "roughness", TextureChannelFlags::Red, true };
        mTextureSlotInfo[(uint32_t)TextureSlot::Transmission] = { "transmission", TextureChannelFlags::RGB, true };
    }

    void StandardMaterial::setShadingModel(ShadingModel model) {
        if(model != ShadingModel::MetalRough && model != ShadingModel::SpecGloss) {
            throw std::runtime_error("ShadingModel must be MetalRough or SpecGloss");
        }

        if (getShadingModel() != model) {
            mData.setShadingModel(model);
            markUpdates(UpdateFlags::DataChanged);
        }
    }

    void StandardMaterial::setRoughness(float roughness) {
        if (getShadingModel() != ShadingModel::MetalRough) {
            LLOG_WRN << "Ignoring setRoughness(). Material '" << mName << "' does not use the metallic/roughness shading model.";
            return;
        }

        if (mData.roughness != (float16_t)roughness) {
            mData.roughness = (float16_t)roughness;
            markUpdates(UpdateFlags::DataChanged);
        }
    }

    void StandardMaterial::setMetallic(float metallic) {
        if (getShadingModel() != ShadingModel::MetalRough) {
            LLOG_WRN << "Ignoring setMetallic(). Material '" << mName << "' does not use the metallic/roughness shading model.";
            return;
        }

        if (mData.metallic != (float16_t)metallic) {
            mData.metallic = (float16_t)metallic;
            markUpdates(UpdateFlags::DataChanged);
        }
    }

    void StandardMaterial::setEmissiveColor(const float3& color) {
        if (mData.emissive != (float16_t3)color) {
            mData.emissive = (float16_t3)color;
            markUpdates(UpdateFlags::DataChanged);
            updateEmissiveFlag();
        }
    }

    void StandardMaterial::setEmissiveFactor(float factor) {
        if (mData.emissiveFactor != factor) {
            mData.emissiveFactor = factor;
            markUpdates(UpdateFlags::DataChanged);
            updateEmissiveFlag();
        }
    }

#ifdef SCRIPTING
    SCRIPT_BINDING(StandardMaterial) {
        SCRIPT_BINDING_DEPENDENCY(BasicMaterial)

        pybind11::enum_<ShadingModel> shadingModel(m, "ShadingModel");
        shadingModel.value("MetalRough", ShadingModel::MetalRough);
        shadingModel.value("SpecGloss", ShadingModel::SpecGloss);

        pybind11::class_<StandardMaterial, BasicMaterial, StandardMaterial::SharedPtr> material(m, "StandardMaterial");
        material.def(pybind11::init(&StandardMaterial::create), "name"_a = "", "model"_a = ShadingModel::MetalRough);

        material.def_property("roughness", &StandardMaterial::getRoughness, &StandardMaterial::setRoughness);
        material.def_property("metallic", &StandardMaterial::getMetallic, &StandardMaterial::setMetallic);
        material.def_property("emissiveColor", &StandardMaterial::getEmissiveColor, &StandardMaterial::setEmissiveColor);
        material.def_property("emissiveFactor", &StandardMaterial::getEmissiveFactor, &StandardMaterial::setEmissiveFactor);
        material.def_property_readonly("shadingModel", &StandardMaterial::getShadingModel);

        // Register alias Material -> StandardMaterial to allow deprecated script syntax.
        // TODO: Remove workaround when all scripts have been updated to create StandardMaterial directly.
        m.attr("Material") = m.attr("StandardMaterial"); // PYTHONDEPRECATED
    }
#endif

}  // namespace Falcor
