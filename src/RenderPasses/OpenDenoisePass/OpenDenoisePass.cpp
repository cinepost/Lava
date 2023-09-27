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
#include "Utils/Color/ColorUtils.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"

#include "OpenDenoisePass.h"


const RenderPass::Info OpenDenoisePass::kInfo {
    "OpenDenoisePass",

    "Denoise image using Intel Open Image Denoiser.\n"
    ""
};

namespace {
    const std::string kInput = "input";
    const std::string kOutput = "output";

    const std::string kAlbedoInput = "albedo";
    const std::string kNormalInput = "normal";

    const std::string kOutputFormat = "outputFormat";
    const std::string kUseAlbedo = "useAlbedo";
    const std::string kUseNormal = "useNormal";
    const std::string kQuality   = "quality";

    const ChannelList kExtraInputChannels = {
        { kAlbedoInput,           "gTextureAlbedo",            "Albedo auxiliary texture", true /* optional */, ResourceFormat::Unknown },
        { kNormalInput,           "gTextureNormal",            "Normal auxiliary texture", true /* optional */, ResourceFormat::Unknown },
    };

    inline oidn::Format toOIDNFormat(ResourceFormat format) {
        bool isHalf = isHalfFloatFormat(format);
        bool isFloat = isFloatFormat(format) && !isHalf;
        if(!isFloat && !isHalf) return oidn::Format::Undefined;

        auto channelCount = getFormatChannelCount(format);
        switch (channelCount) {
            case 1:
                return isFloat ? oidn::Format::Float : oidn::Format::Half;
            case 2:
                return isFloat ? oidn::Format::Float2 : oidn::Format::Half2;
            case 3:
                return isFloat ? oidn::Format::Float3 : oidn::Format::Half3;
            case 4:
                return isFloat ? oidn::Format::Float4 : oidn::Format::Half4;
            default:
                break;
        }
        return oidn::Format::Undefined;
    }

#define str(a) case oidn::Format::a: return #a
    inline std::string to_string(oidn::Format type) {
        switch (type) {
            str(Float);
            str(Float2);
            str(Float3);
            str(Float4);
            str(Half);
            str(Half2);
            str(Half3);
            str(Half4);
        default:
            should_not_get_here();
            return "oidn::Format::Undefined";
        }
    }
#undef str

}

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(OpenDenoisePass::kInfo, OpenDenoisePass::create);
}

OpenDenoisePass::OpenDenoisePass(Device::SharedPtr pDevice, ResourceFormat outputFormat) : RenderPass(pDevice, kInfo), mOutputFormat(outputFormat) {
    mOidnDevice = oidn::newDevice(oidn::DeviceType::CPU);
    mOidnDevice.commit();
}

OpenDenoisePass::SharedPtr OpenDenoisePass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    // outputFormat can only be set on construction
    ResourceFormat outputFormat = ResourceFormat::Unknown;
    if (dict.keyExists(kOutputFormat)) outputFormat = dict[kOutputFormat];

    OpenDenoisePass* pThis = new OpenDenoisePass(pRenderContext->device(), outputFormat);

    pThis->parseDictionary(dict);

    return OpenDenoisePass::SharedPtr(pThis);
}

Dictionary OpenDenoisePass::getScriptingDictionary() {
    Dictionary d;
    return d;
}

void OpenDenoisePass::parseDictionary(const Dictionary& dict) {
    for (const auto& [key, value] : dict) {
        if (key == kOutputFormat) setOutputFormat(value);
        else if (key == kUseAlbedo) useAlbedo(static_cast<bool>(value));
        else if (key == kUseNormal) useNormal(static_cast<bool>(value));
        else if (key == kQuality) setQuality(static_cast<OpenDenoisePass::Quality>(value));
    }
}

RenderPassReflection OpenDenoisePass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    
    const auto& texDims = compileData.defaultTexDims;

    reflector.addInput(kInput, "Color buffer").format(ResourceFormat::Unknown);
    reflector.addOutput(kOutput, "Denoised color buffer").format(mOutputFormat);
    
    addRenderPassInputs(reflector, kExtraInputChannels);

    return reflector;
}

void OpenDenoisePass::compile(RenderContext* pRenderContext, const CompileData& compileData) {
    mFrameDim = compileData.defaultTexDims;
}

