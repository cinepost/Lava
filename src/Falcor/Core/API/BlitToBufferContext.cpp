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
#include "BlitToBufferContext.h"

namespace Falcor {

void BlitToBufferContext::init(Device::SharedPtr pDevice) {
    assert(pDevice);
    if (pPass == nullptr) {
        // Init the blit data.
        Program::DefineList defines = {
            { "PIXEL_STRIDE_BYTES", "1"},
            { "FORMAT_CHANNELS", "1" }, 
            { "DST_HALF_FLOAT", "0" }, 
            { "FORMAT_TYPE", "0" },
            { "SAMPLE_COUNT", "1" },
            { "COMPLEX_BLIT", "0" },
            { "SRC_INT", "0" },
            { "DST_INT", "0" },
        };
        Program::Desc d;
        d.addShaderLibrary("Core/API/BlitToBufferReduction.cs.slang").csEntry("main");
        pPass = ComputePass::create(pDevice, "Core/API/BlitToBufferReduction.cs.slang","main", defines);
        assert(pPass);

        pBlitParamsBuffer = pPass->getVars()->getParameterBlock("BlitParamsCB");
        resolutionVarOffset = pBlitParamsBuffer->getVariableOffset("gResolution");
        offsetVarOffset = pBlitParamsBuffer->getVariableOffset("gOffset");
        scaleVarOffset = pBlitParamsBuffer->getVariableOffset("gScale");
        srcPixelHalfSizeVarOffset = pBlitParamsBuffer->getVariableOffset("gSrcPixelHalfSize");
        prevSrcRectOffset = float2(-1.0f);
        prevSrcReftScale = float2(-1.0f);

        Sampler::Desc desc;
        desc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
        desc.setReductionMode(Sampler::ReductionMode::Standard);
        desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
        pLinearSampler = Sampler::create(pDevice, desc);
        desc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
        pPointSampler = Sampler::create(pDevice, desc);
        
        // Min reductions.
        desc.setReductionMode(Sampler::ReductionMode::Min);
        desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
        pLinearMinSampler = Sampler::create(pDevice, desc);
        desc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
        pPointMinSampler = Sampler::create(pDevice, desc);
        
        // Max reductions.
        desc.setReductionMode(Sampler::ReductionMode::Max);
        desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
        pLinearMaxSampler = Sampler::create(pDevice, desc);
        desc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
        pPointMaxSampler = Sampler::create(pDevice, desc);

        const auto& pDefaultBlockReflection = pPass->getProgram()->getReflector()->getDefaultParameterBlock();
        texBindLoc = pDefaultBlockReflection->getResourceBinding("gTex");
        buffBindLoc = pDefaultBlockReflection->getResourceBinding("gOutputBuffer");

        // Complex blit parameters

        compTransVarOffset[0] = pBlitParamsBuffer->getVariableOffset("gCompTransformR");
        compTransVarOffset[1] = pBlitParamsBuffer->getVariableOffset("gCompTransformG");
        compTransVarOffset[2] = pBlitParamsBuffer->getVariableOffset("gCompTransformB");
        compTransVarOffset[3] = pBlitParamsBuffer->getVariableOffset("gCompTransformA");
        prevComponentsTransform[0] = float4(1.0f, 0.0f, 0.0f, 0.0f);
        prevComponentsTransform[1] = float4(0.0f, 1.0f, 0.0f, 0.0f);
        prevComponentsTransform[2] = float4(0.0f, 0.0f, 1.0f, 0.0f);
        prevComponentsTransform[3] = float4(0.0f, 0.0f, 0.0f, 1.0f);
        for (uint32_t i = 0; i < 4; i++) pBlitParamsBuffer->setVariable(compTransVarOffset[i], prevComponentsTransform[i]);
    }
}

void BlitToBufferContext::release() {
    *this = {};
}

}  // namespace Falcor
