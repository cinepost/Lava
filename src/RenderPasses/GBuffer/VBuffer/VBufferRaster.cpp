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
#include "VBufferRaster.h"
#include "Scene/HitInfo.h"

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassStandardFlags.h"

const RenderPass::Info VBufferRaster::kInfo { "VBufferRaster", "Rasterized V-buffer generation pass." };

namespace {
    const std::string kProgramFile = "RenderPasses/GBuffer/VBuffer/VBufferRaster.3d.slang";
    const std::string kShaderModel = "6_2";

    const RasterizerState::CullMode kDefaultCullMode = RasterizerState::CullMode::None;

    const std::string kVBufferName = "vbuffer";
    const std::string kVBufferDesc = "V-buffer in packed format (indices + barycentrics)";

    const ChannelList kVBufferExtraOutputChannels = {
        { "mvec",             "gMotionVector",      "Motion vectors",                   true /* optional */, ResourceFormat::RG16Float   },
        { "texGrads",         "gTextureGrads",      "Texture coordiante gradients",     true /* optional */, ResourceFormat::RGBA16Float },
    };

    const std::string kDepthName = "depth";
}

RenderPassReflection VBufferRaster::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;

    const auto& texDims = compileData.defaultTexDims;

    reflector.addInputOutput(kDepthName, "Pre-initialized depth-buffer").format(ResourceFormat::D32Float).bindFlags(Resource::BindFlags::DepthStencil)
        .flags(RenderPassReflection::Field::Flags::Optional);//.texture2D(texDims);

    reflector.addOutput(kVBufferName, kVBufferDesc).bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess).format(mVBufferFormat).texture2D(texDims);
    
    // Add all the other outputs.
    addRenderPassOutputs(reflector, kVBufferExtraOutputChannels, Resource::BindFlags::UnorderedAccess, texDims);

    return reflector;
}

VBufferRaster::SharedPtr VBufferRaster::create(RenderContext* pRenderContext, const Dictionary& dict) {
    return SharedPtr(new VBufferRaster(pRenderContext->device(), dict));
}
VBufferRaster::VBufferRaster(Device::SharedPtr pDevice, const Dictionary& dict) : GBufferBase(pDevice, kInfo) {
    parseDictionary(dict);

    // Check for required features.
    if (!pDevice->isFeatureSupported(Device::SupportedFeatures::Barycentrics)) {
        throw std::runtime_error("Pixel shader barycentrics are not supported by the current device");
    }
    if (!pDevice->isFeatureSupported(Device::SupportedFeatures::RasterizerOrderedViews)) {
        throw std::runtime_error("Rasterizer ordered views (ROVs) are not supported by the current device");
    }

    parseDictionary(dict);

    // Create raster program
    Program::Desc desc;
    desc.addShaderLibrary(kProgramFile).vsEntry("vsMain").psEntry("psMain");
    desc.setShaderModel(kShaderModel);
    mRaster.pProgram = GraphicsProgram::create(pDevice, desc);

    // Initialize graphics state
    mRaster.pState = GraphicsState::create(pDevice);
    mRaster.pState->setProgram(mRaster.pProgram);

    // Set depth function
    //DepthStencilState::Desc dsDesc;
    //dsDesc.setDepthFunc(DepthStencilState::Func::LessEqual).setDepthWriteMask(true);
    //mRaster.pState->setDepthStencilState(DepthStencilState::create(dsDesc));

    mpFbo = Fbo::create(pDevice);
}

void VBufferRaster::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    if(mpScene == pScene) return;

    GBufferBase::setScene(pRenderContext, pScene);

    mRaster.pVars = nullptr;

    if (pScene) {
        if (pScene->getMeshVao()->getPrimitiveTopology() != Vao::Topology::TriangleList) {
            throw std::runtime_error("VBufferRaster only works with triangle list geometry due to usage of SV_Barycentrics.");
        }

        mRaster.pProgram->addDefines(pScene->getSceneDefines());
        mRaster.pProgram->setTypeConformances(pScene->getTypeConformances());
    }
}