void OpenDenoisePass::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    auto pInputResource = renderData[kInput];
    auto pOutputResource = renderData[kOutput];

    if(!pInputResource || !pOutputResource) {
        LLOG_ERR << "No input or output resource specified for OpenDenoisePass!";
        return;
    }

    auto pInputTex = pInputResource->asTexture();
    auto pOutputTex = pOutputResource->asTexture();

    if(pInputTex->getWidth() != pOutputTex->getWidth() || pInputTex->getHeight() != pOutputTex->getHeight()) {
        LLOG_ERR << "OpenDenoisePass input and output images dimensions mismatch!";
        bypass(pRenderContext, renderData);
        return;
    }
    
    auto pAlbedoTex = renderData[kAlbedoInput]->asTexture();
    auto pNormalTex = renderData[kNormalInput]->asTexture();

    auto inputImageFormat = pInputTex->getFormat();
    auto outputImageFormat = pOutputTex->getFormat();
    oidn::Format inputImageOIDNFormat = toOIDNFormat(inputImageFormat);
    oidn::Format outputImageOIDNFormat = toOIDNFormat(outputImageFormat);
    uint32_t inputImageDataSize = pInputTex->getWidth(0) * pInputTex->getHeight(0) * getFormatBytesPerBlock(inputImageFormat);
    uint32_t outputImageDataSize = pOutputTex->getWidth(0) * pOutputTex->getHeight(0) * getFormatBytesPerBlock(outputImageFormat);
    
    mMainImageData.resize(inputImageDataSize);
    mOutputImageData.resize(outputImageDataSize);

    auto auxAlbedoImageFormat = ResourceFormat::Unknown;
    oidn::Format auxAlbedoOIDNFormat = oidn::Format::Undefined;
    if(pAlbedoTex && mUseAlbedo) {
        if(pInputTex->getWidth() != pAlbedoTex->getWidth() || pInputTex->getHeight() != pAlbedoTex->getHeight()) {
            LLOG_ERR << "OpenDenoisePass input and albedo images dimensions mismatch!";
            bypass(pRenderContext, renderData);
            return;
        }

        auxAlbedoOIDNFormat = toOIDNFormat(auxAlbedoImageFormat);
        mAlbedoImageData.resize(pAlbedoTex->getWidth(0) * pAlbedoTex->getHeight(0) * getFormatBytesPerBlock(auxAlbedoImageFormat));
        pRenderContext->readTextureSubresource(pAlbedoTex.get(), pAlbedoTex->getSubresourceIndex(0, 0), mAlbedoImageData.data());
    }

    auto auxNormalImageFormat = ResourceFormat::Unknown;
    oidn::Format auxNormalOIDNFormat = oidn::Format::Undefined;
    if(pNormalTex && mUseNormal) {
        if(pInputTex->getWidth() != pNormalTex->getWidth() || pInputTex->getHeight() != pNormalTex->getHeight()) {
            LLOG_ERR << "OpenDenoisePass input and normal images dimensions mismatch!";
            bypass(pRenderContext, renderData);
            return;
        }

        auxNormalOIDNFormat = toOIDNFormat(auxNormalImageFormat);
        mNormalImageData.resize(pNormalTex->getWidth(0) * pNormalTex->getHeight(0) * getFormatBytesPerBlock(auxNormalImageFormat));
        pRenderContext->readTextureSubresource(pNormalTex.get(), pNormalTex->getSubresourceIndex(0, 0), mNormalImageData.data());
    }

    if(!mFilter) {
        mFilter = mOidnDevice.newFilter("RT");
        mFilter.set("quality", (mQuality == Quality::High) ? OIDN_QUALITY_HIGH : OIDN_QUALITY_BALANCED);
    }

    size_t inputImageDataByteOffset = 0;
    size_t inputImageBytePixelStride = getFormatBytesPerBlock(inputImageFormat);

    if(inputImageOIDNFormat == oidn::Format::Half4) inputImageOIDNFormat = oidn::Format::Half3;
    if(inputImageOIDNFormat == oidn::Format::Float4) inputImageOIDNFormat = oidn::Format::Float3;

    pRenderContext->readTextureSubresource(pInputTex.get(), pInputTex->getSubresourceIndex(0, 0), mMainImageData.data());
    
    // Copy input data to output vector. We need this to keep our source image alpha channel;
    // TODO: find a better faster way without full copy
    memcpy(&mOutputImageData[0], &mMainImageData[0], mMainImageData.size());

    mFilter.setImage("color",  mMainImageData.data(),  inputImageOIDNFormat, mFrameDim.x, mFrameDim.y, 
        inputImageDataByteOffset, inputImageBytePixelStride); // beauty
    
    if(pAlbedoTex && mUseAlbedo) {
        LLOG_DBG << "Denosing image using \"albedo\" auxiliary channel";
        size_t albedoImageDataByteOffset = 0;
        size_t albedoImageBytePixelStride = getFormatBytesPerBlock(auxAlbedoImageFormat);
        mFilter.setImage("albedo", mAlbedoImageData.data(), auxAlbedoOIDNFormat, mFrameDim.x, mFrameDim.y, 
            albedoImageDataByteOffset, albedoImageBytePixelStride); // auxiliary
    }

    if(pNormalTex && mUseNormal) {
        LLOG_DBG << "Denosing image using \"normal\" auxiliary channel";
        size_t normalImageDataByteOffset = 0;
        size_t normalImageBytePixelStride = getFormatBytesPerBlock(auxNormalImageFormat);
        mFilter.setImage("normal", mNormalImageData.data(), auxNormalOIDNFormat, mFrameDim.x, mFrameDim.y, 
            normalImageDataByteOffset, normalImageBytePixelStride); // auxiliary
    }

    size_t outputImageDataByteOffset = 0;
    size_t outputImageBytePixelStride = getFormatBytesPerBlock(outputImageFormat);

    if(outputImageOIDNFormat == oidn::Format::Half4) outputImageOIDNFormat = oidn::Format::Half3;
    if(outputImageOIDNFormat == oidn::Format::Float4) outputImageOIDNFormat = oidn::Format::Float3;

    mFilter.setImage("output", mOutputImageData.data(), outputImageOIDNFormat, mFrameDim.x, mFrameDim.y, 
        outputImageDataByteOffset, outputImageBytePixelStride); // denoised beauty

    if(!mDisableHDRInput && (isFloatFormat(inputImageFormat) || isHalfFloatFormat(inputImageFormat))) {
        mFilter.set("hdr", true); // MAIN (beauty) image is HDR
    }

    mFilter.commit();

    // Filter the image
    mFilter.execute();

    // Check for errors
    bool hasErrors = false;
    const char* errorMessage;
    if (mOidnDevice.getError(errorMessage) != oidn::Error::None) {
        bool hasErrors = true;
        LLOG_ERR << "OpenDenoisePass error: " << std::string(errorMessage);
        
        bypass(pRenderContext, renderData);
        return;
    }

    LLOG_DBG << "OpenDenoisePass filter executed. Uploading denoised data.";

    // Upload denoised image back to GPU
    pRenderContext->updateTextureData(pOutputTex.get(), (const void*)mOutputImageData.data());
}

void OpenDenoisePass::bypass(RenderContext* pRenderContext, const RenderData& renderData) {
    LLOG_DBG << "OpenDenoisePass bypass.";

    auto pInputTex = renderData[kInput]->asTexture();
    auto pOutputTex = renderData[kOutput]->asTexture();

    pRenderContext->blit(pInputTex->getSRV(0, 1, 0, 1), pOutputTex->getRTV(0, 0, 1));
}

void OpenDenoisePass::disableHDRInput(bool value) {
    if (mDisableHDRInput == value) return;
    mDisableHDRInput = value;
    mPassChangedCB();
}

void OpenDenoisePass::setOutputFormat(ResourceFormat format) {
    if(mOutputFormat == format) return;
    mOutputFormat = format;
    mPassChangedCB();
}

void OpenDenoisePass::useAlbedo(bool state) {
    if(mUseAlbedo == state) return;
    mUseAlbedo = state;
    mPassChangedCB();
}

void OpenDenoisePass::useNormal(bool state) {
    if(mUseNormal == state) return;
    mUseNormal = state;
    mPassChangedCB();
}

void OpenDenoisePass::setQuality(Quality quality) {
    if(mQuality == quality) return;
    mQuality = quality;
    mPassChangedCB();
}