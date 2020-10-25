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
#include "GraphicsState.h"

namespace Falcor {

static GraphicsStateObject::PrimitiveType topology2Type(Vao::Topology t) {
    switch (t) {
        case Vao::Topology::PointList:
            return GraphicsStateObject::PrimitiveType::Point;
        case Vao::Topology::LineList:
        case Vao::Topology::LineStrip:
            return GraphicsStateObject::PrimitiveType::Line;
        case Vao::Topology::TriangleList:
        case Vao::Topology::TriangleStrip:
            return GraphicsStateObject::PrimitiveType::Triangle;
        default:
            should_not_get_here();
            return GraphicsStateObject::PrimitiveType::Undefined;
    }
}

GraphicsState::GraphicsState(std::shared_ptr<Device> device): mpDevice(device), mDesc(device) {
    uint32_t vpCount = getMaxViewportCount(device);

    // Create the viewports
    mViewports.resize(vpCount);
    mScissors.resize(vpCount);
    mVpStack.resize(vpCount);
    mScStack.resize(vpCount);
    for (uint32_t i = 0; i < vpCount; i++) {
        setViewport(i, mViewports[i], true);
    }

    mpGsoGraph = _StateGraph::create();
}

GraphicsState::~GraphicsState() = default;

GraphicsStateObject::SharedPtr GraphicsState::getGSO(const GraphicsVars* pVars) {
    LOG_DBG("getCSO 0");
    auto pProgramKernels = mpProgram ? mpProgram->getActiveVersion()->getKernels(pVars) : nullptr;
    LOG_DBG("getCSO 1");
    bool newProgVersion = pProgramKernels.get() != mCachedData.pProgramKernels;
    
    LOG_DBG("getCSO 2");    
    if (newProgVersion) {
        LOG_DBG("getCSO 2.2");
        mCachedData.pProgramKernels = pProgramKernels.get();
        LOG_DBG("getCSO 2.3");
        mpGsoGraph->walk((void*)pProgramKernels.get());
    }
    LOG_DBG("getCSO 3");
    RootSignature::SharedPtr pRoot = pProgramKernels ? pProgramKernels->getRootSignature() : RootSignature::getEmpty(mpDevice);

    LOG_DBG("getCSO 4");
    if (mCachedData.pRootSig != pRoot.get()) {
        LOG_DBG("getCSO 4.1");
        mCachedData.pRootSig = pRoot.get();
        LOG_DBG("getCSO 4.2");
        mpGsoGraph->walk((void*)mCachedData.pRootSig);
    }

    LOG_DBG("getCSO 5");
    const Fbo::Desc* pFboDesc = mpFbo ? &mpFbo->getDesc() : nullptr;
    LOG_DBG("getCSO 6");
    if(mCachedData.pFboDesc != pFboDesc) {
        LOG_DBG("getCSO 6.1");
        mpGsoGraph->walk((void*)pFboDesc);
        LOG_DBG("getCSO 6.2");
        mCachedData.pFboDesc = pFboDesc;
    }

    LOG_DBG("getCSO 7");
    GraphicsStateObject::SharedPtr pGso = mpGsoGraph->getCurrentNode();
    LOG_DBG("getCSO 8");
    if(pGso == nullptr) {
        LOG_DBG("getCSO 8.1");
        mDesc.setProgramKernels(pProgramKernels);
        LOG_DBG("getCSO 8.2");
        mDesc.setFboFormats(mpFbo ? mpFbo->getDesc() : Fbo::Desc(mpDevice));
#ifdef FALCOR_VK
        LOG_DBG("getCSO 8.3");
        mDesc.setRenderPass(mpFbo ? (VkRenderPass)mpFbo->getApiHandle() : VK_NULL_HANDLE);
#endif
        LOG_DBG("getCSO 8.4");
        mDesc.setVertexLayout(mpVao->getVertexLayout());
        LOG_DBG("getCSO 8.5");
        mDesc.setPrimitiveType(topology2Type(mpVao->getPrimitiveTopology()));
        LOG_DBG("getCSO 8.6");
        mDesc.setRootSignature(pRoot);

        LOG_DBG("getCSO 9");
        _StateGraph::CompareFunc cmpFunc = [&desc = mDesc](GraphicsStateObject::SharedPtr pGso) -> bool
        {
            LOG_DBG("getCSO 9.1");
            return pGso && (desc == pGso->getDesc());
        };

        LOG_DBG("getCSO 10");
        if (mpGsoGraph->scanForMatchingNode(cmpFunc)) {
            LOG_DBG("getCSO 10.1");
            pGso = mpGsoGraph->getCurrentNode();
        } else {
            LOG_DBG("getCSO 10.2");
            pGso = GraphicsStateObject::create(mpDevice, mDesc);
            LOG_DBG("getCSO 10.3");
            mpGsoGraph->setCurrentNodeData(pGso);
            LOG_DBG("getCSO 10.4");
        }
    }
    LOG_DBG("getCSO done!");
    return pGso;
}

GraphicsState& GraphicsState::setFbo(const Fbo::SharedPtr& pFbo, bool setVp0Sc0) {
    mpFbo = pFbo;

    if (setVp0Sc0 && pFbo) {
        uint32_t w = pFbo->getWidth();
        uint32_t h = pFbo->getHeight();
        GraphicsState::Viewport vp(0, 0, float(w), float(h), 0, 1);
        setViewport(0, vp, true);
    }
    return *this;
}

void GraphicsState::pushFbo(const Fbo::SharedPtr& pFbo, bool setVp0Sc0) {
    mFboStack.push(mpFbo);
    setFbo(pFbo, setVp0Sc0);
}

void GraphicsState::popFbo(bool setVp0Sc0) {
    if (mFboStack.empty()) {
        logError("PipelineState::popFbo() - can't pop FBO since the FBO stack is empty.");
        return;
    }
    setFbo(mFboStack.top(), setVp0Sc0);
    mFboStack.pop();
}

GraphicsState& GraphicsState::setVao(const Vao::SharedConstPtr& pVao) {
    if(mpVao != pVao) {
        mpVao = pVao;

#ifdef FALCOR_VK
        mDesc.setVao(pVao);
#endif

        mpGsoGraph->walk(pVao ? (void*)pVao->getVertexLayout().get() : nullptr);
    }
    return *this;
}

GraphicsState& GraphicsState::setBlendState(BlendState::SharedPtr pBlendState) {
    if(mDesc.getBlendState() != pBlendState) {
        mDesc.setBlendState(pBlendState);
        mpGsoGraph->walk((void*)pBlendState.get());
    }
    return *this;
}

GraphicsState& GraphicsState::setRasterizerState(RasterizerState::SharedPtr pRasterizerState) {
    if(mDesc.getRasterizerState() != pRasterizerState) {
        mDesc.setRasterizerState(pRasterizerState);
        mpGsoGraph->walk((void*)pRasterizerState.get());
    }
    return *this;
}

GraphicsState& GraphicsState::setSampleMask(uint32_t sampleMask) {
    if(mDesc.getSampleMask() != sampleMask) {
        mDesc.setSampleMask(sampleMask);
        mpGsoGraph->walk((void*)(uint64_t)sampleMask);
    }
    return *this;
}

GraphicsState& GraphicsState::setDepthStencilState(DepthStencilState::SharedPtr pDepthStencilState) {
    if(mDesc.getDepthStencilState() != pDepthStencilState) {
        mDesc.setDepthStencilState(pDepthStencilState);
        mpGsoGraph->walk((void*)pDepthStencilState.get());
    }
    return *this;
}

void GraphicsState::pushViewport(uint32_t index, const GraphicsState::Viewport& vp, bool setScissors) {
    mVpStack[index].push(mViewports[index]);
    setViewport(index, vp, setScissors);
}

void GraphicsState::popViewport(uint32_t index, bool setScissors) {
    if (mVpStack[index].empty()) {
        logError("PipelineState::popViewport() - can't pop viewport since the viewport stack is empty.");
        return;
    }
    const auto& VP = mVpStack[index].top();
    setViewport(index, VP, setScissors);
    mVpStack[index].pop();
}

void GraphicsState::pushScissors(uint32_t index, const GraphicsState::Scissor& sc) {
    mScStack[index].push(mScissors[index]);
    setScissors(index, sc);
}

void GraphicsState::popScissors(uint32_t index) {
    if (mScStack[index].empty()) {
        logError("PipelineState::popScissors() - can't pop scissors since the scissors stack is empty.");
        return;
    }
    const auto& sc = mScStack[index].top();
    setScissors(index, sc);
    mScStack[index].pop();
}

void GraphicsState::setViewport(uint32_t index, const GraphicsState::Viewport& vp, bool setScissors) {
    mViewports[index] = vp;

    if (setScissors) {
        GraphicsState::Scissor sc;
        sc.left = (int32_t)vp.originX;
        sc.right = sc.left + (int32_t)vp.width;
        sc.top = (int32_t)vp.originY;
        sc.bottom = sc.top + (int32_t)vp.height;
        this->setScissors(index, sc);
    }
}

void GraphicsState::setScissors(uint32_t index, const GraphicsState::Scissor& sc) {
    mScissors[index] = sc;
}

SCRIPT_BINDING(GraphicsState) {
    pybind11::class_<GraphicsState, GraphicsState::SharedPtr>(m, "GraphicsState");
}

}  // namespace Falcor
