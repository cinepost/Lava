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


namespace Falcor {


static const std::string kLightNamesDelims = "\t\n,| ";

LightLinker::LightSet::LightSet(const LightNamesList& lightNames) {
    for(const std::string& lightName: lightNames) if(!lightName.empty()) mLightNames.insert(lightName);
    mLightSetData.lightsCount = static_cast<uint32_t>(mLightNames.size());
}

LightLinker::LightSet::LightSet(const std::set<std::string>& lightNamesSet) {
    mLightNames = lightNamesSet;
    mLightSetData.lightsCount = static_cast<uint32_t>(mLightNames.size());
}

bool LightLinker::LightSet::hasLightName(const std::string& name) const {
    assert(!name.empty());
    if(name.empty()) return false;
    return mLightNames.find(name) != mLightNames.end();
}

LightLinker::SharedPtr LightLinker::create(std::shared_ptr<Device> pDevice, std::shared_ptr<Scene> pScene) {
    return SharedPtr(new LightLinker(pDevice, pScene));
}

LightLinker::LightLinker(std::shared_ptr<Device> pDevice, std::shared_ptr<Scene> pScene) {
    assert(pDevice);
    mpDevice = pDevice;
    mpScene = pScene ? pScene : nullptr;
}

bool LightLinker::update(RenderContext* pRenderContext, UpdateStatus* pUpdateStatus) {
    PROFILE(mpDevice, "LightLinker::update()");

    auto pScene = mpScene.lock();
    if(pScene) {
        
    }

    return false;
}

uint32_t LightLinker::addLight(const Light::SharedPtr& pLight) {
    assert(pLight);
    mLightsMap[pLight->getName()] = pLight;
    assert(mLightsMap.size() <= std::numeric_limits<uint32_t>::max());
    mLightsChanged = true;
    return mLightsMap.size() - 1;
}

void LightLinker::setShaderData(const ShaderVar& var) const {
    assert(var.isValid());

    buildBuffers();

    // Set variables.
    var["lightSetsCount"] = mLightSets.size();
    
    // Bind buffers.
    var["lights"] = mpLightsDataBuffer;
    var["lightSets"] = mpLightSetsDataBuffer;
}

void LightLinker::buildBuffers() const {
    if(mLightsMap.empty()) return;

    const bool rebuildIndirectionBuffer = mLightsChanged || mLightSetsChanged;

    // Lights data buffer
    if(!mpLightsDataBuffer || mLightsChanged) {

        std::vector<LightData> lightsData(mLightsMap.size());

        size_t light_idx = 0;
        for(auto const& [name, pLight] : mLightsMap) {
            lightsData[light_idx++] = pLight->getData();
        }

        mpLightsDataBuffer = Buffer::createStructured(mpDevice, sizeof(LightData), (uint32_t)lightsData.size(), ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, lightsData.data(), false);
        mpLightsDataBuffer->setName("LightLinker::mpLightsDataBuffer");
        mLightsChanged = false;
    }

    // Light sets buffer
    if(!mpLightSetsDataBuffer || mLightSetsChanged) {

        std::vector<LightSetData> lightSetsData(mLightSets.size());
        size_t light_set_idx = 0;
        for(auto const& lightSet : mLightSets) {
            lightSetsData[light_set_idx++] = lightSet.getData();
        }

        mpLightSetsDataBuffer = Buffer::createStructured(mpDevice, sizeof(LightSetData), (uint32_t)lightSetsData.size(), ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, lightSetsData.data(), false);
        mpLightSetsDataBuffer->setName("LightLinker::mpLightSetsDataBuffer");
        mLightSetsChanged = false;
    }
}

uint32_t LightLinker::getOrCreateLightSetIndex(const std::string& lightNamesString) {
    LightNamesList lightNames;
    if(!lightNamesString.empty()) {
        boost::split(lightNames, lightNamesString, boost::is_any_of(kLightNamesDelims));
    }

    return getOrCreateLightSetIndex(lightNames);
}

uint32_t LightLinker::getOrCreateLightSetIndex(const LightNamesList& lightNames) {
    uint32_t index = 0;
    if(lightNames.empty()) return index;
    if(!findLightSetIndex(lightNames, index)) {
        mLightSetsChanged = true;
        mLightSets.push_back(LightSet(lightNames));
    }
    return index;
}

bool LightLinker::findLightSetIndex(const LightNamesList& lightNames, uint32_t& index) const {
    index = 0;

    for(const auto& lightSet: mLightSets) {
        std::set<std::string> lightNameSet;
        for(const std::string& lightName: lightNames) if(!lightName.empty()) lightNameSet.insert(lightName);
        if(lightSet.lightNames() == lightNameSet) return true;
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

} // namespace Falcor