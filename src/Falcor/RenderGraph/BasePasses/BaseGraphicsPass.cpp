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
#include "BaseGraphicsPass.h"

namespace Falcor {

BaseGraphicsPass::BaseGraphicsPass(std::shared_ptr<Device> pDevice, const Program::Desc& progDesc, const Program::DefineList& programDefines): mpDevice(pDevice) {
    auto pProg = GraphicsProgram::create(pDevice, progDesc, programDefines);

    mpState = GraphicsState::create(pDevice);
    mpState->setProgram(pProg);

    mpVars = GraphicsVars::create(pDevice, pProg.get());
}

void BaseGraphicsPass::addDefine(const std::string& name, const std::string& value, bool updateVars) {
    mpState->getProgram()->addDefine(name, value);
    if (updateVars) mpVars = GraphicsVars::create(mpDevice, mpState->getProgram().get());
}

void BaseGraphicsPass::removeDefine(const std::string& name, bool updateVars) {
    mpState->getProgram()->removeDefine(name);
    if (updateVars) mpVars = GraphicsVars::create(mpDevice, mpState->getProgram().get());
}

void BaseGraphicsPass::setVars(const GraphicsVars::SharedPtr& pVars) {
    mpVars = pVars ? pVars : GraphicsVars::create(mpDevice, mpState->getProgram().get());
}

}  // namespace Falcor
