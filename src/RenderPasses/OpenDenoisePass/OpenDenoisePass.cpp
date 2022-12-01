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

    const ChannelList kExtraInputChannels = {
        { kAlbedoInput,           "gTextureAlbedo",            "Albedo auxiliary texture", true /* optional */, ResourceFormat::Unknown },
        { kNormalInput,           "gTextureNormal",            "Normal auxiliary texture", true /* optional */, ResourceFormat::Unknown },
    };

    inline oidn::Format toOIDNFormat(ResourceFormat format) {
        bool isFloat = isFloatFormat(format), isHalf = isHalfFloatFormat(format);
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

}

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(OpenDenoisePass::kInfo, OpenDenoisePass::create);
}

OpenDenoisePass::OpenDenoisePass(Device::SharedPtr pDevice, ResourceFormat outputFormat) : RenderPass(pDevice, kInfo), mOutputFormat(outputFormat) {
    mIntelDevice = oidn::newDevice();
    mIntelDevice.commit();
}

OpenDenoisePass::SharedPtr OpenDenoisePass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    // outputFormat can only be set on construction
    ResourceFormat outputFormat = ResourceFormat::Unknown;
    if (dict.keyExists(kOutputFormat)) outputFormat = dict[kOutputFormat];

    OpenDenoisePass* pThis = new OpenDenoisePass(pRenderContext->device(), outputFormat);

    for (const auto& [key, value] : dict) {
    
    }

    return OpenDenoisePass::SharedPtr(pThis);
}

Dictionary OpenDenoisePass::getScriptingDictionary() {
    Dictionary d;
    return d;
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
    auto pInputTex = renderData[kInput]->asTexture();
    auto pOutputTex = renderData[kOutput]->asTexture();
    
    auto pAlbedoTex = renderData[kAlbedoInput]->asTexture();
    auto pNormalTex = renderData[kNormalInput]->asTexture();

    auto mainImageFormat = pInputTex->getFormat();
    uint32_t mainImageDataSize = pInputTex->getWidth(0) * pInputTex->getHeight(0) * getFormatBytesPerBlock(mainImageFormat);
    mMainImageData.resize(mainImageDataSize);
    mOutputImageData.resize(mainImageDataSize);

    auto auxAlbedoFormat = ResourceFormat::Unknown;
    if(pAlbedoTex) {
        mAlbedoImageData.resize(pAlbedoTex->getWidth(0) * pAlbedoTex->getHeight(0) * getFormatBytesPerBlock(mainImageFormat));
    }

    auto auxNormalFormat = ResourceFormat::Unknown;
    if(pNormalTex) {
        mNormalImageData.resize(pNormalTex->getWidth(0) * pNormalTex->getHeight(0) * getFormatBytesPerBlock(mainImageFormat));
    }

    oidn::FilterRef filter = mIntelDevice.newFilter("RT");

    filter.setImage("color",  mMainImageData.data(),  oidn::Format::Float3, mFrameDim.x, mFrameDim.y); // beauty
    
    if(pAlbedoTex) {
        LLOG_DBG << "Denosing image using \"albedo\" auxiliary channel";
        filter.setImage("albedo", mAlbedoImageData.data(), oidn::Format::Float3, mFrameDim.x, mFrameDim.y); // auxiliary
    }

    if(pNormalTex) {
        LLOG_DBG << "Denosing image using \"normal\" auxiliary channel";
        filter.setImage("normal", mNormalImageData.data(), oidn::Format::Float3, mFrameDim.x, mFrameDim.y); // auxiliary
    }

    filter.setImage("output", mOutputImageData.data(), oidn::Format::Float3, mFrameDim.x, mFrameDim.y); // denoised beauty

    if(isFloatFormat(mainImageFormat) || isHalfFloatFormat(mainImageFormat)) {
        filter.set("hdr", true); // MAIN (beauty) image is HDR
    }

    filter.commit();

    // Filter the image
    filter.execute();

    // Check for errors
    bool hasErrors = false;
    const char* errorMessage;
    if (mIntelDevice.getError(errorMessage) != oidn::Error::None) {
        bool hasErrors = true;
        LLOG_ERR << "OpenDenoisePass error: " << std::string(errorMessage);
    }
}


void OpenDenoisePass::setOutputFormat(ResourceFormat format) {
    if(mOutputFormat == format) return;
    mOutputFormat = format;
    mPassChangedCB();
}
