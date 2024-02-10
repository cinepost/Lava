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

#include <boost/algorithm/string.hpp>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Utils/Timing/Profiler.h"
#include "Falcor/Utils/Timing/TimeReport.h"

#include "Falcor/Core/API/RenderContext.h"

#include "LightCollectionShared.slang"
#include "Scene/Scene.h"

#include <sstream>

#include "LightLinker.h"


namespace {
    static const std::string    kLightNamesDelims       = "\t\n,| ";
}


namespace Falcor {

LightLinker::NameSet::NameSet() {
    mUseCounter = 0;
}

LightLinker::NameSet::NameSet(const StringList& names): NameSet() {
    for(const std::string& name: names) if(!name.empty()) mNames.insert(name);
}

LightLinker::NameSet::NameSet(const StringSet& names): NameSet() {
    mNames = names;
}

LightLinker::LightSet::LightSet(): NameSet() {
    mLightSetData.lightsCount = 0;
    mLightSetData.lightsOffset = 0;
}

LightLinker::LightSet::LightSet(const StringList& names): NameSet(names) {
    mLightSetData.lightsCount = static_cast<uint32_t>(mNames.size());
}

LightLinker::LightSet::LightSet(const StringSet& names): NameSet(names) {
    mLightSetData.lightsCount = static_cast<uint32_t>(mNames.size());
}

bool LightLinker::NameSet::hasName(const std::string& name) const {
    assert(!name.empty());
    if(name.empty()) return false;
    return mNames.find(name) != mNames.end();
}

LightLinker::SharedPtr LightLinker::create(std::shared_ptr<Device> pDevice, std::shared_ptr<Scene> pScene) {
    return SharedPtr(new LightLinker(pDevice, pScene));
}

LightLinker::LightLinker(std::shared_ptr<Device> pDevice, std::shared_ptr<Scene> pScene) {
    assert(pDevice);
    mpDevice = pDevice;
    mpScene = pScene ? pScene : nullptr;

    // Global light set (includes all lights)
    LightSet global;
    global.mLightSetData.lightsCount = 0;
    global.mLightSetData.lightsOffset = 0;
    global.mUseCounter = std::numeric_limits<size_t>::max();

    mLightSets.push_back(global);
    
}

LightLinker::UpdateFlags LightLinker::update(bool forceUpdate) {
    PROFILE(mpDevice, "LightLinker::update()");

    UpdateFlags flags = UpdateFlags::None;

    auto pScene = mpScene.lock();
    if(pScene) {
        
    }

    const bool lightDataChanged = buildActiveLightsData(forceUpdate || mLightsChanged); 
    if(lightDataChanged) flags |= UpdateFlags::LightsChanged;

    const bool indirectionDataChanged = buildLightsIndirectionData(forceUpdate || lightDataChanged || mLightSetsChanged);

    if(buildLightSetsData(forceUpdate || indirectionDataChanged)) flags |= UpdateFlags::LightSetsChanged;
    return flags;
}

uint32_t LightLinker::addLight(const Light::SharedPtr& pLight) {
    assert(pLight);

    const auto lightName = pLight->getName();
    const auto it = mLightsMap.find(lightName);

    if(it == mLightsMap.end()) {
        mLightsMap[lightName] = pLight;
        mLightsChanged = true;
    } else {
        return std::distance(mLightsMap.begin(), it);
    }

    assert(mLightsMap.size() <= std::numeric_limits<uint32_t>::max());    
    return static_cast<uint32_t>(mLightsMap.size() - 1);
}

void LightLinker::updateLight(const Light::SharedPtr& pLight) {

    const auto lightName = pLight->getName();
    if(mLightsMap.find(lightName) == mLightsMap.end()) return;
    mLightsChanged = true;
}

void LightLinker::setLightsActive(bool state) {
    for(auto& entry: mLightsMap) {
        if(entry.second->isActive() != state) {
            entry.second->setActive(state);
            mLightsChanged = true;
        }
    }
}

void LightLinker::deleteLight(const std::string& name) {
    auto it = mLightsMap.find(name);
    if(it != mLightsMap.end()) {
        mLightsMap.erase(it);
        mLightsChanged = true;
    }
}

void LightLinker::setLightActive(const std::string& name, bool state) {
    auto it = mLightsMap.find(name);
    if(it != mLightsMap.end()) {
        if(it->second->isActive() == state) {
            return;
        }
        it->second->setActive(state);
        mLightsChanged = true;
    }
}

void LightLinker::setShaderData(const ShaderVar& var) const {
    assert(var.isValid());

    // Set variables.
    var["globalLightsCount"] = mActiveLightsData.size();

    LLOG_DBG << "LightLinker global lights count " << mActiveLightsData.size();

    var["lightSetsCount"] = mLightSets.size();

    LLOG_DBG << "LightLinker lightSetsCount " << mLightSets.size();

    for(uint32_t idx = 0; idx < mLightSets.size(); ++idx) {
        LLOG_DBG << "LightLinker lightSets[" << idx << "] lightsCount " << mLightSets[idx].getData().lightsCount;
    }

    // Bind buffers.
    var["lights"] = mpLightsDataBuffer;
    var["lightSets"] = mpLightSetsDataBuffer;
    var["indirectionTable"] = mpIndirectionTableBuffer;
}

bool LightLinker::buildActiveLightsData(bool force) {
    if(!force && (mLightsMap.empty() || !mLightsChanged)) return false;

    bool rebuildGPUBuffer = mActiveLightsData.size() != mLightsMap.size();

    mActiveLightIDsMap.clear();
    mActiveLightsData.clear();
    mActiveLightsData.reserve(mLightsMap.size());

    uint32_t activeLightID = 0;
    for(auto const& [name, pLight] : mLightsMap) {
        if(!pLight->isActive()) {
            rebuildGPUBuffer = true;
            continue;
        }
        
        mActiveLightIDsMap[name] = activeLightID++; 
        mActiveLightsData.push_back(pLight->getData());
        if(pLight->getChanges() != Light::Changes::None) {
                rebuildGPUBuffer = true;
        }
    }

    mLightsChanged = false;

    if(!rebuildGPUBuffer) return false;
    
    if(!mActiveLightsData.empty()) {
        mpLightsDataBuffer = Buffer::createStructured(mpDevice, sizeof(LightData), (uint32_t)mActiveLightsData.size(), ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, mActiveLightsData.data(), false);
        mpLightsDataBuffer->setName("LightLinker::mpLightsDataBuffer");
    }

    return true;
}

bool LightLinker::buildLightsIndirectionData(bool force) {
    if(!force || mActiveLightIDsMap.empty()) return false;

    bool rebuildIndirectionGPUBuffer = false;
    const std::vector<uint32_t> prevIndirectionData = mIndirectionData;

    mIndirectionData.clear();
    mIndirectionData.reserve(1024);

    // Indirection table with light sets data update
    for(size_t i = 1; i < mLightSets.size(); ++i) {

        LightSet& lightSet = mLightSets[i];
        //if(!lightSet.isInUse()) continue;

        lightSet.mLightSetData.lightsOffset = static_cast<uint32_t>(mIndirectionData.size());
        lightSet.mLightSetData.lightsCount = 0;
        
        for(const std::string& lightName: lightSet.names()) {
            const auto it = mActiveLightIDsMap.find(lightName);
            if(it != mActiveLightIDsMap.end()) {
                lightSet.mLightSetData.lightsCount++;
                mIndirectionData.push_back(it->second);
            }
        }
    }

    // Indirection data has no changes
    if(prevIndirectionData == mIndirectionData) return false;

    mLightSetsChanged = true;

    if(!mIndirectionData.empty()) {
        mpIndirectionTableBuffer = Buffer::createStructured(mpDevice, sizeof(uint32_t), (uint32_t)mIndirectionData.size(), ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, mIndirectionData.data(), false);
        mpIndirectionTableBuffer->setName("LightLinker::mpIndirectionTableBuffer");
    }

    return true;
}

bool LightLinker::buildLightSetsData(bool force) {
    if(!force && (mLightSets.empty() || !mLightSetsChanged)) return false;

    bool rebuildGPUBuffer = mLightSetsData.size() != mLightSets.size();

    mLightSetsData.resize(mLightSets.size());

    mLightSets[0].mLightSetData.lightsCount = static_cast<uint32_t>(mActiveLightsData.size());

    for(size_t light_set_idx = 0; light_set_idx < mLightSets.size(); ++light_set_idx) {
        auto const& lightSet = mLightSets[light_set_idx];
        mLightSetsData[light_set_idx] = lightSet.getData();
    }

    mLightSetsChanged = false;

    if(!rebuildGPUBuffer) return false;

    if(!mLightSetsData.empty()) {
        mpLightSetsDataBuffer = Buffer::createStructured(mpDevice, sizeof(LightSetData), (uint32_t)mLightSetsData.size(), ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, mLightSetsData.data(), false);
        mpLightSetsDataBuffer->setName("LightLinker::mpLightSetsDataBuffer");
    }

    return true;
}

LightLinker::StringList LightLinker::lightNamesStringToList(const std::string& lightNamesString) {
    StringList lightNamesList;

    if(!lightNamesString.empty()) {
        boost::split(lightNamesList, lightNamesString, boost::is_any_of(kLightNamesDelims));
    }
    return lightNamesList;
}

uint32_t LightLinker::getOrCreateLightSetIndex(const std::string& lightNamesString) {
    StringList lightNamesList;

    if(!lightNamesString.empty()) {
        lightNamesList = lightNamesStringToList(lightNamesString);
        return getOrCreateLightSetIndex(lightNamesList);
    }
    return 0;
}

uint32_t LightLinker::getOrCreateLightSetIndex(const StringList& lightNamesList) {
    uint32_t index = 0;
    if(lightNamesList.empty()) return index;
    if(!findLightSetIndex(lightNamesList, index)) {
        mLightSetsChanged = true;
        mLightSets.push_back(LightSet(lightNamesList));
    }
    return index;
}

bool LightLinker::findLightSetIndex(const StringList& lightNamesList, uint32_t& index) const {
    index = 0;

    for(const auto& lightSet: mLightSets) {
        std::set<std::string> lightNameSet;
        for(const std::string& lightName: lightNamesList) if(!lightName.empty() || (lightNamesList.size() == 1)) lightNameSet.insert(lightName);
        if(lightSet.names() == lightNameSet) return true;
        index++;
    }
    return false;
}

void LightLinker::copyDataToStagingBuffer(RenderContext* pRenderContext) const {
    if (mStagingBufferValid) return;

    mStagingBufferValid = true;
}

void LightLinker::syncCPUData() const {
    if (mCPUInvalidData == CPUOutOfDateFlags::None) return;

    mCPUInvalidData = CPUOutOfDateFlags::None;
}

uint64_t LightLinker::getMemoryUsageInBytes() const {
    uint64_t m = 0;
    return m;
}

Shader::DefineList LightLinker::getDefaultDefines() {
    Shader::DefineList defines;
    defines.add("SCENE_HAS_LIGHT_LINKER", "0");

    return defines;
}

Shader::DefineList LightLinker::getDefines() const {
    Shader::DefineList defines;
    defines.add("SCENE_HAS_LIGHT_LINKER", "1");

    return defines;
}

} // namespace Falcor