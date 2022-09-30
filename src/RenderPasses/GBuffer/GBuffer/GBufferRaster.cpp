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
#include "Falcor.h"

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassStandardFlags.h"
#include "GBufferRaster.h"

const RenderPass::Info GBufferRaster::kInfo { "GBufferRaster", "Rasterized G-buffer generation pass." };

namespace {

    const std::string kProgramFile = "RenderPasses/GBuffer/GBuffer/GBufferRaster.3d.slang";
    const std::string shaderModel = "450";
    const RasterizerState::CullMode kDefaultCullMode = RasterizerState::CullMode::Back;

    
    // Additional output channels.
    // TODO: Some are RG32 floats now. I'm sure that all of these could be fp16.
    const std::string kVBufferName = "vbuffer";
    const ChannelList kGBufferExtraChannels = {
        { "vbuffer",          "gVBuffer",            "Visibility buffer",                true /* optional */, ResourceFormat::RG32Uint    },
        { "mvec",             "gMotionVectors",      "Motion vectors",                   true /* optional */, ResourceFormat::RG32Float   },
        { "faceNormalW",      "gFaceNormalW",        "Face normal in world space",       true /* optional */, ResourceFormat::RGBA32Float },
        { "pnFwidth",         "gPosNormalFwidth",    "position and normal filter width", true /* optional */, ResourceFormat::RG32Float   },
        { "linearZ",          "gLinearZAndDeriv",    "linear z (and derivative)",        true /* optional */, ResourceFormat::RG32Float   },
        { "surfSpreadAngle",  "gSurfaceSpreadAngle", "surface spread angle (texlod)",    true /* optional */, ResourceFormat::R16Float    },
    };

    const std::string kDepthName = "depth";
}

RenderPassReflection GBufferRaster::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    // Add the required depth output. This always exists.
    reflector.addOutput(kDepthName, "Depth buffer").format(ResourceFormat::D32Float).bindFlags(Resource::BindFlags::DepthStencil);

    // Add all the other outputs.
    // The default channels are written as render targets, the rest as UAVs as there is way to assign/pack render targets yet.
    addRenderPassOutputs(reflector, kGBufferChannels, Resource::BindFlags::RenderTarget);
    addRenderPassOutputs(reflector, kGBufferExtraChannels, Resource::BindFlags::UnorderedAccess);

    return reflector;
}

GBufferRaster::SharedPtr GBufferRaster::create(RenderContext* pRenderContext, const Dictionary& dict) {
    return SharedPtr(new GBufferRaster(pRenderContext->device(), dict));
}

GBufferRaster::GBufferRaster(Device::SharedPtr pDevice, const Dictionary& dict): GBuffer(pDevice, kInfo) {
    // Check for required features.
    if (!pDevice->isFeatureSupported(Device::SupportedFeatures::Barycentrics)) {
        throw std::runtime_error("Pixel shader barycentrics are not supported by the current device");
    }
    if (!pDevice->isFeatureSupported(Device::SupportedFeatures::RasterizerOrderedViews)) {
        throw std::runtime_error("Rasterizer ordered views (ROVs) are not supported by the current device");
    }

    parseDictionary(dict);

    // Create raster program
    Program::DefineList defines = { { "_DEFAULT_ALPHA_TEST", "" }, {"DISABLE_RAYTRACING", ""} };
    Program::Desc desc;
    desc.addShaderLibrary(kProgramFile).vsEntry("vsMain").psEntry("psMain");
    desc.setShaderModel(shaderModel);
    mRaster.pProgram = GraphicsProgram::create(mpDevice, desc, defines);

    // Initialize graphics state
    mRaster.pState = GraphicsState::create(mpDevice);
    mRaster.pState->setProgram(mRaster.pProgram);

    // Set default cull mode
    setCullMode(mCullMode);

    // Set depth function
    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthFunc(DepthStencilState::Func::Equal).setDepthWriteMask(false);
    DepthStencilState::SharedPtr pDsState = DepthStencilState::create(dsDesc);
    mRaster.pState->setDepthStencilState(pDsState);

    mpFbo = Fbo::create(mpDevice);
}

void GBufferRaster::compile(RenderContext* pContext, const CompileData& compileData) {
    GBuffer::compile(pContext, compileData);

    mpDepthPrePassGraph = RenderGraph::create(pContext->device(), mGBufferParams.frameSize, ResourceFormat::D32Float , "Depth Pre-Pass");
    mpDepthPrePass = DepthPass::create(pContext);
    mpDepthPrePass->setDepthBufferFormat(ResourceFormat::D32Float);
    mpDepthPrePassGraph->addPass(mpDepthPrePass, "DepthPrePass");
    mpDepthPrePassGraph->markOutput("DepthPrePass.depth");
    mpDepthPrePassGraph->setScene(mpScene);

    //mpTexturesResolvePassGraph = RenderGraph::create(pContext->device(), mGBufferParams.frameSize, ResourceFormat::RGBA16Float , "Sparse textures resolve Pre-Pass");
    //mpTexturesResolvePass = TexturesResolvePass::create(pContext);
    //mpTexturesResolvePassGraph->addPass(mpTexturesResolvePass, "SparseTexturesResolvePrePass");
    // //mpTexturesResolvePassGraph->setInput("SparseTexturesResolvePrePass.depth", mpDepthPrePassGraph->getOutput("DepthPrePass.depth"));
    //mpTexturesResolvePassGraph->markOutput("SparseTexturesResolvePrePass.debugColor");
    //mpTexturesResolvePassGraph->setScene(mpScene);
}

