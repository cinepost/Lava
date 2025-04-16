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
#include "VBufferDBG.h"

#include "Scene/HitInfo.h"

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/IndirectCommands.h"

#include "Falcor/RenderGraph/RenderPassStandardFlags.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/Scene/Material/BasicMaterial.h"
#include "Falcor/Scene/SceneDefines.slangh"
#include "Falcor/Utils/Timing/SimpleProfiler.h"


#include <limits>

const RenderPass::Info VBufferDBG::kInfo { "VBufferDBG", "Debug V-buffer generation pass." };

namespace {
    const std::string kProgramComputeFile = "RenderPasses/GBuffer/VBuffer/VBufferDBG.cs.slang";

    // Scripting options.


    // Ray tracing settings that affect the traversal stack size. Set as small as possible.
    const uint32_t kMaxPayloadSizeBytes = 4; // TODO: The shader doesn't need a payload, set this to zero if it's possible to pass a null payload to TraceRay()
    const uint32_t kMaxRecursionDepth = 1;

    const std::string kVBufferName = "vbuffer";
    const std::string kVBufferDesc = "V-buffer in packed format (indices + barycentrics)";

    const ChannelList kVBufferExtraChannels = {
        { "depth",          "gDepth",           "Depth buffer (NDC)",               true /* optional */, ResourceFormat::R32Float    },
    };
};

VBufferDBG::SharedPtr VBufferDBG::create(RenderContext* pRenderContext, const Dictionary& dict) {
    return SharedPtr(new VBufferDBG(pRenderContext->device(), dict));
}

VBufferDBG::VBufferDBG(Device::SharedPtr pDevice, const Dictionary& dict): GBufferBase(pDevice, kInfo), mDirty(true) {
    if (!mpDevice->isShaderModelSupported(ShaderModel::SM6_6)) {
        FALCOR_THROW("VBufferDBG: requires Shader Model 6.6 support.");
    }

    mDirty = true;
}

RenderPassReflection VBufferDBG::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    // Add the required output. This always exists.
    reflector.addOutput(kVBufferName, kVBufferDesc).bindFlags(Resource::BindFlags::UnorderedAccess).format(mVBufferFormat);
    // Add extra outputs.
    addRenderPassOutputs(reflector, kVBufferExtraChannels, ResourceBindFlags::UnorderedAccess);
    return reflector;
}

void VBufferDBG::compile(RenderContext* pRenderContext, const CompileData& compileData) {
    GBufferBase::compile(pRenderContext, compileData);
}

void VBufferDBG::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    LLOG_WRN << "VBufferDBG::execute";
    
    // Create compute pass.
    if (!mpComputePass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kProgramComputeFile).csEntry("main");
        
        Program::DefineList defines;
        mpComputePass = ComputePass::create(mpDevice, desc, defines, true);
    }
    
    auto pOutputTex = renderData[kVBufferName]->asTexture();
    mpComputePass->execute(pRenderContext, pOutputTex->getWidth(), pOutputTex->getHeight());
}
