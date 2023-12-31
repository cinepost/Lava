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
#include "Falcor/Utils/SampleGenerators/HaltonSamplePattern.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassStandardFlags.h"

const RenderPass::Info VBufferRaster::kInfo { "VBufferRaster", "Rasterized V-buffer generation pass." };

namespace {
    const std::string kProgramFile = "RenderPasses/GBuffer/VBuffer/VBufferRaster.3d.slang";
    const std::string kQuadCombineFile = "RenderPasses/GBuffer/VBuffer/QuadInterleaveCombiner.cs.slang";
    const std::string kShaderModel = "6_2";

    const RasterizerState::CullMode kDefaultCullMode = RasterizerState::CullMode::None;

    const std::string kVBufferName = "vbuffer";
    const std::string kVBufferDesc = "V-buffer in packed format (indices + barycentrics)";
    const std::string kDepthName   = "depth";

    // Scripting options.
    const char perPixelJitterRaster[] = "perPixelJitterRaster";

    // Extra output channels.
    const ChannelList kVBufferExtraOutputChannels = {
        { "mvec",             "gMotionVector",      "Motion vectors",                   true /* optional */, ResourceFormat::RG16Float   },
        { "texGrads",         "gTextureGrads",      "Texture coordiante gradients",     true /* optional */, ResourceFormat::RGBA16Float },
    };
}

void VBufferRaster::parseDictionary(const Dictionary& dict) {
    GBufferBase::parseDictionary(dict);

    for (const auto& [key, value] : dict) {
        if (key == perPixelJitterRaster) setPerPixelJitterRaster(value);
    }
}

RenderPassReflection VBufferRaster::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;

    const auto& texDims = compileData.defaultTexDims;

    //reflector.addInputOutput(kDepthName, "Pre-initialized depth-buffer")
    //    .format(ResourceFormat::Unknown).bindFlags(Resource::BindFlags::DepthStencil).flags(RenderPassReflection::Field::Flags::Optional);

    reflector.addOutput(kDepthName, "Depth-buffer")
        .format(mDepthFormat).bindFlags(Resource::BindFlags::DepthStencil).texture2D(texDims);

    reflector.addOutput(kVBufferName, kVBufferDesc).bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess).format(mVBufferFormat).texture2D(texDims);

    // Add extra outputs.
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

    mpSampleGenerator = StratifiedSamplePattern::create(1024);

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

    // Quad jittered mode parm
    mSubPasses[0].cameraJitterOffset = {.5f, -.5f};
    mSubPasses[1].cameraJitterOffset = {-.5f, -.5f};
    mSubPasses[2].cameraJitterOffset = {.5f, .5f};
    mSubPasses[3].cameraJitterOffset = {-.5f, .5f};

    mSubPasses[0].pSampleGenerator = StratifiedSamplePattern::create(1024);
    mSubPasses[1].pSampleGenerator = StratifiedSamplePattern::create(1024);
    mSubPasses[2].pSampleGenerator = StratifiedSamplePattern::create(1024);
    mSubPasses[3].pSampleGenerator = StratifiedSamplePattern::create(1024);
}

void VBufferRaster::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    GBufferBase::setScene(pRenderContext, pScene);

    mRaster.pVars = nullptr;

    if (mpScene && mDirty) {
        if (mpScene->getMeshVao()->getPrimitiveTopology() != Vao::Topology::TriangleList) {
            throw std::runtime_error("VBufferRaster only works with triangle list geometry due to usage of SV_Barycentrics.");
        }

        mRaster.pProgram->addDefines(pScene->getSceneDefines());
        mRaster.pProgram->setTypeConformances(pScene->getTypeConformances());
    }
}

