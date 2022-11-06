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

#include "Falcor/Core/Program/ProgramReflection.h"
#include "Falcor/Core/Program/ShaderVar.h"
#include "Falcor/Utils/Color/ColorHelpers.slang"

#include "Light.h"

namespace Falcor {

static_assert(sizeof(LightData) % 16 == 0, "LightData struct size should be a multiple of 16");

static float16_t kMinColorComponentContribution = (float16_t)0.0001f;
static constexpr float M_2PI = (float)M_PI * 2.0f;

static inline bool checkOffset(const std::string& structName, UniformShaderVarOffset cbOffset, size_t cppOffset, const char* field) {
    if (cbOffset.getByteOffset() != cppOffset) {
        LLOG_ERR << "Light::" << std::string(structName) << ":: " << std::string(field) << " CB offset mismatch. CB offset is " << std::to_string(cbOffset.getByteOffset()) 
        << ", C++ data offset is " << std::to_string(cppOffset);
        return false;
    }
    return true;
}

static inline float16_t maxColorComponentValue(const float16_t3& color) {
    return std::max(std::max(color[0], color[1]), color[2]);
}

// Light

void Light::setActive(bool active) {
    if (active != mActive) {
        mActive = active;
        mActiveChanged = true;
    }
}

void Light::setDiffuseIntensity(const float3& intensity) {
    mData.diffuseIntensity = (float16_t3)(intensity * M_2PI); // We do this to match mantra intensity
}

void Light::setSpecularIntensity(const float3& intensity) {
    mData.specularIntensity = (float16_t3)(intensity * M_2PI); // We do this to match mantra intensity
}

void Light::setIndirectDiffuseIntensity(const float3& intensity) {
    mData.indirectDiffuseIntensity = (float16_t3)(intensity * M_2PI); // We do this to match mantra intensity
}

void Light::setIndirectSpecularIntensity(const float3& intensity) {
    mData.indirectSpecularIntensity = (float16_t3)(intensity * M_2PI); // We do this to match mantra intensity
}

void Light::setShadowColor(const float3& shadowColor) {
    mData.shadowColor = (float16_t3)shadowColor;
}

void Light::setShadowType(LightShadowType shadowType) { 
    mData.shadowType = (uint32_t)shadowType; 
}

void Light::setLightRadius(float radius) {
    mData.radius = radius;
}

void Light::update() {
    if(maxColorComponentValue(mData.diffuseIntensity) > kMinColorComponentContribution) {
        mData.flags |= (uint32_t)LightDataFlags::ContribureDirectDiffuse;
    }

    if(maxColorComponentValue(mData.specularIntensity) > kMinColorComponentContribution) {
        mData.flags |= (uint32_t)LightDataFlags::ContributeDirectSpecular;
    }

    if(maxColorComponentValue(mData.indirectDiffuseIntensity) > kMinColorComponentContribution) {
        mData.flags |= (uint32_t)LightDataFlags::ContributeIndirectDiffuse;
    }

    if(maxColorComponentValue(mData.indirectSpecularIntensity) > kMinColorComponentContribution) {
        mData.flags |= (uint32_t)LightDataFlags::ContributeIndirectSpecular;
    }
}

Light::Changes Light::beginFrame() {
    mChanges = Changes::None;
    if (mActiveChanged) mChanges |= Changes::Active;
    if (mPrevData.posW != mData.posW) mChanges |= Changes::Position;
    if (mPrevData.dirW != mData.dirW) mChanges |= Changes::Direction;
    
    if (mPrevData.diffuseIntensity != mData.diffuseIntensity) mChanges |= Changes::Intensity;
    if (mPrevData.specularIntensity != mData.specularIntensity) mChanges |= Changes::Intensity;
    if (mPrevData.indirectDiffuseIntensity != mData.indirectDiffuseIntensity) mChanges |= Changes::Intensity;
    if (mPrevData.indirectSpecularIntensity != mData.indirectSpecularIntensity) mChanges |= Changes::Intensity;

    if (mPrevData.openingAngle != mData.openingAngle) mChanges |= Changes::SurfaceArea;
    if (mPrevData.penumbraAngle != mData.penumbraAngle) mChanges |= Changes::SurfaceArea;
    if (mPrevData.cosSubtendedAngle != mData.cosSubtendedAngle) mChanges |= Changes::SurfaceArea;
    
    if (mPrevData.surfaceArea != mData.surfaceArea) mChanges |= Changes::SurfaceArea;
    if (mPrevData.transMat != mData.transMat) mChanges |= (Changes::Position | Changes::Direction);

    if (mPrevData.flags != mData.flags) mChanges |= Changes::Flags;

    if (mPrevData.shadowType != mData.shadowType) mChanges |= Changes::Shadow;
    if (mPrevData.shadowColor != mData.shadowColor) mChanges |= Changes::Shadow;
    if (mPrevData.radius != mData.radius) mChanges |= Changes::Shadow;

    assert(mPrevData.tangent == mData.tangent);
    assert(mPrevData.bitangent == mData.bitangent);

    mPrevData = mData;
    mActiveChanged = false;

    return getChanges();
}

void Light::setShaderData(const ShaderVar& var) {
//#if _LOG_ENABLED
#define check_offset(_a) {static bool b = true; if(b) {assert(checkOffset("LightData", var.getType()->getMemberOffset(#_a), offsetof(LightData, _a), #_a));} b = false;}
    check_offset(dirW);
    check_offset(diffuseIntensity);
    check_offset(specularIntensity);
    check_offset(penumbraAngle);
    check_offset(transMat);
#undef check_offset
//#endif

    var.setBlob(mData);
}

Light::Light(const std::string& name, LightType type) : mName(name) {
    mData.type = (uint32_t)type;
    mData.flags = 0x0;
}

void Light::setTexture(Texture::SharedPtr pTexture) {
    mpTexture = pTexture;
}

// PointLight

PointLight::SharedPtr PointLight::create(const std::string& name) {
    return SharedPtr(new PointLight(name));
}

PointLight::PointLight(const std::string& name) : Light(name, LightType::Point) {
    mPrevData = mData;
    mData.flags |= (uint32_t)LightDataFlags::DeltaPosition; 
}

void PointLight::setWorldDirection(const float3& dir) {
    if (!(glm::length(dir) > 0.f)) { 
        // NaNs propagate
        LLOG_WRN << "Can't set light direction to zero length vector. Ignoring call.";
        return;
    }
    mData.dirW = normalize(dir);
    update();
}

void PointLight::update() {
    // Update transformation matrices
    // Assumes that mData.dirW is normalized
    const float3 up(0.f, 0.f, 1.f);
    float3 vec = glm::cross(up, -mData.dirW);
    float sinTheta = glm::length(vec);
    
    if (sinTheta > 0.f) {
        float cosTheta = glm::dot(up, -mData.dirW);
        mData.transMat = glm::rotate(glm::mat4(), std::acos(cosTheta), vec);
    } else {
        mData.transMat = glm::mat4();
    }
    mData.transMatIT = glm::inverse(glm::transpose(mData.transMat));
    Light::update();
}

void PointLight::setWorldPosition(const float3& pos) {
    mData.posW = pos;
}

float PointLight::getPower() const {
    return luminance((float3)mData.diffuseIntensity) * 4.f * (float)M_PI;
}

void PointLight::setOpeningAngle(float openingAngle) {
    setOpeningHalfAngle(openingAngle * 0.5f);
}

void PointLight::setOpeningHalfAngle(float openingAngle) {
     openingAngle = glm::clamp(openingAngle, 0.f, (float)M_PI);
    if (openingAngle == mData.openingAngle) return;

    mData.openingAngle = openingAngle;
    mData.penumbraAngle = std::min(mData.penumbraAngle, openingAngle);

    // Prepare an auxiliary cosine of the opening angle to quickly check whether we're within the cone of a spot light.
    mData.cosOpeningAngle = std::cos(openingAngle);   
}

void PointLight::setPenumbraAngle(float angle) {
    setPenumbraHalfAngle(angle * 0.5f);
}

void PointLight::setPenumbraHalfAngle(float angle) {
    angle = glm::clamp(angle, 0.0f, mData.openingAngle);
    if (mData.penumbraAngle == angle) return;
    mData.penumbraAngle = angle;
}


void PointLight::updateFromAnimation(const glm::mat4& transform) {
    float3 fwd = float3(-transform[2]);
    float3 pos = float3(transform[3]);
    setWorldPosition(pos);
    setWorldDirection(fwd);
}

// DirectionalLight

DirectionalLight::DirectionalLight(const std::string& name) : Light(name, LightType::Directional) {
    mData.flags |= (uint32_t)LightDataFlags::DeltaDirection; 
    mPrevData = mData;
}

DirectionalLight::SharedPtr DirectionalLight::create(const std::string& name) {
    return SharedPtr(new DirectionalLight(name));
}

void DirectionalLight::setWorldDirection(const float3& dir) {
    if (!(glm::length(dir) > 0.f)) // NaNs propagate
    {
        LLOG_WRN << "Can't set light direction to zero length vector. Ignoring call.";
        return;
    }
    mData.dirW = normalize(dir);
}

void DirectionalLight::updateFromAnimation(const glm::mat4& transform) {
    float3 fwd = float3(-transform[2]);
    setWorldDirection(fwd);
}

// DistantLight

DistantLight::SharedPtr DistantLight::create(const std::string& name) {
    return SharedPtr(new DistantLight(name));
}

DistantLight::DistantLight(const std::string& name) : Light(name, LightType::Distant) {
    mData.dirW = float3(0.f, -1.f, 0.f);
    setAngle(0.5f * 0.53f * (float)M_PI / 180.f);   // Approximate sun half-angle
    update();
    mPrevData = mData;
}

void DistantLight::setDiffuseIntensity(const float3& intensity) {
    mDiffuseIntensity = intensity;
    update();
};

void DistantLight::setSpecularIntensity(const float3& intensity) {
    mSpecularIntensity = intensity;
    update();
};

void DistantLight::setAngle(float angle) {
    mAngle = glm::clamp(angle, 0.f, (float)M_PI_2);
    mData.cosSubtendedAngle = std::cos(mAngle);
    update();
}

void DistantLight::setAngleDegrees(float deg) {
    setAngle(( deg * M_PI ) / 180.0f);
}

void DistantLight::setWorldDirection(const float3& dir) {
    if (!(glm::length(dir) > 0.f)) // NaNs propagate
    {
        LLOG_WRN << "Can't set light direction to zero length vector. Ignoring call.";
        return;
    }
    mData.dirW = normalize(dir);
    update();
}

void DistantLight::update() {
    // Update transformation matrices
    // Assumes that mData.dirW is normalized
    const float3 up(0.f, 0.f, 1.f);
    float3 vec = glm::cross(up, -mData.dirW);
    float sinTheta = glm::length(vec);
    
    if (sinTheta > 0.f) {
        float cosTheta = glm::dot(up, -mData.dirW);
        mData.transMat = glm::rotate(glm::mat4(), std::acos(cosTheta), vec);
    } else {
        mData.transMat = glm::mat4();
    }
    mData.transMatIT = glm::inverse(glm::transpose(mData.transMat));

    if(mData.cosSubtendedAngle == 1.0f) {
        mData.flags |= (uint32_t)LightDataFlags::DeltaDirection;
        mData.diffuseIntensity = (float16_t3)mDiffuseIntensity;
        mData.specularIntensity = (float16_t3)mSpecularIntensity;
    } else {
        mData.flags &= !(uint32_t)LightDataFlags::DeltaDirection;
        mData.diffuseIntensity = (float16_t3)(mDiffuseIntensity / (1.0f - mData.cosSubtendedAngle));
        mData.specularIntensity = (float16_t3)(mSpecularIntensity / (1.0f - mData.cosSubtendedAngle));
    }
    Light::update();
}

void DistantLight::updateFromAnimation(const glm::mat4& transform) {
    float3 fwd = float3(-transform[2]);
    setWorldDirection(fwd);
}

// EnvironmentLight

EnvironmentLight::EnvironmentLight(const std::string& name, Texture::SharedPtr pTexture) : Light(name, LightType::Env) {
    setTexture(pTexture);
}

EnvironmentLight::SharedPtr EnvironmentLight::create(const std::string& name, Texture::SharedPtr pTexture) {
    return SharedPtr(new EnvironmentLight(name, pTexture));
}

void EnvironmentLight::updateFromAnimation(const glm::mat4& transform) {

}

float EnvironmentLight::getPower() const { 
    // TODO: calculate total power in prepass or use special ltx value
    return 0.f; 
}

// AnalyticAreaLight

AnalyticAreaLight::AnalyticAreaLight(const std::string& name, LightType type) : Light(name, type) {
    mData.tangent = float3(1, 0, 0);
    mData.bitangent = float3(0, 1, 0);
    mData.surfaceArea = 1.0f;
    mData.flags |= (uint32_t)LightDataFlags::Area; 

    mScaling = float3(0.5, 0.5, 0.5); // 0.5 is a "radius" of a unit sized light primitive
    update();
    mPrevData = mData;
}

void AnalyticAreaLight::setScaling(float3 scale) { 
    mScaling = scale * 0.5f; // We do this multiplication to make our area light of a unit size  
    update(); 
}

float AnalyticAreaLight::getPower() const {
    return luminance((float3)mData.diffuseIntensity) * (float)M_PI * mData.surfaceArea;
}

void AnalyticAreaLight::setSingleSided(bool value) { 
    if (value && !mData.isSingleSided()) {
        mData.flags |= (uint32_t)LightDataFlags::SingleSided; 
    }
    update();
}

void AnalyticAreaLight::update() {
    // Update matrix
    mData.transMat = mTransformMatrix * glm::scale(glm::mat4(), mScaling);
    mData.transMatIT = glm::inverse(glm::transpose(mData.transMat));
    mData.transMatInv = glm::inverse(mData.transMat);

    mData.posW = {mData.transMat[3][0], mData.transMat[3][1], mData.transMat[3][2]};
    Light::update();
}

// RectLight

RectLight::SharedPtr RectLight::create(const std::string& name) {
    return SharedPtr(new RectLight(name));
}

void RectLight::update() {
    AnalyticAreaLight::update();

    float rx = glm::length(mData.transMat * float4(1.0f, 0.0f, 0.0f, 0.0f));
    float ry = glm::length(mData.transMat * float4(0.0f, 1.0f, 0.0f, 0.0f));

    mData.surfaceArea = std::max(std::numeric_limits<float>::min(), 4.0f * rx * ry );
    Light::update();
}

// DiscLight

DiscLight::SharedPtr DiscLight::create(const std::string& name) {
    return SharedPtr(new DiscLight(name));
}

void DiscLight::update() {
    AnalyticAreaLight::update();

    float rx = glm::length(mData.transMat * float4(1.0f, 0.0f, 0.0f, 0.0f));
    float ry = glm::length(mData.transMat * float4(0.0f, 1.0f, 0.0f, 0.0f));
    mData.surfaceArea = std::max(std::numeric_limits<float>::min(), (float)M_PI * rx * ry);
    Light::update();
}

// SphereLight

SphereLight::SharedPtr SphereLight::create(const std::string& name) {
    return SharedPtr(new SphereLight(name));
}

void SphereLight::update() {
    AnalyticAreaLight::update();

    float rx = glm::length(mData.transMat * float4(1.0f, 0.0f, 0.0f, 0.0f));
    float ry = glm::length(mData.transMat * float4(0.0f, 1.0f, 0.0f, 0.0f));
    float rz = glm::length(mData.transMat * float4(0.0f, 0.0f, 1.0f, 0.0f));
    mData.surfaceArea = std::max(
        std::numeric_limits<float>::min(), 
        4.0f * (float)M_PI * std::pow((std::pow(rx * ry, 1.6075f) + std::pow(ry * rz, 1.6075f) + std::pow(rx * rz, 1.6075f)) / 3.0f, (1.0f / 1.6075f)));

    Light::update();
}


#ifdef SCRIPTING
SCRIPT_BINDING(Light) {
    pybind11::class_<Light, Animatable, Light::SharedPtr> light(m, "Light");
    light.def_property_readonly("name", &Light::getName);
    light.def_property("active", &Light::isActive, &Light::setActive);
    light.def_property("animated", &Light::isAnimated, &Light::setIsAnimated);
    //light.def_property("intensity", &Light::getIntensityForScript, &Light::setIntensityFromScript);
    light.def_property("color", &Light::getColorForScript, &Light::setColorFromScript);

    pybind11::class_<DirectionalLight, Light, DirectionalLight::SharedPtr> directionalLight(m, "DirectionalLight");
    directionalLight.def_property("direction", &DirectionalLight::getWorldDirection, &DirectionalLight::setWorldDirection);

    pybind11::class_<DistantLight, Light, DistantLight::SharedPtr> distantLight(m, "DistantLight");
    distantLight.def_property("direction", &DistantLight::getWorldDirection, &DistantLight::setWorldDirection);
    distantLight.def_property("angle", &DistantLight::getAngle, &DistantLight::setAngle);

    pybind11::class_<PointLight, Light, PointLight::SharedPtr> pointLight(m, "PointLight");
    pointLight.def_property("position", &PointLight::getWorldPosition, &PointLight::setWorldPosition);
    pointLight.def_property("direction", &PointLight::getWorldDirection, &PointLight::setWorldDirection);
    pointLight.def_property("openingAngle", &PointLight::getOpeningAngle, &PointLight::setOpeningAngle);
    pointLight.def_property("penumbraAngle", &PointLight::getPenumbraAngle, &PointLight::setPenumbraAngle);

    pybind11::class_<AnalyticAreaLight, Light, AnalyticAreaLight::SharedPtr> analyticLight(m, "AnalyticAreaLight");
}
#endif

}  // namespace Falcor