void GBufferRaster::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    GBuffer::setScene(pRenderContext, pScene);

    mRaster.pVars = nullptr;

    if (pScene) {
        if (pScene->getMeshVao()->getPrimitiveTopology() != Vao::Topology::TriangleList) {
            throw std::runtime_error("GBufferRaster only works with triangle list geometry due to usage of SV_Barycentrics.");
        }

        mRaster.pProgram->addDefines(pScene->getSceneDefines());
    }

    if (mpDepthPrePassGraph) mpDepthPrePassGraph->setScene(pScene);
}

void GBufferRaster::resolvePerFrameSparseResources(RenderContext* pRenderContext, const RenderData& renderData) {
    GBuffer::resolvePerFrameSparseResources(pRenderContext, renderData);

    // TODO: Setup depth pass to use same culling mode.

    // Copy depth buffer.
    mpDepthPrePassGraph->execute(pRenderContext);
    
    // Execute sparse textures resolve pass
    //mpTexturesResolvePassGraph->setInput("SparseTexturesResolvePrePass.depth", mpDepthPrePassGraph->getOutput("DepthPrePass.depth"));
    //mpTexturesResolvePassGraph->execute(pRenderContext);
}

void GBufferRaster::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    GBuffer::execute(pRenderContext, renderData);
    
    // Bind primary channels as render targets and clear them.
    for (size_t i = 0; i < kGBufferChannels.size(); ++i) {
        Texture::SharedPtr pTex = renderData[kGBufferChannels[i].name]->asTexture();
        mpFbo->attachColorTarget(pTex, uint32_t(i));
    }
    pRenderContext->clearFbo(mpFbo.get(), float4(0), 1.f, 0, FboAttachmentType::Color);

    // Clear extra output buffers.
    auto clear = [&](const ChannelDesc& channel) {
        auto pTex = renderData[channel.name]->asTexture();
        if (pTex) {
            if (channel.name == kVBufferName) pRenderContext->clearUAV(pTex->getUAV().get(), uint4(std::numeric_limits<uint32_t>::max()));
            else pRenderContext->clearUAV(pTex->getUAV().get(), float4(0.f));
        }
    };
    for (const auto& channel : kGBufferExtraChannels) clear(channel);

    // If there is no scene, clear depth buffer and return.
    if (mpScene == nullptr) {
        auto pDepth = renderData[kDepthName]->asTexture();
        pRenderContext->clearDsv(pDepth->getDSV().get(), 1.f, 0);
        return;
    }

    // Set program defines.
    mRaster.pProgram->addDefine("ADJUST_SHADING_NORMALS", mAdjustShadingNormals ? "1" : "0");
    mRaster.pProgram->addDefine("USE_ALPHA_TEST", mUseAlphaTest ? "1" : "0");

    // For optional I/O resources, set 'is_valid_<name>' defines to inform the program of which ones it can access.
    // TODO: This should be moved to a more general mechanism using Slang.
    mRaster.pProgram->addDefines(getValidResourceDefines(kGBufferExtraChannels, renderData));

    // Create program vars.
    if (!mRaster.pVars) {
        mRaster.pVars = GraphicsVars::create(mpDevice, mRaster.pProgram.get());
    }

    // Setup depth pass to use same culling mode.
    RasterizerState::CullMode cullMode = mForceCullMode ? mCullMode : kDefaultCullMode;
    mpDepthPrePass->setCullMode(cullMode);

    // Copy depth buffer.
    mpDepthPrePassGraph->execute(pRenderContext);
    mpFbo->attachDepthStencilTarget(mpDepthPrePassGraph->getOutput("DepthPrePass.depth")->asTexture());
    pRenderContext->copyResource(renderData[kDepthName].get(), mpDepthPrePassGraph->getOutput("DepthPrePass.depth").get());

    // Bind extra channels as UAV buffers.
    for (const auto& channel : kGBufferExtraChannels) {
        Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
        mRaster.pVars[channel.texname] = pTex;
    }

    mRaster.pVars["PerFrameCB"]["gParams"].setBlob(mGBufferParams);
    mRaster.pState->setFbo(mpFbo); // Sets the viewport

    // Rasterize the scene.
    mpScene->rasterize(pRenderContext, mRaster.pState.get(), mRaster.pVars.get(), cullMode);

    mGBufferParams.frameCount++;
}