void VBufferRaster::initDepth(RenderContext* pContext, const RenderData& renderData) {
    if(!mDirty) return;

    const auto& pDepthExternal = renderData[kDepthName]->asTexture();

    if (1 == 2) {
        // Using external depth buffer texture
        LLOG_DBG << "VBufferRaster using external depth buffer";

        mpDepth = nullptr;
        DepthStencilState::Desc dsDesc;
        dsDesc.setDepthWriteMask(false).setDepthFunc(DepthStencilState::Func::LessEqual);
        mRaster.pState->setDepthStencilState(DepthStencilState::create(dsDesc));
    } else {
        // Using own generated depth buffer texture
        LLOG_DBG << "VBufferRaster using internal depth buffer";
        
        //mpDepth = Texture::create2D(pContext->device(), mFrameDim.x, mFrameDim.y, ResourceFormat::D32Float, 1, 1, nullptr, Resource::BindFlags::DepthStencil | Resource::BindFlags::ShaderResource);

        DepthStencilState::Desc dsDesc;
        dsDesc.setDepthFunc(DepthStencilState::Func::LessEqual).setDepthWriteMask(true).setDepthEnabled(true);
        mRaster.pState->setDepthStencilState(DepthStencilState::create(dsDesc));
    }    
}

void VBufferRaster::initFineDepth(RenderContext *pContext, const RenderData& renderData) {
    if(!mHighpDepthEnabled && !mDirty) return;

    mpHighpDepth = Texture::create2D(pContext->device(), mFrameDim.x, mFrameDim.y, ResourceFormat::R32Float, 1, 1, nullptr, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource);

    mpTestTexture = Texture::create2D(pContext->device(), mFrameDim.x, mFrameDim.y, ResourceFormat::RGBA8Unorm, 1, 1, nullptr, Resource::BindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource);

}

