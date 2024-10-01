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
#include <algorithm>
#include <pybind11/embed.h>

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/RenderGraph/RenderPassStandardFlags.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"

#include "Falcor/Utils/Textures/FilterKernelsLUT.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Utils/Scripting/ScriptBindings.h"

#include <boost/algorithm/string.hpp>

#include "AccumulatePass.h"

const uint32_t kPixelFilterKernelMinTextureSize = 3u;
const uint32_t kPixelFilterKernelMaxTextureSize = 19u;
const uint32_t kPixelFilterKernelTextureHalfSize = 64u;

const RenderPass::Info AccumulatePass::kInfo { "AccumulatePass", "Buffer accumulation." };


// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

static void regAccumulatePass(pybind11::module& m) {
    pybind11::class_<AccumulatePass, RenderPass, AccumulatePass::SharedPtr> pass(m, "AccumulatePass");
    pass.def("reset", &AccumulatePass::reset);

    pybind11::enum_<AccumulatePass::Precision> precision(m, "AccumulatePrecision");
    precision.value("Double", AccumulatePass::Precision::Double);
    precision.value("Single", AccumulatePass::Precision::Single);
    precision.value("SingleCompensated", AccumulatePass::Precision::SingleCompensated);
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(AccumulatePass::kInfo, AccumulatePass::create);
    ScriptBindings::registerBinding(regAccumulatePass);
}

namespace {

    const char kShaderFile[] = "RenderPasses/AccumulatePass/Accumulate.cs.slang";
    const char kFilterFile[] = "RenderPasses/AccumulatePass/Accumulate.SeparableFilter.cs.slang";

    const std::string kShaderModel = "6_5";

    const char kInputChannel[] = "input";
    const char kOutputChannel[] = "output";
    const char kInputDepthChannel[] = "depth";
    const char kInputSampleOffsetsChannel[] = "offsets";

    const ChannelList kAccumulatePassExtraInputChannels = {
        { kInputDepthChannel,           "gDepth",           "Depth buffer",                 true /* optional */, ResourceFormat::Unknown },
        { kInputSampleOffsetsChannel,   "gSampleOffsets",   "Per pixel sample offsets",     true /* optional */, ResourceFormat::Unknown },
    };

    // Serialized parameters
    const char kEnableAccumulation[] = "enableAccumulation";
    const char kAutoReset[] = "autoReset";
    const char kPrecisionMode[] = "precisionMode";
    const char kSubFrameCount[] = "subFrameCount";
}

static const float hMinDbg = 1.f;
static const float hMaxDbg = 0.f;
static const float vMinDbg = 1.f;
static const float vMaxDbg = 0.f;

static bool isDepthDependentFilterType(AccumulatePass::PixelFilterType filterType) {
    switch(filterType) {
        case PixelFilterType::Farthest:
        case PixelFilterType::Closest:
        case PixelFilterType::Point:    // We use lastFrameDepth to store last per pixel sample distance
            return true;
        default:
            return false;
    }
}

AccumulatePass::SharedPtr AccumulatePass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    return SharedPtr(new AccumulatePass(pRenderContext->device(), dict));
}

