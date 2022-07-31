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
#include <vector>

#include "Falcor/stdafx.h"

#include "Falcor/Core/API/ComputeStateObject.h"
#include "VKState.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Utils/Debug/debug.h"

namespace Falcor {

void ComputeStateObject::apiInit() {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfos;
    //initVkShaderStageInfo(mDesc.getProgramVersion().get(), shaderStageInfos);
    initVkShaderStageInfo(mDesc.getProgramKernels(), shaderStageInfos);
    assert(shaderStageInfos.size() == 1);

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.pNext = nullptr;
    info.flags = 0;
    info.basePipelineHandle = VK_NULL_HANDLE;
    info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.stage = shaderStageInfos[0];
    info.layout = mDesc.mpRootSignature->getApiHandle();


    VkPipeline pipeline;
    if (VK_FAILED(vkCreateComputePipelines(mpDevice->getApiHandle(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline))) {
        throw std::runtime_error("Could not create compute pipeline.");
    }
    mApiHandle = ApiHandle::create(mpDevice, pipeline);
}

}  // namespace Falcor
