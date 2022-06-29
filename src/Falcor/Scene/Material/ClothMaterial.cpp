/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
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
#include "ClothMaterial.h"

namespace Falcor
{
    ClothMaterial::SharedPtr ClothMaterial::create(Device::SharedPtr pDevice, const std::string& name)
    {
        return SharedPtr(new ClothMaterial(pDevice, name));
    }

    ClothMaterial::ClothMaterial(Device::SharedPtr pDevice, const std::string& name)
        : BasicMaterial(pDevice, name, MaterialType::Cloth)
    {
        // Setup additional texture slots.
        mTextureSlotInfo[(uint32_t)TextureSlot::BaseColor] = { "baseColor", TextureChannelFlags::RGBA, true };
        mTextureSlotInfo[(uint32_t)TextureSlot::Specular] = { "specular", TextureChannelFlags::Green, false };
        mTextureSlotInfo[(uint32_t)TextureSlot::Normal] = { "normal", TextureChannelFlags::RGB, false };
    }

    void ClothMaterial::setRoughness(float roughness)
    {
        if (mData.roughness != (float16_t)roughness)
        {
            mData.roughness = (float16_t)roughness;
            markUpdates(UpdateFlags::DataChanged);
        }
    }

#ifdef SCRIPTING
    SCRIPT_BINDING(ClothMaterial)
    {
        SCRIPT_BINDING_DEPENDENCY(BasicMaterial)

        pybind11::class_<ClothMaterial, BasicMaterial, ClothMaterial::SharedPtr> material(m, "ClothMaterial");
        material.def(pybind11::init(&ClothMaterial::create), "name"_a = "");

        material.def_property("roughness", &ClothMaterial::getRoughness, &ClothMaterial::setRoughness);
    }
#endif
}