void VBufferRaster::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    //GBufferBase::execute(pRenderContext, renderData);
    
    // Update frame dimension based on render pass output.
    auto pOutput = renderData[kVBufferName]->asTexture();
    assert(pOutput);
    updateFrameDim(uint2(pOutput->getWidth(), pOutput->getHeight()));
    
    initDepth(pRenderContext, renderData);
    initFineDepth(pRenderContext, renderData);

    auto pDepthInternal = renderData[kDepthName]->asTexture();

    // Clear output buffer.
    pRenderContext->clearUAV(pOutput->getUAV().get(), uint4(0)); // Clear as UAV for integer clear value
    
    /// Clear depth if we are using internal one.
    pRenderContext->clearDsv(pDepthInternal->getDSV().get(), 1.f, 0);

    /// Clear fine depth buffer.
    //pRenderContext->clearUAV(mpFineDepth->getUAV().get(), uint4(0));
    pRenderContext->clearUAV(mpHighpDepth->getUAV().get(), float4(std::numeric_limits<float>::max()));
    pRenderContext->clearUAV(mpTestTexture->getUAV().get(), uint4(0));
    
    // If there is no scene, we're done.
    if (!mpScene) return;

    // Clear extra output buffers.
    //clearRenderPassChannels(pRenderContext, kVBufferExtraChannels, renderData);

    if(mDirty) {
        // Set program defines.
        mRaster.pProgram->addDefine("USE_ALPHA_TEST", mUseAlphaTest ? "1" : "0");
        mRaster.pProgram->addDefine("is_valid_gCamZDepth", mpHighpDepth != nullptr ? "1" : "0");
        mRaster.pProgram->addDefine("is_valid_gTestTexture", mpTestTexture != nullptr ? "1" : "0");

        // For optional I/O resources, set 'is_valid_<name>' defines to inform the program of which ones it can access.
        // TODO: This should be moved to a more general mechanism using Slang.
        mRaster.pProgram->addDefines(getValidResourceDefines(kVBufferExtraOutputChannels, renderData));
    }

    // Create program vars.
    if (!mRaster.pVars) mRaster.pVars = GraphicsVars::create(mpDevice, mRaster.pProgram.get());

    if(mPerPixelJitterRaster) {
        // 4 quads jittered rendering

        initQuarterBuffers(pRenderContext, renderData);

        const auto& pCamera = mpScene->getCamera();

        // Save scene camera sampling generator
        const float2 samplePatternGeneratorScale = pCamera->getPatternGeneratorScale();
        const auto& pSamplePatternGenerator = pCamera->getPatternGenerator();

        //CPUSampleGenerator::SharedPtr pTmpSampleGenerator = std::make_shared<CPUSampleGenerator>(*pSamplePatternGenerator);

        uint32_t i = 0;
        for(auto& subPass: mSubPasses) {
            LLOG_DBG << "VBufferRaster rasterizing jittered quad " << i++;
            subPass.pFbo->attachColorTarget(subPass.pVBuff, 0);
            subPass.pFbo->attachDepthStencilTarget(subPass.pDepth);
            mRaster.pState->setFbo(subPass.pFbo);
            mRaster.pVars["PerFrameCB"]["gFrameDim"] = mQuarterFrameDim;

            // Bind extra outpu channels as UAV buffers.
            for (const auto& channel : kVBufferExtraOutputChannels) {
                Texture::SharedPtr pTex = getOutput(renderData, channel.name);
                mRaster.pVars[channel.texname] = pTex;
            }

            // Adjust camera.
            
            float2 subPixelJitter = subPass.pSampleGenerator->next(); // range [-0.5 : 0.5]
            LLOG_DBG << "Sub pixel jitter " << to_string(subPixelJitter);
            pCamera->setJitter((subPass.cameraJitterOffset + subPixelJitter) * samplePatternGeneratorScale);
            mpScene->update(pRenderContext, 0.0);

            // Rasterize the scene.
            mpScene->rasterize(pRenderContext, mRaster.pState.get(), mRaster.pVars.get(), mForceCullMode ? mCullMode : kDefaultCullMode);
        }

        // Restore camera sampling
        pCamera->setPatternGenerator(pSamplePatternGenerator, samplePatternGeneratorScale);
        mpScene->update(pRenderContext, 0.0);

        if(mDirty) {
            if(!mpCombineQuadsProgram) {
                mpCombineQuadsProgram = ComputeProgram::createFromFile(mpDevice, kQuadCombineFile, "combine", Program::DefineList(), Shader::CompilerFlags::TreatWarningsAsErrors);
                assert(mpCombineQuadsProgram);

                mpCombineQuadsVars = ComputeVars::create(mpDevice, mpCombineQuadsProgram->getReflector());
                mpCombineQuadsState = ComputeState::create(mpDevice);
                mpCombineQuadsState->setProgram(mpCombineQuadsProgram);
            }
        }

        mpCombineQuadsVars["PerFrameCB"]["gOutputResolution"] = mFrameDim;
        mpCombineQuadsVars["PerFrameCB"]["gQuadResolution"] = mQuarterFrameDim;

        mpCombineQuadsVars["gVBuff1"] = mSubPasses[0].pVBuff;
        mpCombineQuadsVars["gVBuff2"] = mSubPasses[1].pVBuff;
        mpCombineQuadsVars["gVBuff3"] = mSubPasses[2].pVBuff;
        mpCombineQuadsVars["gVBuff4"] = mSubPasses[3].pVBuff;

        mpCombineQuadsVars["gDepth1"] = mSubPasses[0].pDepth;
        mpCombineQuadsVars["gDepth2"] = mSubPasses[1].pDepth;
        mpCombineQuadsVars["gDepth3"] = mSubPasses[2].pDepth;
        mpCombineQuadsVars["gDepth4"] = mSubPasses[3].pDepth;

        mpCombineQuadsVars["gOutputVBuff"] = pOutput;
        mpCombineQuadsVars["gOutputDepth"] = pDepthInternal;

        uint3 numGroups = div_round_up(uint3(mQuarterFrameDim.x, mQuarterFrameDim.y, 1u), mpCombineQuadsProgram->getReflector()->getThreadGroupSize());
        pRenderContext->dispatch(mpCombineQuadsState.get(), mpCombineQuadsVars.get(), numGroups);

    } else {
        // Conventional rendering

        if(mDirty) {
            mpFbo->attachColorTarget(pOutput, 0);
            mpFbo->attachDepthStencilTarget(pDepthInternal);
        
            mRaster.pState->setFbo(mpFbo); // Sets the viewport
            mRaster.pVars["gVBuffer"] = pOutput;
            mRaster.pVars["gCamZDepth"] = mpHighpDepth;
            mRaster.pVars["gTestTexture"] = mpTestTexture;
            mRaster.pVars["PerFrameCB"]["gFrameDim"] = mFrameDim;

            // Bind extra outpu channels as UAV buffers.
            for (const auto& channel : kVBufferExtraOutputChannels) {
                Texture::SharedPtr pTex = getOutput(renderData, channel.name);
                mRaster.pVars[channel.texname] = pTex;
            }
        }

        // Rasterize the scene.
        mpScene->rasterize(pRenderContext, mRaster.pState.get(), mRaster.pVars.get(), mForceCullMode ? mCullMode : kDefaultCullMode);
    }

    //mpTestTexture->captureToFile(0, 0, "/home/max/ztest.png", Bitmap::FileFormat::PngFile, Bitmap::ExportFlags::None);

    mDirty = false;
}

