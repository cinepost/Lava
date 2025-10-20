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
#ifndef SRC_FALCOR_CORE_API_COMPUTESTATEOBJECT_H_
#define SRC_FALCOR_CORE_API_COMPUTESTATEOBJECT_H_

#include "Falcor/Core/Object.h"
#include "Falcor/Core/Program/ProgramVersion.h"
#include "GFXAPI.h"

namespace Falcor {

class Device;

struct ComputeStateObjectDesc {
    Falcor::SharedPtr<const ProgramKernels> pProgramKernels;

    bool operator==(const ComputeStateObjectDesc& other) const {
        return pProgramKernels == other.pProgramKernels;
    }
};

class FALCOR_API ComputeStateObject : public Object {
    FALCOR_OBJECT(ComputeStateObject)
 public:
    ComputeStateObject(Falcor::SharedPtr<Device> pDevice, ComputeStateObjectDesc desc);
    ~ComputeStateObject();

    gfx::IPipelineState* getGfxPipelineState() const { return mGfxPipelineState; }

    VkPipeline getNativeHandle() const;
    
    const ComputeStateObjectDesc& getDesc() const { return mDesc; }

  
  private:
    Falcor::SharedPtr<Device> mpDevice;
    ComputeStateObjectDesc mDesc;
    Slang::ComPtr<gfx::IPipelineState> mGfxPipelineState;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_COMPUTESTATEOBJECT_H_