AccumulatePass::AccumulatePass(Device::SharedPtr pDevice, const Dictionary& dict): RenderPass(pDevice, kInfo) {
    // Deserialize pass from dictionary.
    for (const auto& [key, value] : dict) {
        if (key == kEnableAccumulation) mEnableAccumulation = value;
        else if (key == kAutoReset) mAutoReset = value;
        else if (key == kPrecisionMode) mPrecisionMode = value;
        else if (key == kSubFrameCount) mSubFrameCount = value;
        else LLOG_WRN << "Unknown field '" << key << "' in AccumulatePass dictionary";
    }

    mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_UNIFORM);

    setPixelFilterType(PixelFilterType::Box);
    setPixelFilterSize({1u, 1u});

    // Create accumulation programs.
    // Note only compensated summation needs precise floating-point mode.
    mpProgram[Precision::Double] = ComputeProgram::createFromFile(pDevice, kShaderFile, "accumulateDouble", Program::DefineList(), Shader::CompilerFlags::TreatWarningsAsErrors);
    mpProgram[Precision::Single] = ComputeProgram::createFromFile(pDevice, kShaderFile, "accumulateSingle", Program::DefineList(), Shader::CompilerFlags::TreatWarningsAsErrors);
    mpProgram[Precision::SingleCompensated] = ComputeProgram::createFromFile(pDevice, kShaderFile, "accumulateSingleCompensated", Program::DefineList(), Shader::CompilerFlags::FloatingPointModePrecise | Shader::CompilerFlags::TreatWarningsAsErrors);
    
    for(auto& [key, pProgram]: mpProgram) {
        pProgram->addDefine("is_valid_gDepth", "0");
    }

    mpVars = ComputeVars::create(pDevice, mpProgram[mPrecisionMode]->getReflector());
    mpState = ComputeState::create(pDevice);
}

Dictionary AccumulatePass::getScriptingDictionary() {
    Dictionary dict;
    dict[kEnableAccumulation] = mEnableAccumulation;
    dict[kAutoReset] = mAutoReset;
    dict[kPrecisionMode] = mPrecisionMode;
    dict[kSubFrameCount] = mSubFrameCount;
    return dict;
}

RenderPassReflection AccumulatePass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    reflector.addInput(kInputChannel, "Input data to be accumulated").bindFlags(ResourceBindFlags::ShaderResource);
    reflector.addOutput(kOutputChannel, "Output data that is accumulated").bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource)
        .format(mOutputFormat);

    addRenderPassInputs(reflector, kAccumulatePassExtraInputChannels, ResourceBindFlags::ShaderResource);

    return reflector;
}

void AccumulatePass::compile(RenderContext* pContext, const CompileData& compileData) {
    assert(mpDevice == pContext->device());

    // Reset accumulation when resolution changes.
    if (compileData.defaultTexDims != mFrameDim) {
        mFrameCount = 0;
        mFrameDim = compileData.defaultTexDims;
        mDirty = true;
    }
}

void AccumulatePass::enableAccumulation(bool enable) {
    if(mEnableAccumulation == enable) return;
    reset();
    mEnableAccumulation = enable;
}

