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
#include "stdafx.h"

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Utils/Timing/Profiler.h"
#include "RenderGraphExe.h"

namespace Falcor {

    void RenderGraphExe::execute(const Context& ctx, uint32_t frameNumber, uint32_t sampleNumber) {
        auto pDevice = ctx.pRenderContext->device();
        PROFILE(pDevice, "RenderGraphExe::execute()");

        for (const auto& pass : mExecutionList) {
            PROFILE(pDevice, pass.name);
            
            RenderData renderData(pass.name, mpResourceCache, ctx.pGraphDictionary, ctx.defaultTexDims, ctx.defaultTexFormat, frameNumber, sampleNumber);
            pass.pPass->execute(ctx.pRenderContext, renderData);
        }
    }

    bool RenderGraphExe::beginFrame(const Context& ctx, uint32_t frameNumber) {
        auto pDevice = ctx.pRenderContext->device();
        PROFILE(pDevice, "RenderGraphExe::beginFrame()");

        for (const auto& pass : mExecutionList) {
            PROFILE(pDevice, pass.name);
            
            RenderData renderData(pass.name, mpResourceCache, ctx.pGraphDictionary, ctx.defaultTexDims, ctx.defaultTexFormat, frameNumber, 0);
            if(!pass.pPass->beginFrame(ctx.pRenderContext, renderData)) {
                LLOG_ERR << "Error beginning frame for pass " << pass.name;
                return false;
            }
        }

        return true;
    }

    void RenderGraphExe::endFrame(const Context& ctx, uint32_t frameNumber) {
        auto pDevice = ctx.pRenderContext->device();
        PROFILE(pDevice, "RenderGraphExe::endFrame()");

        for (const auto& pass : mExecutionList) {
            PROFILE(pDevice, pass.name);
            
            RenderData renderData(pass.name, mpResourceCache, ctx.pGraphDictionary, ctx.defaultTexDims, ctx.defaultTexFormat, frameNumber, 0);
            pass.pPass->endFrame(ctx.pRenderContext, renderData);
        }
    }

    void RenderGraphExe::resolvePerFrameSparseResources(const Context& ctx) {
        auto pDevice = ctx.pRenderContext->device();
        PROFILE(pDevice, "RenderGraphExe::resolvePerFrameSparseResources()");

        for (const auto& pass : mExecutionList) {
            PROFILE(pDevice, pass.name);

            RenderData renderData(pass.name, mpResourceCache, ctx.pGraphDictionary, ctx.defaultTexDims, ctx.defaultTexFormat);
            pass.pPass->resolvePerFrameSparseResources(ctx.pRenderContext, renderData);
        }
    }

    void RenderGraphExe::resolvePerSampleSparseResources(const Context& ctx) {
        auto pDevice = ctx.pRenderContext->device();
        PROFILE(pDevice, "RenderGraphExe::resolvePerSampleSparseResources()");

        for (const auto& pass : mExecutionList) {
            PROFILE(pDevice, pass.name);

            RenderData renderData(pass.name, mpResourceCache, ctx.pGraphDictionary, ctx.defaultTexDims, ctx.defaultTexFormat);
            pass.pPass->resolvePerSampleSparseResources(ctx.pRenderContext, renderData);
        }
    }

    void RenderGraphExe::onHotReload(HotReloadFlags reloaded) {
        for (const auto& p : mExecutionList) {
            const auto& pPass = p.pPass;
            pPass->onHotReload(reloaded);
        }
    }

    void RenderGraphExe::insertPass(const std::string& name, const RenderPass::SharedPtr& pPass) {
        mExecutionList.push_back(Pass(name, pPass));
    }

    Resource::SharedPtr RenderGraphExe::getResource(const std::string& name) const {
        assert(mpResourceCache);
        return mpResourceCache->getResource(name);
    }

    void RenderGraphExe::setInput(const std::string& name, const Resource::SharedPtr& pResource) {
        mpResourceCache->registerExternalResource(name, pResource);
    }
}
