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
#ifndef FALCOR_CORE_API_BLITTOBUFFERCONTEXT_H_
#define FALCOR_CORE_API_BLITTOBUFFERCONTEXT_H_

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Sampler.h"
#include "Falcor/Core/Program/ProgramVersion.h"

#include "RenderGraph/BasePasses/ComputePass.h"

namespace Falcor {

struct BlitToBufferContext {
    std::shared_ptr<ComputePass> pPass;

    Sampler::SharedPtr pLinearSampler;
    Sampler::SharedPtr pPointSampler;
    Sampler::SharedPtr pLinearMinSampler;
    Sampler::SharedPtr pPointMinSampler;
    Sampler::SharedPtr pLinearMaxSampler;
    Sampler::SharedPtr pPointMaxSampler;

    ParameterBlock::SharedPtr pBlitParamsBuffer;
    float2 prevSrcRectOffset = float2(0, 0);
    float2 prevSrcReftScale = float2(0, 0);

    // Variable offsets in constant buffer
    UniformShaderVarOffset resolutionVarOffset;
    UniformShaderVarOffset offsetVarOffset;
    UniformShaderVarOffset scaleVarOffset;
    ProgramReflection::BindLocation texBindLoc;
    ProgramReflection::BindLocation buffBindLoc;

    // Parameters for complex blit
    float4 prevComponentsTransform[4] = { float4(0), float4(0), float4(0), float4(0) };
    UniformShaderVarOffset compTransVarOffset[4];
    void init(Device::SharedPtr pDevice);
    void release();
};

}  // namespace Falcor

#endif  // FALCOR_CORE_API_BLITTOBUFFERCONTEXT_H_