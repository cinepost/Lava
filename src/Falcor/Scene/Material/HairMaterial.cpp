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
#include "HairMaterial.h"

namespace Falcor
{
    HairMaterial::SharedPtr HairMaterial::create(Device::SharedPtr pDevice, const std::string& name)
    {
        return SharedPtr(new HairMaterial(pDevice, name));
    }

    HairMaterial::HairMaterial(Device::SharedPtr pDevice, const std::string& name)
        : BasicMaterial(pDevice, name, MaterialType::Hair)
    {
        // Setup additional texture slots.
        mTextureSlotInfo[(uint32_t)TextureSlot::BaseColor] = { "baseColor", TextureChannelFlags::RGB, true }; // Note: No alpha support
        mTextureSlotInfo[(uint32_t)TextureSlot::Metallic] = { "metallic", TextureChannelFlags::Red, false };
    }

#ifdef SCRIPTING
    SCRIPT_BINDING(HairMaterial)
    {
        SCRIPT_BINDING_DEPENDENCY(BasicMaterial)

        pybind11::class_<HairMaterial, BasicMaterial, HairMaterial::SharedPtr> material(m, "HairMaterial");
        material.def(pybind11::init(&HairMaterial::create), "name"_a = "");
    }
#endif
}