void VBufferRaster::initQuarterBuffers(RenderContext* pContext, const RenderData& renderData) {
    const auto& pOutput = renderData[kVBufferName]->asTexture();
    const auto& pDepth = renderData[kDepthName]->asTexture();

    if(!pOutput || !pDepth) return;

    auto depthFormat = pDepth->getFormat();
    auto pDevice = pContext->device();

    auto prepareBuffer = [&](Texture::SharedPtr& pBuf, uint32_t width, uint32_t height, ResourceFormat format, bool bufUsed) {
        if (!bufUsed) {
            pBuf = nullptr;
            return;
        }

        bool isDepth = isDepthStencilFormat(format);
        // (Re-)create buffer if needed.
        if (!pBuf || pBuf->getWidth() != width || pBuf->getHeight() != height) {
            Resource::BindFlags bindFlags;
            if (isDepth) {
                bindFlags = Resource::BindFlags::ShaderResource | Resource::BindFlags::DepthStencil;
            } else {
                bindFlags = Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess;
            }

            pBuf = Texture::create2D(pDevice, width, height, format, 1, 1, nullptr, bindFlags);
            assert(pBuf);
            mFrameCount = 0;
            mDirty = true;
        }
        // Clear data if accumulation has been reset (either above or somewhere else).
        if (mFrameCount == 0 && pBuf) {
            if (isDepth) {
                pContext->clearDsv(pBuf->getDSV().get(), 1.f, 0);
            } else {
                pContext->clearUAV(pBuf->getUAV().get(), uint4(0));
            }
        }
    };

    mQuarterFrameDim.x = mFrameDim.x >> 1;
    mQuarterFrameDim.y = mFrameDim.y >> 1;

    for(auto& subPass: mSubPasses) {
        if(!subPass.pFbo && mPerPixelJitterRaster) subPass.pFbo = Fbo::create(pDevice);
        
        prepareBuffer(subPass.pVBuff, mQuarterFrameDim.x, mQuarterFrameDim.y, mVBufferFormat, mPerPixelJitterRaster);
        prepareBuffer(subPass.pVBuff, mQuarterFrameDim.x, mQuarterFrameDim.y, mVBufferFormat, mPerPixelJitterRaster);
        prepareBuffer(subPass.pVBuff, mQuarterFrameDim.x, mQuarterFrameDim.y, mVBufferFormat, mPerPixelJitterRaster);
        prepareBuffer(subPass.pVBuff, mQuarterFrameDim.x, mQuarterFrameDim.y, mVBufferFormat, mPerPixelJitterRaster);

        prepareBuffer(subPass.pDepth, mQuarterFrameDim.x, mQuarterFrameDim.y, depthFormat, mPerPixelJitterRaster);
        prepareBuffer(subPass.pDepth, mQuarterFrameDim.x, mQuarterFrameDim.y, depthFormat, mPerPixelJitterRaster);
        prepareBuffer(subPass.pDepth, mQuarterFrameDim.x, mQuarterFrameDim.y, depthFormat, mPerPixelJitterRaster);
        prepareBuffer(subPass.pDepth, mQuarterFrameDim.x, mQuarterFrameDim.y, depthFormat, mPerPixelJitterRaster);
    }
}

VBufferRaster& VBufferRaster::setPerPixelJitterRaster(bool state) {
    if(mPerPixelJitterRaster != state) {
        mPerPixelJitterRaster = state;
        mDirty = true;
    }
    return *this;
}

VBufferRaster& VBufferRaster::setHighpDepth(bool state) {
    if(mHighpDepthEnabled != state) {
        mHighpDepthEnabled = state;
        mDirty = true;
    }
    return *this;
}