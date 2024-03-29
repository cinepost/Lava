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
#include "Falcor/stdafx.h"
#include "RenderPass.h"
#include "Falcor/Scene/Scene.h"

namespace Falcor {

RenderData::RenderData(const std::string& passName, const ResourceCache::SharedPtr& pResourceCache, const InternalDictionary::SharedPtr& pDict, const uint2& defaultTexDims, ResourceFormat defaultTexFormat
	,uint32_t frameNumber, uint32_t sampleNumber)
    : mName(passName)
    , mpResources(pResourceCache)
    , mpDictionary(pDict)
    , mDefaultTexDims(defaultTexDims)
    , mDefaultTexFormat(defaultTexFormat)
    , mFrameNumber(frameNumber)
    , mSampleNumber(sampleNumber)
{
    if (!mpDictionary) mpDictionary = InternalDictionary::create();
}

const Resource::SharedPtr& RenderData::getResource(const std::string& name) const {
    return mpResources->getResource(mName + '.' + name);
}

const Texture::SharedPtr& RenderData::getTexture(const std::string& name) const {
    const auto pResource = mpResources->getResource(mName + '.' + name);
    if (!pResource) return mpNullTexture;
    return pResource->asTexture();
}

const Buffer::SharedPtr& RenderData::getBuffer(const std::string& name) const {
    const auto pResource = mpResources->getResource(mName + '.' + name);
    if (!pResource) return mpNullBuffer;
    return pResource->asBuffer();
}

RenderPass::RenderPass(Device::SharedPtr pDevice, const Info& info): mpDevice(pDevice), mInfo(info) {
    assert(pDevice);
}

}  // namespace Falcor
