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
#pragma once

#include "Core/Framework.h"
#include "Core/API/CopyContext.h"
#include "Core/API/RootSignature.h"

namespace Falcor {

class dlldecl RtVarsContext : public CopyContext {
  public:
    using SharedPtr = std::shared_ptr<RtVarsContext>;
    ~RtVarsContext();

    /** Create a new ray tracing vars context object.
        \return A new object, or throws an exception if creation failed.
    */
    static SharedPtr create(std::shared_ptr<Device> pDevice, CommandQueueHandle queue = nullptr);

    const LowLevelContextData::SharedPtr& getLowLevelData() const override { return mpLowLevelData; }
    bool resourceBarrier(const Resource* pResource, Resource::State newState, const ResourceViewInfo* pViewInfo = nullptr) override;
    //RtVarsCmdList::SharedPtr getRtVarsCmdList() const { return mpList; }

    void uavBarrier(const Resource* pResource) override;

  private:
    RtVarsContext(std::shared_ptr<Device> pDevice, CommandQueueHandle queue);
    //RtVarsCmdList::SharedPtr mpList;
};

}  // namespace Falcor
