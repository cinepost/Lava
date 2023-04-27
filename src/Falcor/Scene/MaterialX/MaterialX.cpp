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
#include <regex>

// #ifdef _WIN32
// #include <filesystem>
// namespace fs = std::filesystem;
// #else
#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;
// #endif

#include <boost/format.hpp>

#include "stdafx.h"
#include "MaterialX.h"

#include "Core/Program/GraphicsProgram.h"
#include "Core/Program/ProgramVars.h"
#include "Utils/Color/ColorHelpers.slang"
#include "Falcor/Utils/Debug/debug.h"


namespace Falcor {

MaterialX::MaterialX(std::shared_ptr<Device> pDevice, const std::string& name) : mpDevice(pDevice), mName(name) {
    MxNode::TypeCreateInfo info = {};
    info.nameSpace = "";
    info.typeName = "";
    info.version = 0;
    mpMxRoot = MxNode::create(info, mName, nullptr); // special case manager-like root node
}

MaterialX::SharedPtr MaterialX::createShared(std::shared_ptr<Device> pDevice, const std::string& name) {
    return std::make_shared<MaterialX>(pDevice, name);
}

MaterialX::UniquePtr MaterialX::createUnique(std::shared_ptr<Device> pDevice, const std::string& name) {
    return std::make_unique<MaterialX>(pDevice, name);
}

MaterialX::~MaterialX() = default;

MxNode::SharedPtr MaterialX::createNode(const MxNode::TypeCreateInfo& info, const std::string& name) {
    return mpMxRoot->createNode(info, name);
}

void MaterialX::setDevice(std::shared_ptr<Device> pDevice) {
    assert(pDevice);
    if (!mpDevice) mpDevice = pDevice;
}

bool MaterialX::operator==(const MaterialX& other) const {
/*
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
*/
    return true;
}






#ifdef SCRIPTING
/*
SCRIPT_BINDING(Material) {
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
*/
#endif

}  // namespace Falcor
