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

    const char kInputChannel[] = "input";
    const char kOutputChannel[] = "output";
    const char kInputDepthChannel[] = "depth";

    const ChannelList kAccumulatePassExtraInputChannels = {
        { kInputDepthChannel,  "gDepth",  "Depth buffer",    true /* optional */, ResourceFormat::Unknown },
    };

    // Serialized parameters
    const char kEnableAccumulation[] = "enableAccumulation";
    const char kAutoReset[] = "autoReset";
    const char kPrecisionMode[] = "precisionMode";
    const char kSubFrameCount[] = "subFrameCount";
}

static float hMinDbg = 1.f;
static float hMaxDbg = 0.f;
static float vMinDbg = 1.f;
static float vMaxDbg = 0.f;

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

    setPixelFilterType(PixelFilterType::Box);
    setPixelFilterSize({1u, 1u});

    // Create accumulation programs.
    // Note only compensated summation needs precise floating-point mode.
    mpProgram[Precision::Double] = ComputeProgram::createFromFile(pDevice, kShaderFile, "accumulateDouble", Program::DefineList(), Shader::CompilerFlags::TreatWarningsAsErrors);
    mpProgram[Precision::Single] = ComputeProgram::createFromFile(pDevice, kShaderFile, "accumulateSingle", Program::DefineList(), Shader::CompilerFlags::TreatWarningsAsErrors);
    mpProgram[Precision::SingleCompensated] = ComputeProgram::createFromFile(pDevice, kShaderFile, "accumulateSingleCompensated", Program::DefineList(), Shader::CompilerFlags::FloatingPointModePrecise | Shader::CompilerFlags::TreatWarningsAsErrors);
    
    // Create filtering programs.
    mFilterPasses[0].pProgram = ComputeProgram::createFromFile(pDevice, kFilterFile, "filterH", Program::DefineList(), Shader::CompilerFlags::TreatWarningsAsErrors);
    mFilterPasses[1].pProgram = ComputeProgram::createFromFile(pDevice, kFilterFile, "filterV", Program::DefineList(), Shader::CompilerFlags::TreatWarningsAsErrors);

    for(auto& [key, pProgram]: mpProgram) {
        pProgram->addDefine("is_valid_gDepth", "0");
    }

    mpVars = ComputeVars::create(pDevice, mpProgram[mPrecisionMode]->getReflector());
    mpState = ComputeState::create(pDevice);

    mFilterPasses[0].pVars = ComputeVars::create(pDevice, mFilterPasses[0].pProgram->getReflector());
    mFilterPasses[1].pVars = ComputeVars::create(pDevice, mFilterPasses[1].pProgram->getReflector());

    mFilterPasses[0].pState = ComputeState::create(pDevice);
    mFilterPasses[1].pState = ComputeState::create(pDevice);
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

    Texture::SharedPtr pDepth = renderData[kInputDepthChannel]->asTexture();

    assert(pSrc && pDst);
    assert(pSrc->getWidth() == pDst->getWidth() && pSrc->getHeight() == pDst->getHeight());
    const uint2 resolution = uint2(pSrc->getWidth(), pSrc->getHeight());


    // Setup filtering passes if needed.
    Texture::SharedPtr pFilteredImage = pSrc;

    preparePixelFilterKernelTexture(pRenderContext);

    // Look... For now we use fixed size filter kernel 1D texture used for both vertical and horizontal filtering with fixed 64 elements size
    if(mPixelFilterSize[0] > 1u || 
        (mpKernelTexture && pixelFilterSingularValueCountSVD(mPixelFilterType, kPixelFilterKernelTextureHalfSize, kPixelFilterKernelTextureHalfSize))) mDoHorizontalFiltering = true;
    if(mPixelFilterSize[1] > 1u || 
        (mpKernelTexture && pixelFilterSingularValueCountSVD(mPixelFilterType, kPixelFilterKernelTextureHalfSize, kPixelFilterKernelTextureHalfSize))) mDoVerticalFiltering = true;
    
    prepareFilteredTextures(pSrc);
    prepareImageSampler(pRenderContext);

    float2 maxSampleDistance = {float(mPixelFilterSize[0])*0.5, float(mPixelFilterSize[1])*0.5};
    uint2 halfFilterSize = {mPixelFilterSize[0] >> 1, mPixelFilterSize[1] >> 1};
    float2 sampleDistanceLocal = mpCamera ? float2({mpCamera->getSubpixelJitterX(), mpCamera->getSubpixelJitterY()}) : float2({0.f, 0.f});

    LLOG_DBG << "AccumulatePass maximum sample distance " << to_string(maxSampleDistance);
    LLOG_DBG << "AccumulatePass filter size " << to_string(mPixelFilterSize) << ". Half size " << to_string(halfFilterSize);
    LLOG_DBG << "AccumulatePass sample local distance " << to_string(sampleDistanceLocal);

    // Horizontal filtering pass
    if(mDoHorizontalFiltering) {
        auto& filterPass = mFilterPasses[0];

        auto pProgram = filterPass.pProgram;
        pProgram->addDefine("is_valid_gKernelTexture", mpKernelTexture ? "1" : "0");
        pProgram->addDefine("_FILTER_HALF_SIZE_H", std::to_string(halfFilterSize[0]));
        
        auto pVars = filterPass.pVars;
        pVars["PerFrameCB"]["gResolution"] = resolution;
        pVars["PerFrameCB"]["gKernelTextureSize"] = mpKernelTexture ? uint2({mpKernelTexture->getWidth(0), mpKernelTexture->getHeight(0)}) : uint2({0, 0});
        pVars["PerFrameCB"]["gSampleDistanceLocal"] = sampleDistanceLocal;
        pVars["PerFrameCB"]["gFilterSize"] = mPixelFilterSize;
        pVars["PerFrameCB"]["gMaxSampleDistance"] = maxSampleDistance;
        pVars["gKernelTexture"] = mpKernelTexture;
        pVars["gKernelSampler"] = mpKernelSampler;
        pVars["gImageSampler"] = mpImageSampler;
        pVars["gInputSamplePlane"] = pSrc;

        pFilteredImage = mDoVerticalFiltering ? mpTmpFilteredImage : mpFilteredImage;
        pVars["gOutputSamplePlane"] = pFilteredImage;

        filterPass.pState->setProgram(pProgram);

        uint3 numGroupsFH = div_round_up(uint3(resolution.x, resolution.y, 1u), mFilterPasses[0].pProgram->getReflector()->getThreadGroupSize());    
        pRenderContext->dispatch(mFilterPasses[0].pState.get(), mFilterPasses[0].pVars.get(), numGroupsFH);
    }
    
    // Horizontal filtering pass
    if(mDoVerticalFiltering) {
        auto& filterPass = mFilterPasses[1];

        auto pProgram = filterPass.pProgram;
        pProgram->addDefine("is_valid_gKernelTexture", mpKernelTexture ? "1" : "0");
        pProgram->addDefine("_FILTER_HALF_SIZE_V", std::to_string(halfFilterSize[1]));

        auto pVars = filterPass.pVars;
        pVars["PerFrameCB"]["gResolution"] = resolution;
        pVars["PerFrameCB"]["gKernelTextureSize"] = mpKernelTexture ? uint2({mpKernelTexture->getWidth(0), mpKernelTexture->getHeight(0)}) : uint2({0, 0});
        pVars["PerFrameCB"]["gSampleDistanceLocal"] = sampleDistanceLocal;
        pVars["PerFrameCB"]["gFilterSize"] = mPixelFilterSize;
        pVars["PerFrameCB"]["gMaxSampleDistance"] = maxSampleDistance;
        pVars["gKernelTexture"] = mpKernelTexture;
        pVars["gKernelSampler"] = mpKernelSampler;
        pVars["gImageSampler"] = mpImageSampler;
        pVars["gInputSamplePlane"] = mDoHorizontalFiltering ? mpTmpFilteredImage : pSrc;

        pFilteredImage = mpFilteredImage;
        pVars["gOutputSamplePlane"] = pFilteredImage;

        filterPass.pState->setProgram(pProgram);

        uint3 numGroupsFV = div_round_up(uint3(resolution.x, resolution.y, 1u), mFilterPasses[1].pProgram->getReflector()->getThreadGroupSize());
        pRenderContext->dispatch(mFilterPasses[1].pState.get(), mFilterPasses[1].pVars.get(), numGroupsFV);
    }

    // If accumulation is disabled, just blit the source to the destination and return.
    if (!mEnableAccumulation) {
        // Only blit mip 0 and array slice 0, because that's what the accumulation uses otherwise otherwise.
        pRenderContext->blit(pFilteredImage->getSRV(0, 1, 0, 1), pDst->getRTV(0, 0, 1));
        return;
    }

    
    // Setup accumulation.
    prepareAccumulation(pRenderContext, resolution.x, resolution.y);

    // Set shader parameters.
    mpVars["PerFrameCB"]["gResolution"] = resolution;
    mpVars["PerFrameCB"]["gAccumCount"] = mFrameCount++;
    mpVars["gCurFrame"] = pFilteredImage;
    mpVars["gOutputFrame"] = pDst;

    // Additional channels /samplers
    mpVars["gDepth"] = pDepth;

    // Bind accumulation buffers. Some of these may be nullptr's.
    mpVars["gLastFrameSum"] = mpLastFrameSum;
    mpVars["gLastFrameCorr"] = mpLastFrameCorr;
    mpVars["gLastFrameSumLo"] = mpLastFrameSumLo;
    mpVars["gLastFrameSumHi"] = mpLastFrameSumHi;

    // Run the accumulation program.
    auto pAccProgram = mpProgram[mPrecisionMode];

    // For optional I/O resources, set 'is_valid_<name>' defines to inform the program of which ones it can access.
    // TODO: This should be moved to a more general mechanism using Slang.
    if(mFrameCount == 0) {
        pAccProgram->addDefines(getValidResourceDefines(kAccumulatePassExtraInputChannels, renderData));
    }

    assert(pAccProgram);
    uint3 numGroups = div_round_up(uint3(resolution.x, resolution.y, 1u), pAccProgram->getReflector()->getThreadGroupSize());
    mpState->setProgram(pAccProgram);

    mDirty = false;

    pRenderContext->dispatch(mpState.get(), mpVars.get(), numGroups);

}

