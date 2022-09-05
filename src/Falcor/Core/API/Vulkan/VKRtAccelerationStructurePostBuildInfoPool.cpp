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

#include "Falcor/Core/API/RtAccelerationStructure.h"
#include "Falcor/Core/API/RtAccelerationStructurePostBuildInfoPool.h"
#include "Falcor/Core/API/Vulkan/VKRtAccelerationStructure.h"
#include "Falcor/Core/ErrorHandling.h"

#define _FALCOR_GFX_CALL(a) {auto hr_ = a; if(SLANG_FAILED(hr_)) { reportError(#a); }}

namespace Falcor {

RtAccelerationStructurePostBuildInfoPool::RtAccelerationStructurePostBuildInfoPool(Device::SharedPtr pDevice, const Desc& desc): mpDevice(pDevice), mDesc(desc) {
    RtQueryPool::Desc queryPoolDesc = {};
    queryPoolDesc.count = desc.elementCount;
    queryPoolDesc.type = getVKAccelerationStructurePostBuildQueryType(desc.queryType);
    //FALCOR_GFX_CALL(mpDevice->getApiHandle()->createQueryPool(queryPoolDesc, mpQueryPool));
    mpRtQueryPool = RtQueryPool::create(pDevice, queryPoolDesc);
}

RtAccelerationStructurePostBuildInfoPool::~RtAccelerationStructurePostBuildInfoPool() {}

uint64_t RtAccelerationStructurePostBuildInfoPool::getElement(CopyContext* pContext, uint32_t index) {
    if (mNeedFlush) {
        pContext->flush(true);
        mNeedFlush = false;
    }
    uint64_t result = 0;
    _FALCOR_GFX_CALL(mpRtQueryPool->getResult(index, 1, &result));
    return result;
}

void RtAccelerationStructurePostBuildInfoPool::reset(CopyContext* pContext) {
    _FALCOR_GFX_CALL(mpRtQueryPool->reset());
    mNeedFlush = true;
}

}  // namespace Falcor