void VBufferRaster::initDepth(RenderContext* pContext, const RenderData& renderData) {
    if(!mDirty) return;

    const auto& pDepth = renderData[kDepthName]->asTexture();

    if (pDepth) {
        // Using external depth buffer texture
        LLOG_DBG << "VBufferRaster using external depth buffer";

        DepthStencilState::Desc dsDesc;
        dsDesc.setDepthWriteMask(false).setDepthFunc(DepthStencilState::Func::LessEqual);
        mRaster.pState->setDepthStencilState(DepthStencilState::create(dsDesc));
        mpFbo->attachDepthStencilTarget(pDepth);
    } else {
        // Using own generated depth buffer texture
        LLOG_DBG << "VBufferRaster using internal depth buffer";
        
        mpDepth = Texture::create2D(pContext->device(), mFrameDim.x, mFrameDim.y, ResourceFormat::D32Float, 1, 1, nullptr, Resource::BindFlags::DepthStencil | Resource::BindFlags::ShaderResource);
        
        DepthStencilState::Desc dsDesc;
        dsDesc.setDepthFunc(DepthStencilState::Func::LessEqual).setDepthWriteMask(true).setDepthEnabled(false); // WTF !?
        mRaster.pState->setDepthStencilState(DepthStencilState::create(dsDesc));
    }
}

void VBufferRaster::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    GBufferBase::execute(pRenderContext, renderData);
    
    // Update frame dimension based on render pass output.
    auto pOutput = renderData[kVBufferName]->asTexture();
    assert(pOutput);
    updateFrameDim(uint2(pOutput->getWidth(), pOutput->getHeight()));
    
    initDepth(pRenderContext, renderData);

    // Clear output buffer.
    pRenderContext->clearUAV(pOutput->getUAV().get(), uint4(0)); // Clear as UAV for integer clear value
    
    /// Cleat depth if we are using internal
    if(mpDepth) {
        pRenderContext->clearDsv(mpDepth->getDSV().get(), 1, 0);
    }

    // Clear extra output buffers.
    //clearRenderPassChannels(pRenderContext, kVBufferExtraChannels, renderData);
    
    // If there is no scene, we're done.
    if (mpScene == nullptr) {
        return;
    }

    // Set program defines.
    const auto& pExternalDepth = renderData[kDepthName]->asTexture();
    mRaster.pProgram->addDefine("USE_ALPHA_TEST", mUseAlphaTest ? "1" : "0");
    mRaster.pProgram->addDefine("_USE_EXTERNAL_DEPTH_BUFFER", pExternalDepth ? "1" : "0");
    mRaster.pProgram->addDefine("_USE_INTERNAL_DEPTH_BUFFER", mpDepth ? "1" : "0");

    // For optional I/O resources, set 'is_valid_<name>' defines to inform the program of which ones it can access.
    // TODO: This should be moved to a more general mechanism using Slang.
    mRaster.pProgram->addDefines(getValidResourceDefines(kVBufferExtraOutputChannels, renderData));

    // Create program vars.
    if (!mRaster.pVars) {
        mRaster.pVars = GraphicsVars::create(mpDevice, mRaster.pProgram.get());
    }

    mpFbo->attachColorTarget(pOutput, 0);
    mpFbo->attachDepthStencilTarget(mpDepth ? mpDepth : renderData[kDepthName]->asTexture());
    mRaster.pState->setFbo(mpFbo); // Sets the viewport
    mRaster.pVars["PerFrameCB"]["gFrameDim"] = mFrameDim;

    // Bind extra outpu channels as UAV buffers.
    for (const auto& channel : kVBufferExtraOutputChannels) {
        Texture::SharedPtr pTex = getOutput(renderData, channel.name);
        mRaster.pVars[channel.texname] = pTex;
    }

    // Rasterize the scene.
    RasterizerState::CullMode cullMode = mForceCullMode ? mCullMode : kDefaultCullMode;
    mpScene->rasterize(pRenderContext, mRaster.pState.get(), mRaster.pVars.get(), cullMode);

    mDirty = false;
}