void AccumulatePass::setScene(const Scene::SharedPtr& pScene) {
    // Reset accumulation when the scene changes.
    mFrameCount = 0;
    mpScene = pScene;

    if(mpScene) mpCamera = mpScene->getCamera();
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

void AccumulatePass::prepareAccumulation(RenderContext* pRenderContext, uint32_t width, uint32_t height) {
    // Allocate/resize/clear buffers for intermedate data. These are different depending on accumulation mode.
    // Buffers that are not used in the current mode are released.
    auto prepareBuffer = [&](Texture::SharedPtr& pBuf, ResourceFormat format, bool bufUsed) {
        if (!bufUsed) {
            pBuf = nullptr;
            return;
        }
        // (Re-)create buffer if needed.
        if (!pBuf || pBuf->getWidth() != width || pBuf->getHeight() != height) {
            pBuf = Texture::create2D(pRenderContext->device(), width, height, format, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
            assert(pBuf);
            mFrameCount = 0;
        }
        // Clear data if accumulation has been reset (either above or somewhere else).
        if (mFrameCount == 0) {
            if (getFormatType(format) == FormatType::Float) pRenderContext->clearUAV(pBuf->getUAV().get(), float4(0.f));
            else pRenderContext->clearUAV(pBuf->getUAV().get(), uint4(0));
        }
    };

    prepareBuffer(mpLastFrameSum, ResourceFormat::RGBA32Float, mPrecisionMode == Precision::Single || mPrecisionMode == Precision::SingleCompensated);
    prepareBuffer(mpLastFrameCorr, ResourceFormat::RGBA32Float, mPrecisionMode == Precision::SingleCompensated);
    prepareBuffer(mpLastFrameSumLo, ResourceFormat::RGBA32Uint, mPrecisionMode == Precision::Double);
    prepareBuffer(mpLastFrameSumHi, ResourceFormat::RGBA32Uint, mPrecisionMode == Precision::Double);
}

void AccumulatePass::preparePixelFilterKernelTexture(RenderContext* pRenderContext) {
    if(!mDirty || !mpFilterCreateKernelTextureFunc) return;
    
    auto pDevice = pRenderContext->device();
    uint32_t kernelTextureLUTWidth = 64; // We opt for constant kernel texture size for now ...
    
    LLOG_DBG << "Creating pixel fitler kernel of type " << to_string(mPixelFilterType) << " with LUT size " << std::to_string(kernelTextureLUTWidth);
    bool createHalfWidthTable = true;
    mpKernelTexture = mpFilterCreateKernelTextureFunc(pDevice, kernelTextureLUTWidth, createHalfWidthTable);

    // Create/recreate sampler if needed
    if(!mpKernelSampler) {
        Sampler::Desc desc;
        desc.setMaxAnisotropy(1);
        desc.setLodParams(0.0f, 1000.0f, -0.0f);
        desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
        desc.setAddressingMode(Sampler::AddressMode::MirrorOnce, Sampler::AddressMode::MirrorOnce, Sampler::AddressMode::MirrorOnce);
        mpKernelSampler = Sampler::create(pDevice, desc);
    }
}

void AccumulatePass::prepareFilteredTextures(const Texture::SharedPtr& pSrc) {
    if (!mDirty || !pSrc || (!mDoHorizontalFiltering && !mDoVerticalFiltering)) return;
    
    auto srcFormat = pSrc->getFormat();
    auto srcWidth = pSrc->getWidth(0);
    auto srcHeight = pSrc->getHeight(0);

    auto pDevice = pSrc->device();

    if (mDoHorizontalFiltering && mDoVerticalFiltering ) {
        if(!mpTmpFilteredImage || mpTmpFilteredImage->getFormat() != srcFormat && mpTmpFilteredImage->getWidth(0) != srcWidth && mpTmpFilteredImage->getWidth(0) != srcHeight) {
            mpTmpFilteredImage = Texture::create2D(pDevice, srcWidth, srcHeight, srcFormat, 1, 1, nullptr, 
                Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
        }
    }

    if(!mpFilteredImage || mpFilteredImage->getFormat() != srcFormat && mpFilteredImage->getWidth(0) != srcWidth || mpFilteredImage->getWidth(0) == srcHeight) {
        mpFilteredImage = Texture::create2D(pDevice, srcWidth, srcHeight, srcFormat, 1, 1, nullptr, 
            Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
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

    switch(mPixelFilterType) {
        case PixelFilterType::Gaussian:
            mpFilterCreateKernelTextureFunc = Gaussian::createKernelTexture1D;
            break;
        case PixelFilterType::Blackman:
            mpFilterCreateKernelTextureFunc = Blackman::createKernelTexture1D;
            break;
        case PixelFilterType::Mitchell:
            mpFilterCreateKernelTextureFunc = Gaussian::createKernelTexture1D;
            break;
        default:
            // Pixel filter does not require LUT texture
            mpKernelTexture = nullptr;
            break;
    }

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
    if(boost::iequals(typeName, "gaussian")) return setPixelFilterType(PixelFilterType::Gaussian);
    if(boost::iequals(typeName, "blackman")) return setPixelFilterType(PixelFilterType::Blackman);
    if(boost::iequals(typeName, "mitchell")) return setPixelFilterType(PixelFilterType::Mitchell);
    if(boost::iequals(typeName, "closest")) return setPixelFilterType(PixelFilterType::Closest);
    if(boost::iequals(typeName, "farthest")) return setPixelFilterType(PixelFilterType::Farthest);
    if(boost::iequals(typeName, "min")) return setPixelFilterType(PixelFilterType::Min);
    if(boost::iequals(typeName, "max")) return setPixelFilterType(PixelFilterType::Max);
    if(boost::iequals(typeName, "additive")) return setPixelFilterType(PixelFilterType::Additive);
}