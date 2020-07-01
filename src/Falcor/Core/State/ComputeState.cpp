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
#include "ComputeState.h"
#include "Falcor/Core/Program/ProgramVars.h"
#include "Falcor/Utils/Debug/debug.h"

namespace Falcor {

ComputeState::ComputeState() {
    mpCsoGraph = _StateGraph::create();
}

ComputeStateObject::SharedPtr ComputeState::getCSO(const ComputeVars* pVars) {
    LOG_WARN("1");
    auto pProgramKernels = mpProgram ? mpProgram->getActiveVersion()->getKernels(pVars) : nullptr;
    bool newProgram = (pProgramKernels.get() != mCachedData.pProgramKernels);
    
     LOG_WARN("2");
    if (newProgram) {
        LOG_WARN("2.1");
        mCachedData.pProgramKernels = pProgramKernels.get();
        mpCsoGraph->walk((void*)mCachedData.pProgramKernels);
    }

    LOG_WARN("3");
    if(!pProgramKernels) {
        LOG_WARN("get RootSignature::getEmpty");
    } else {
        LOG_WARN("pProgramKernels->getRootSignature");
    }
    RootSignature::SharedPtr pRoot = pProgramKernels ? pProgramKernels->getRootSignature() : RootSignature::getEmpty();

    if(!pRoot.get()) {
        LOG_FTL("no pRoot.get()!!!");
    }

    LOG_WARN("4");
    if (mCachedData.pRootSig != pRoot.get()) {
        LOG_WARN("4.1");
        mCachedData.pRootSig = pRoot.get();
        mpCsoGraph->walk((void*)mCachedData.pRootSig);
    }

    LOG_WARN("5");
    ComputeStateObject::SharedPtr pCso = mpCsoGraph->getCurrentNode();

    LOG_WARN("6");
    if(pCso == nullptr) {
        LOG_WARN("6.1");
        mDesc.setProgramKernels(pProgramKernels);
        mDesc.setRootSignature(pRoot);

        _StateGraph::CompareFunc cmpFunc = [&desc = mDesc](ComputeStateObject::SharedPtr pCso) -> bool {
            return pCso && (desc == pCso->getDesc());
        };

        LOG_WARN("6.2");
        if (mpCsoGraph->scanForMatchingNode(cmpFunc)) {
            LOG_WARN("6.2.1");
            pCso = mpCsoGraph->getCurrentNode();
        } else {
            LOG_WARN("6.2.2");
            pCso = ComputeStateObject::create(mDesc);
            if(!pCso) {
                LOG_ERR("pCso creation error!!!");
            }
            LOG_WARN("6.2.3");
            mpCsoGraph->setCurrentNodeData(pCso);
        }
    }

    LOG_WARN("return ");
    return pCso;
}

SCRIPT_BINDING(ComputeState) {
    m.regClass(ComputeState);
}

}  // namespace Falcor
