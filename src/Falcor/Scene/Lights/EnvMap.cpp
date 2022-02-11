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
#include "EnvMap.h"
#include "glm/gtc/integer.hpp"
#include "glm/gtx/euler_angles.hpp"

namespace Falcor {

EnvMap::SharedPtr EnvMap::create(Device::SharedPtr pDevice, const Texture::SharedPtr& pTexture) {
    return SharedPtr(new EnvMap(pDevice, pTexture));
}

EnvMap::SharedPtr EnvMap::create(Device::SharedPtr pDevice, const std::string& filename) {
    // Load environment map from file. Set it to generate mips and use linear color.
    auto pTexture = Texture::createFromFile(pDevice, filename, true, false);
    if (!pTexture) throw std::runtime_error("Failed to load environment map texture");
    return create(pDevice, pTexture);
}

void EnvMap::setRotation(float3 degreesXYZ)
{
    if (degreesXYZ != mRotation)
    {
        mRotation = degreesXYZ;

        auto transform = glm::eulerAngleXYZ(glm::radians(mRotation.x), glm::radians(mRotation.y), glm::radians(mRotation.z));

        mData.transform = static_cast<float3x4>(transform);
        mData.invTransform = static_cast<float3x4>(glm::inverse(transform));
    }
}

void EnvMap::setIntensity(float intensity)
{
    mData.intensity = intensity;
}

void EnvMap::setTint(const float3& tint)
{
    mData.tint = tint;
}

void EnvMap::setShaderData(const ShaderVar& var) const
{
    assert(var.isValid());

    // Set variables.
    var["data"].setBlob(mData);

    // Bind resources.
    var["envMap"].setTexture(mpEnvMap);
    var["envSampler"].setSampler(mpEnvSampler);
}

EnvMap::Changes EnvMap::beginFrame()
{
    mChanges = Changes::None;

    if (mData.transform != mPrevData.transform) mChanges |= Changes::Transform;
    if (mData.intensity != mPrevData.intensity) mChanges |= Changes::Intensity;
    if (mData.tint != mPrevData.tint) mChanges |= Changes::Intensity;

    mPrevData = mData;

    return getChanges();
}

uint64_t EnvMap::getMemoryUsageInBytes() const
{
    return mpEnvMap ? mpEnvMap->getTextureSizeInBytes() : 0;
}

EnvMap::EnvMap(Device::SharedPtr pDevice, const Texture::SharedPtr& texture)
{
    if (!texture) throw std::runtime_error("Failed to create environment map without texture");

    mpDevice =pDevice;
    mpEnvMap = texture;

    // Create sampler.
    // The lat-long map wraps around horizontally, but not vertically. Set the sampler to only wrap in U.
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    samplerDesc.setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    mpEnvSampler = Sampler::create(mpDevice, samplerDesc);
}

#ifdef SCRIPTING
SCRIPT_BINDING(EnvMap)
{
    pybind11::class_<EnvMap, EnvMap::SharedPtr> envMap(m, "EnvMap");
    envMap.def(pybind11::init(pybind11::overload_cast<const std::string&>(&EnvMap::create)), "filename"_a);
    envMap.def_property_readonly("filename", &EnvMap::getFilename);
    envMap.def_property("rotation", &EnvMap::getRotation, &EnvMap::setRotation);
    envMap.def_property("intensity", &EnvMap::getIntensity, &EnvMap::setIntensity);
    envMap.def_property("tint", &EnvMap::getTint, &EnvMap::setTint);
}
#endif SCRIPTING
}