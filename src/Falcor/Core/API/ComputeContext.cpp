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
#include "ComputeContext.h"
#include "Falcor/Utils/Debug/debug.h"


namespace Falcor {

ComputeContext::SharedPtr ComputeContext::create(std::shared_ptr<Device> pDevice, CommandQueueHandle queue) {
    auto pCtx = SharedPtr(new ComputeContext(pDevice, LowLevelContextData::CommandQueueType::Compute, queue));
    pCtx->bindDescriptorHeaps(); // TODO: Should this be done here?
    return pCtx;
}

#if defined(FALCOR_VK)
bool ComputeContext::applyComputeVars(ComputeVars* pVars, RootSignature* pRootSignature) {
    bool varsChanged = (pVars != mpLastBoundComputeVars);

    // FIXME TODO Temporary workaround
    varsChanged = true;

    if (pVars->apply(this, varsChanged, pRootSignature) == false) {
        LLOG_WRN << "ComputeContext::applyComputeVars() - applying ComputeVars failed, most likely because we ran out of descriptors. Flushing the GPU and retrying";
        flush(true);

        if (!pVars->apply(this, varsChanged, pRootSignature)) {
            LLOG_ERR << "ComputeVars::applyComputeVars() - applying ComputeVars failed, most likely because we ran out of descriptors";
            return false;
        }
    }
    return true;
}
#endif

void ComputeContext::flush(bool wait) {
    CopyContext::flush(wait);
    mpLastBoundComputeVars = nullptr;
}

}  // namespace Falcor