void AccumulatePass::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    if (mAutoReset) {
        if (mSubFrameCount > 0) // Option to accumulate N frames. Works also for motion blur. Overrides logic for automatic reset on scene changes.
        {
            if (mFrameCount == mSubFrameCount) {
                mFrameCount = 0;
            }
        } else {
            // Query refresh flags passed down from the application and other passes.
            auto& dict = renderData.getDictionary();
            auto refreshFlags = dict.getValue(kRenderPassRefreshFlags, RenderPassRefreshFlags::None);

            // If any refresh flag is set, we reset frame accumulation.
            if (refreshFlags != RenderPassRefreshFlags::None) mFrameCount = 0;

            // Reset accumulation upon all scene changes, except camera jitter and history changes.
            // TODO: Add UI options to select which changes should trigger reset
            if (mpScene) {
                auto sceneUpdates = mpScene->getUpdates();
                if ((sceneUpdates & ~Scene::UpdateFlags::CameraPropertiesChanged) != Scene::UpdateFlags::None) {
                    mFrameCount = 0;
                }
                if (is_set(sceneUpdates, Scene::UpdateFlags::CameraPropertiesChanged)) {
                    auto excluded = Camera::Changes::Jitter | Camera::Changes::History;
                    auto cameraChanges = mpScene->getCamera()->getChanges();
                    if ((cameraChanges & ~excluded) != Camera::Changes::None) mFrameCount = 0;
                }
            }
        }
    }

    // Grab our input/output buffers.
    Texture::SharedPtr pSrc = renderData[kInputChannel]->asTexture();
    Texture::SharedPtr pDst = renderData[kOutputChannel]->asTexture();

    Texture::SharedPtr pSrcDepth = renderData[kInputDepthChannel]->asTexture();
    Texture::SharedPtr pSrcOffsets = renderData[kInputSampleOffsetsChannel]->asTexture();


    assert(pSrc && pDst);
    assert(pSrc->getWidth() == pDst->getWidth() && pSrc->getHeight() == pDst->getHeight());
    const uint2 resolution = uint2(pSrc->getWidth(), pSrc->getHeight());


    // Setup filtering passes if needed.
    Texture::SharedPtr pFilteredImage = pSrc;
    Texture::SharedPtr pFilteredDepth = pSrcDepth;

    preparePixelFilterKernelTexture(pRenderContext);

    // Look... For now we use fixed size filter kernel 1D texture used for both vertical and horizontal filtering with fixed 64 elements size
    if(mPixelFilterSize[0] > 1u ||
        (pixelFilterSingularValueCountSVD(mPixelFilterType, kPixelFilterKernelTextureHalfSize, kPixelFilterKernelTextureHalfSize) > 1)) mDoHorizontalFiltering = true;
    if(mPixelFilterSize[1] > 1u || 
        (pixelFilterSingularValueCountSVD(mPixelFilterType, kPixelFilterKernelTextureHalfSize, kPixelFilterKernelTextureHalfSize) > 1)) mDoVerticalFiltering = true;
    if(mPixelFilterType == PixelFilterType::Point) mDoHorizontalFiltering = mDoVerticalFiltering = false; // Point filter doesn't need additional filtering

    // Setup buffers, samplers...
    prepareBuffers(pRenderContext, pSrc, pSrcDepth);
    prepareImageSampler(pRenderContext);

    float2 maxSampleOffset = {float(mPixelFilterSize[0])*0.5, float(mPixelFilterSize[1])*0.5};
    uint2 halfFilterSize = {mPixelFilterSize[0] >> 1, mPixelFilterSize[1] >> 1};
    float2 sampleOffsetUniform = mpCamera ? float2({mpCamera->getSubpixelJitterX(), mpCamera->getSubpixelJitterY()}) : float2({0.f, 0.f});
    float sampleDistanceUniform = sqrt(sampleOffsetUniform.x*sampleOffsetUniform.x+sampleOffsetUniform.y*sampleOffsetUniform.y);

    if(mDoHorizontalFiltering || mDoVerticalFiltering ) {
        //LLOG_DBG << "AccumulatePass sample local distance " << to_string(sampleOffsetUniform);
    }

    // Horizontal filtering pass
    if(mDoHorizontalFiltering) {
        auto& filterPass = mFilterPasses[0];

        if(!filterPass.pPass || mDirty) {
            Program::Desc desc;
            desc.addShaderLibrary(kFilterFile).setShaderModel(kShaderModel).csEntry("filterH");

            auto defines = Program::DefineList();

            defines.add(getValidResourceDefines(kAccumulatePassExtraInputChannels, renderData));
            defines.add("is_valid_gKernelTexture", mpKernelTexture ? "1" : "0");
            defines.add("is_valid_gSampleOffsets", pSrcDepth ? "1" : "0");
            defines.add("_FILTER_HALF_SIZE_H", std::to_string(halfFilterSize[0]));
            defines.add("_FILTER_HALF_SIZE_V", std::to_string(halfFilterSize[1]));
            defines.add("_FILTER_TYPE", std::to_string((uint32_t)mPixelFilterType));
            if (mpSampleGenerator) defines.add(mpSampleGenerator->getDefines());

            filterPass.pPass = ComputePass::create(mpDevice, desc, defines, true);
        }
        
        auto cb_var = filterPass.pPass["PerFrameCB"];
        cb_var["gResolution"] = resolution;
        cb_var["gKernelTextureSize"] = mpKernelTexture ? uint2({mpKernelTexture->getWidth(0), mpKernelTexture->getHeight(0)}) : uint2({0, 0});
        cb_var["gSampleOffsetUniform"] = sampleOffsetUniform;
        cb_var["gSampleDistanceUniform"] = sampleDistanceUniform;
        cb_var["gFilterSize"] = mPixelFilterSize;
        cb_var["gMaxSampleOffset"] = maxSampleOffset;
        cb_var["gSampleNumber"] = mFrameCount;

        auto& pPass = filterPass.pPass;
        pPass["gInput"] = pSrc;
        pPass["gDepth"] = pSrcDepth;

        // Optional textures
        pPass["gSampleOffsets"] = pSrcOffsets;
        pPass["gKernelTexture"] = mpKernelTexture;
        pPass["gKernelSampler"] = mpKernelSampler;
        pPass["gImageSampler"] = mpImageSampler;

        pFilteredImage = mDoVerticalFiltering ? mpTmpFilteredImage : mpFilteredImage;
        pFilteredDepth = mDoVerticalFiltering ? mpTmpFilteredDepth : mpFilteredDepth;
        pPass["gOutputFilteredImage"] = pFilteredImage;
        pPass["gOutputFilteredDepth"] = pFilteredDepth;

        pPass->execute(pRenderContext, resolution.x, resolution.y);
    }
    
    // Horizontal filtering pass
    if(mDoVerticalFiltering) {
        auto& filterPass = mFilterPasses[1];

        if(!filterPass.pPass || mDirty) {
            Program::Desc desc;
            desc.addShaderLibrary(kFilterFile).setShaderModel(kShaderModel).csEntry("filterV");

            auto defines = Program::DefineList();

            defines.add(getValidResourceDefines(kAccumulatePassExtraInputChannels, renderData));
            defines.add("is_valid_gKernelTexture", mpKernelTexture ? "1" : "0");
            defines.add("is_valid_gSampleOffsets", pSrcDepth ? "1" : "0");
            defines.add("_FILTER_HALF_SIZE_H", std::to_string(halfFilterSize[0]));
            defines.add("_FILTER_HALF_SIZE_V", std::to_string(halfFilterSize[1]));
            defines.add("_FILTER_TYPE", std::to_string((uint32_t)mPixelFilterType));
            if (mpSampleGenerator) defines.add(mpSampleGenerator->getDefines());

            filterPass.pPass = ComputePass::create(mpDevice, desc, defines, true);
        }

        auto cb_var = filterPass.pPass["PerFrameCB"];
        cb_var["gResolution"] = resolution;
        cb_var["gKernelTextureSize"] = mpKernelTexture ? uint2({mpKernelTexture->getWidth(0), mpKernelTexture->getHeight(0)}) : uint2({0, 0});
        cb_var["gSampleOffsetUniform"] = sampleOffsetUniform;
        cb_var["gSampleDistanceUniform"] = sampleDistanceUniform;
        cb_var["gFilterSize"] = mPixelFilterSize;
        cb_var["gMaxSampleOffset"] = maxSampleOffset;
        cb_var["gSampleNumber"] = mFrameCount;

        auto& pPass = filterPass.pPass;
        pPass["gInput"] = mDoHorizontalFiltering ? mpTmpFilteredImage : pSrc;
        pPass["gDepth"] = mDoHorizontalFiltering ? mpTmpFilteredDepth : pSrcDepth;
        
        // Optional textures
        pPass["gSampleOffsets"] = pSrcOffsets;
        pPass["gKernelTexture"] = mpKernelTexture;
        pPass["gKernelSampler"] = mpKernelSampler;
        pPass["gImageSampler"] = mpImageSampler;


        pFilteredImage = mpFilteredImage;
        pFilteredDepth = mpFilteredDepth;
        pPass["gOutputFilteredImage"] = pFilteredImage;
        pPass["gOutputFilteredDepth"] = pFilteredDepth;

        pPass->execute(pRenderContext, resolution.x, resolution.y);
    }

    // If accumulation is disabled, just blit the source to the destination and return.
    if (!mEnableAccumulation) {
        // Only blit mip 0 and array slice 0, because that's what the accumulation uses otherwise otherwise.
        pRenderContext->blit(pFilteredImage->getSRV(0, 1, 0, 1), pDst->getRTV(0, 0, 1));
        return;
    }

    // Accumulation

    // Set shader parameters.
    mpVars["PerFrameCB"]["gResolution"] = resolution;
    mpVars["PerFrameCB"]["gAccumCount"] = mFrameCount++;
    mpVars["PerFrameCB"]["gSampleDistanceUniform"] = sampleDistanceUniform;
    mpVars["PerFrameCB"]["gLastSampleDistaceUniform"] = mLastSampleDistanceUniform;
    mpVars["gCurFrame"] = pFilteredImage;
    mpVars["gOutputFrame"] = pDst;

    // Additional channels /samplers
    mpVars["gDepth"] = pSrcDepth;
    mpVars["gSampleOffsets"] = pSrcOffsets;

    // Bind accumulation buffers. Some of these may be nullptr's.
    mpVars["gLastFrameSum"] = mpLastFrameSum;
    mpVars["gLastFrameCorr"] = mpLastFrameCorr;
    mpVars["gLastFrameSumLo"] = mpLastFrameSumLo;
    mpVars["gLastFrameSumHi"] = mpLastFrameSumHi;
    mpVars["gLastFrameDepth"] = mpLastFrameDepth;

    // Run the accumulation program.
    auto pAccProgram = mpProgram[mPrecisionMode];

    // For optional I/O resources, set 'is_valid_<name>' defines to inform the program of which ones it can access.
    // TODO: This should be moved to a more general mechanism using Slang.
    if(mDirty) {
        pAccProgram->addDefines(getValidResourceDefines(kAccumulatePassExtraInputChannels, renderData));
        pAccProgram->addDefine("_FILTER_TYPE", std::to_string((uint32_t)mPixelFilterType));
        pAccProgram->addDefine("is_valid_gSampleOffsets", pSrcDepth ? "1" : "0");
    }

    assert(pAccProgram);
    uint3 numGroups = div_round_up(uint3(resolution.x, resolution.y, 1u), pAccProgram->getReflector()->getThreadGroupSize());
    mpState->setProgram(pAccProgram);

    pRenderContext->dispatch(mpState.get(), mpVars.get(), numGroups);
    mLastSampleDistanceUniform = sampleDistanceUniform;

    mDirty = false;
}

void AccumulatePass::reset() { 
    mLastSampleDistanceUniform = M_SQRT2;
    mFrameCount = 0;
}

void AccumulatePass::setScene(const Scene::SharedPtr& pScene) {
    if(mpScene == pScene) return;

    mpScene = pScene;
    mpCamera = mpScene ? mpScene->getCamera() : nullptr;

    // Reset accumulation when the scene changes.
    reset();
}

void AccumulatePass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    if(pRenderContext->device() != pScene->device()) {
        LLOG_ERR << "Unable to set scene created on different device!";
        mpScene = nullptr;
    }
    
    setScene(pScene);
}

void AccumulatePass::onHotReload(HotReloadFlags reloaded) {
    // Reset accumulation if programs changed.
    if (is_set(reloaded, HotReloadFlags::Program)) mFrameCount = 0;
}

void AccumulatePass::prepareBuffers(RenderContext* pRenderContext, const Texture::SharedPtr& pSrc, const Texture::SharedPtr& pDepthSrc) {
    if(!mDirty) return;

    // Allocate/resize/clear buffers for intermedate data. These are different depending on accumulation mode.
    // Buffers that are not used in the current mode are released.
    auto prepareBuffer = [&](Texture::SharedPtr& pBuf, uint32_t width, uint32_t height, ResourceFormat format, bool bufUsed, bool clearAsDepth = false) {
        if (!bufUsed) {
            pBuf = nullptr;
            return;
        }

        // (Re-)create buffer if needed.
        if (!pBuf || pBuf->getWidth() != width || pBuf->getHeight() != height) {
            pBuf = Texture::create2D(pRenderContext->device(), width, height, format, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
            assert(pBuf);
        }
        // Clear data if accumulation has been reset (either above or somewhere else).
        if (clearAsDepth) {
            bool isDepth = isDepthStencilFormat(format);
            if(mPixelFilterType == PixelFilterType::Closest) {
                if (isDepth) pRenderContext->clearDsv(pBuf->getDSV().get(), 1.f, 0);
                else pRenderContext->clearUAV(pBuf->getUAV().get(), float4(1.f));
            } else {
                if (isDepth) pRenderContext->clearDsv(pBuf->getDSV().get(), 0.f, 0);
                else pRenderContext->clearUAV(pBuf->getUAV().get(), float4(0.f));
            }
        } else {
            if (getFormatType(format) == FormatType::Float) pRenderContext->clearUAV(pBuf->getUAV().get(), float4(0.f));
            else pRenderContext->clearUAV(pBuf->getUAV().get(), uint4(0));
        }
    };

    // Color
    if(pSrc) {
        uint32_t width = pSrc->getWidth(0);
        uint32_t height = pSrc->getHeight(0);
        ResourceFormat format = pSrc->getFormat();

        // Separable filtering buffers
        prepareBuffer(mpTmpFilteredImage, width, height, format, mDoHorizontalFiltering && mDoVerticalFiltering);
        prepareBuffer(mpFilteredImage, width, height, format, mDoHorizontalFiltering || mDoVerticalFiltering);

        // Accumulation buffers
        prepareBuffer(mpLastFrameSum, width, height, ResourceFormat::RGBA32Float, mPrecisionMode == Precision::Single || mPrecisionMode == Precision::SingleCompensated);
        prepareBuffer(mpLastFrameCorr, width, height, ResourceFormat::RGBA32Float, mPrecisionMode == Precision::SingleCompensated);
        prepareBuffer(mpLastFrameSumLo, width, height, ResourceFormat::RGBA32Uint, mPrecisionMode == Precision::Double);
        prepareBuffer(mpLastFrameSumHi, width, height, ResourceFormat::RGBA32Uint, mPrecisionMode == Precision::Double);
    }

    // Depth
    if(pDepthSrc && (mPixelFilterType == PixelFilterType::Closest || mPixelFilterType == PixelFilterType::Farthest || mPixelFilterType == PixelFilterType::Point)) {
        ResourceFormat format = pDepthSrc->getFormat();
        uint32_t width = pDepthSrc->getWidth(0);
        uint32_t height = pDepthSrc->getHeight(0);

        // Separable filtering buffers
        bool depthRequired = isDepthDependentFilterType(mPixelFilterType);
        prepareBuffer(mpTmpFilteredDepth, width, height, ResourceFormat::R32Float, depthRequired && (mDoHorizontalFiltering && mDoVerticalFiltering), true);
        prepareBuffer(mpFilteredDepth, width, height, ResourceFormat::R32Float, depthRequired && (mDoHorizontalFiltering || mDoVerticalFiltering), true);

        // Depth buffers
        prepareBuffer(mpLastFrameDepth, width, height, ResourceFormat::R32Float, depthRequired, true);
    }
}

void AccumulatePass::preparePixelFilterKernelTexture(RenderContext* pRenderContext) {
    if(!mDirty) return;
    
    auto pDevice = pRenderContext->device();
    uint32_t kernelTextureLUTWidth = 64; // We opt for constant kernel texture size for now ...
    
    bool createHalfTable = true;
    Kernels::NormalizationMode normalization = Kernels::NormalizationMode::Peak;

    static const float gauss_sigma = 2.f;
    static constexpr float mitchell_b = .5f;
    static constexpr float mitchell_c = (1.f - mitchell_b) / 2.f;
    static const float sinc_tau = 3.0f;

    switch(mPixelFilterType) {
        case PixelFilterType::Gaussian: 
            mpKernelTexture = Kernels::Gaussian::createKernelTexture1D(pDevice, kernelTextureLUTWidth, gauss_sigma, createHalfTable, normalization);
            break;
        case PixelFilterType::Blackman:
            mpKernelTexture = Kernels::Blackman::createKernelTexture1D(pDevice, kernelTextureLUTWidth, createHalfTable, normalization);
            break;
        case PixelFilterType::Mitchell:
            mpKernelTexture = Kernels::Mitchell::createKernelTexture1D(pDevice, kernelTextureLUTWidth, mitchell_b, mitchell_c, createHalfTable, normalization);
            break;
        case PixelFilterType::Sinc:
            mpKernelTexture = Kernels::Sinc::createKernelTexture1D(pDevice, kernelTextureLUTWidth, sinc_tau, createHalfTable, normalization);
            break;
        default:
            // Pixel filter does not require LUT texture
            mpKernelTexture = nullptr;
            break;
    }

    if(mpKernelTexture) {
        LLOG_DBG << "Created pixel fitler kernel of type " << to_string(mPixelFilterType) << " with LUT size " << std::to_string(kernelTextureLUTWidth);
    }

    // Create/recreate sampler if needed
    if(!mpKernelSampler && mpKernelTexture) {
        Sampler::Desc desc;
        desc.setMaxAnisotropy(1);
        desc.setLodParams(0.0f, 1000.0f, -0.0f);
        desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
        desc.setAddressingMode(Sampler::AddressMode::MirrorOnce, Sampler::AddressMode::MirrorOnce, Sampler::AddressMode::MirrorOnce);
        //desc.setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap);
        mpKernelSampler = Sampler::create(pDevice, desc);
    }
}

void AccumulatePass::prepareFilteredTextures(const Texture::SharedPtr& pSrc, const Texture::SharedPtr& pDepthSrc) {
    if (!mDirty || !pSrc || (!mDoHorizontalFiltering && !mDoVerticalFiltering)) return;

    auto pDevice = pSrc->device();

    // Filtered spatial color data buffers
    if (mDoHorizontalFiltering || mDoVerticalFiltering ) {
        
        auto format = pSrc->getFormat();
        auto width = pSrc->getWidth(0);
        auto height = pSrc->getHeight(0);

        // Intermediate spatial filtered color data buffer
        if (mDoHorizontalFiltering && mDoVerticalFiltering ) {
            // Create/Recreate itermediate image data buffer
            if(!mpTmpFilteredImage || (mpTmpFilteredImage->getFormat() != format) || (mpTmpFilteredImage->getWidth(0) != width) || (mpTmpFilteredImage->getWidth(0) != height)) {
                mpTmpFilteredImage = Texture::create2D(pDevice, width, height, format, 1, 1, nullptr, 
                    Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
            }
        }

        // Create/Recreate final filtered color data buffer
        if(!mpFilteredImage || (mpFilteredImage->getFormat() != format) || (mpFilteredImage->getWidth(0) != width) || (mpFilteredImage->getWidth(0) == height)) {
            mpFilteredImage = Texture::create2D(pDevice, width, height, format, 1, 1, nullptr, 
                Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
        }
    }
 
    if(pDepthSrc && (mPixelFilterType == PixelFilterType::Closest || mPixelFilterType == PixelFilterType::Farthest)) {
        auto depthFormat = depthToColorFormat(pDepthSrc->getFormat());

        auto depthWidth = pDepthSrc->getWidth(0);
        auto depthHeight = pDepthSrc->getHeight(0);

        // Spatial depth dependent part
        if (mDoHorizontalFiltering || mDoVerticalFiltering ) {

            if(!mpFilteredDepth || (mpFilteredDepth->getFormat() != depthFormat) || (mpFilteredDepth->getWidth(0) != depthWidth) || (mpFilteredDepth->getWidth(0) != depthHeight)) {
                mpFilteredDepth = Texture::create2D(pDevice, depthWidth, depthHeight, depthFormat, 1, 1, nullptr, 
                    Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
            }

            if (mDoHorizontalFiltering && mDoVerticalFiltering ) {
                if(!mpTmpFilteredDepth || (mpTmpFilteredDepth->getFormat() != depthFormat) || (mpTmpFilteredDepth->getWidth(0) != depthWidth) || (mpTmpFilteredDepth->getWidth(0) != depthHeight)) {
                    mpTmpFilteredDepth = Texture::create2D(pDevice, depthWidth, depthHeight, depthFormat, 1, 1, nullptr, 
                        Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
                }
            }
        }
    }
}

void AccumulatePass::prepareImageSampler(RenderContext* pContext) {
    if(!mDirty || mpImageSampler) return;

    Sampler::Desc desc;
    desc.setMaxAnisotropy(1);
    desc.setLodParams(0.0f, 1000.0f, -0.0f);
    desc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    desc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    desc.setUnnormalizedCoordinates(true);
    mpImageSampler = Sampler::create(pContext->device(), desc);
}

void AccumulatePass::setOutputFormat(ResourceFormat format) {
    if(mOutputFormat == format) return;
    mOutputFormat = format;
    mPassChangedCB();
    mDirty = true;
}

void AccumulatePass::setPixelFilterSize(uint2 size) {
    if (mPixelFilterSize == size) return;
    mPixelFilterSize = {std::max(size[0], 1u), std::max(size[1], 1u)};
    mPassChangedCB();
    mDirty = true;
}

void AccumulatePass::setPixelFilterSize(uint32_t width, uint32_t height) {
    setPixelFilterSize({width, height});
}

void AccumulatePass::setPixelFilterType(PixelFilterType type) {
    if (mPixelFilterType == type) return;
    mPixelFilterType = type;
    mPassChangedCB();
    mDirty = true;
}

// Simplified version. We either get 1 or matrix elements count. 
size_t AccumulatePass::pixelFilterSingularValueCountSVD(PixelFilterType pixelFilterType, uint32_t matrixWidth, uint32_t matrixHeight) {
    switch(pixelFilterType) {
        case PixelFilterType::Gaussian:
        case PixelFilterType::Blackman:
        case PixelFilterType::Mitchell:
        case PixelFilterType::Catmullrom:
        case PixelFilterType::Triangle:
        case PixelFilterType::Sinc:
            return matrixWidth * matrixHeight;
        default:
            return 1;
    }
}

void AccumulatePass::setPixelFilterType(const std::string& typeName) {
    if(boost::iequals(typeName, "box")) return setPixelFilterType(PixelFilterType::Box);
    if(boost::iequals(typeName, "point")) return setPixelFilterType(PixelFilterType::Point);
    if(boost::iequals(typeName, "triangle")) return setPixelFilterType(PixelFilterType::Triangle);
    if(boost::iequals(typeName, "gaussian")) return setPixelFilterType(PixelFilterType::Gaussian);
    if(boost::iequals(typeName, "blackman")) return setPixelFilterType(PixelFilterType::Blackman);
    if(boost::iequals(typeName, "mitchell")) return setPixelFilterType(PixelFilterType::Mitchell);
    if(boost::iequals(typeName, "sinc")) return setPixelFilterType(PixelFilterType::Sinc);
    if(boost::iequals(typeName, "closest")) return setPixelFilterType(PixelFilterType::Closest);
    if(boost::iequals(typeName, "farthest")) return setPixelFilterType(PixelFilterType::Farthest);
    if(boost::iequals(typeName, "min")) return setPixelFilterType(PixelFilterType::Min);
    if(boost::iequals(typeName, "max")) return setPixelFilterType(PixelFilterType::Max);
    if(boost::iequals(typeName, "additive")) return setPixelFilterType(PixelFilterType::Additive);
}