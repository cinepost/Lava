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

#include "ToneMapperPass.h"


const RenderPass::Info ToneMapperPass::kInfo
{
    "ToneMapperPass",

    "Computes Tone-mapped image.\n"
    ""
};

namespace {
    const std::string kSrc = "input";
    const std::string kDst = "output";
    const std::string kLuminanceTex = "luminanceTex";

    const std::string kOutputFormat = "outputFormat";

    const std::string kExposureCompensation = "exposureCompensation";
    const std::string kAutoExposure = "autoExposure";
    const std::string kExposureValue = "exposureValue";
    const std::string kFilmSpeed = "filmSpeed";

    const std::string kWhiteBalance = "whiteBalance";
    const std::string kWhitePoint = "whitePoint";

    const std::string kOperator = "operator";
    const std::string kClamp = "clamp";
    const std::string kWhiteMaxLuminance = "whiteMaxLuminance";
    const std::string kWhiteScale = "whiteScale";

    const char kLuminanceShaderFile[] = "RenderPasses/ToneMapperPass/Luminance.cs.slang";
    const char kToneMappingShaderFile[] = "RenderPasses/ToneMapperPass/ToneMapping.cs.slang";

    const std::string kShaderModel = "6_5";

    const float kExposureCompensationMin = -12.f;
    const float kExposureCompensationMax = 12.f;

    const float kExposureValueMin = -24.f;
    const float kExposureValueMax = 24.f;

    const float kFilmSpeedMin = 1.f;
    const float kFilmSpeedMax = 6400.f;

    // Note: Color temperatures < ~1905K are out-of-gamut in Rec.709.
    const float kWhitePointMin = 1905.f;
    const float kWhitePointMax = 25000.f;
}

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(ToneMapperPass::kInfo, ToneMapperPass::create);
}

ToneMapperPass::ToneMapperPass(Device::SharedPtr pDevice, ToneMapperPass::Operator op, ResourceFormat outputFormat) : RenderPass(pDevice, kInfo), mOperator(op), mOutputFormat(outputFormat) {
    createLuminancePass(pDevice);
    createToneMapPass(pDevice);

    updateWhiteBalanceTransform();

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpPointSampler = Sampler::create(pDevice, samplerDesc);
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
    mpLinearSampler = Sampler::create(pDevice, samplerDesc);
}

ToneMapperPass::SharedPtr ToneMapperPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    // outputFormat can only be set on construction
    ResourceFormat outputFormat = ResourceFormat::Unknown;
    auto pThis = SharedPtr(new ToneMapperPass(pRenderContext->device(), Operator::HableUc2, ResourceFormat::Unknown));

    for (const auto& [key, value] : dict) {
        if (key == kExposureCompensation) pThis->setExposureCompensation(value);
        else if (key == kAutoExposure) pThis->setAutoExposure(value);
        else if (key == kExposureValue) pThis->setExposureValue(value);
        else if (key == kFilmSpeed) pThis->setFilmSpeed(value);
        else if (key == kWhiteBalance) pThis->setWhiteBalance(value);
        else if (key == kWhitePoint) pThis->setWhitePoint(value);
        else if (key == kOperator) pThis->setOperator(value);
        else if (key == kClamp) pThis->setClamp(value);
        else if (key == kWhiteMaxLuminance) pThis->setWhiteMaxLuminance(value);
        else if (key == kWhiteScale) pThis->setWhiteScale(value);
        else if (key == kOutputFormat) pThis->setOutputFormat(value);
    }

    return pThis;
}

Dictionary ToneMapperPass::getScriptingDictionary() {
    Dictionary d;
    if (mOutputFormat != ResourceFormat::Unknown) d[kOutputFormat] = mOutputFormat;
    d[kExposureCompensation] = mExposureCompensation;
    d[kAutoExposure] = mAutoExposure;
    d[kExposureValue] = mExposureValue;
    d[kFilmSpeed] = mFilmSpeed;
    d[kWhiteBalance] = mWhiteBalance;
    d[kWhitePoint] = mWhitePoint;
    d[kOperator] = mOperator;
    d[kClamp] = mClamp;
    d[kWhiteMaxLuminance] = mWhiteMaxLuminance;
    d[kWhiteScale] = mWhiteScale;
    return d;
}

RenderPassReflection ToneMapperPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    reflector.addInput(kSrc, "Source texture");
    auto& output = reflector.addOutput(kDst, "Tone-mapped output texture");
    
    reflector.addInternal(kLuminanceTex, "Luminance texture");

    if (mOutputFormat != ResourceFormat::Unknown) {
        output.format(mOutputFormat);
    }

    return reflector;
}

void ToneMapperPass::compile(RenderContext* pRenderContext, const CompileData& compileData) {

}

void ToneMapperPass::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    auto pDevice = pRenderContext->device();
    auto pSrc = renderData[kSrc]->asTexture();
    auto pDst = renderData[kDst]->asTexture();
    auto pLuminanceTex = renderData[kLuminanceTex]->asTexture();

    if(!pSrc) {
        LLOG_WRN << "No input specified for ToneMapperPass. Bypassing.";
        return;
    }

    uint2 dims = {pSrc->getWidth(), pSrc->getHeight()};

    // Run luminance pass if auto exposure is enabled
    if (mAutoExposure) {
        mpLuminancePass["gColorTex"] = pSrc;
        mpLuminancePass["gColorSampler"] = mpLinearSampler;
        mpLuminancePass["gLuminanceOutColor"] = pLuminanceTex;

        auto cb_var = mpLuminancePass["PerFrameCB"];
        cb_var["gFrameDim"] = dims;

        mpLuminancePass->execute(pRenderContext, dims.x, dims.y);
        pLuminanceTex->generateMips(pRenderContext);
    }

    // Run main pass
    if (mRecreateToneMapPass) {
        createToneMapPass(pDevice);
        mUpdateToneMapPass = true;
        mRecreateToneMapPass = false;
    }

    if (mUpdateToneMapPass) {
        updateWhiteBalanceTransform();
        updateColorTransform();

        ToneMapperParams params;
        params.whiteScale = mWhiteScale;
        params.whiteMaxLuminance = mWhiteMaxLuminance;
        params.colorTransform = static_cast<float3x4>(mColorTransform);
        
        auto cb_var = mpToneMapPass["PerFrameCB"];
        cb_var["gFrameDim"] = dims;
        cb_var["gParams"].setBlob(&params, sizeof(params));

        mUpdateToneMapPass = false;
    }

    mpToneMapPass["gColorTex"] = pSrc;
    mpToneMapPass["gDstColorTex"] = pDst;
    mpToneMapPass["gColorSampler"] = mpPointSampler;

    if (mAutoExposure) {
        mpToneMapPass["gLuminanceTexSampler"] = mpLinearSampler;
        mpToneMapPass["gLuminanceTex"] = pLuminanceTex;
    }

    mpToneMapPass->execute(pRenderContext, dims.x, dims.y);
}

void ToneMapperPass::setOutputFormat(ResourceFormat format) {
    if(mOutputFormat == format) return;
    mOutputFormat = format;
    mPassChangedCB();
}

void ToneMapperPass::setExposureCompensation(float exposureCompensation) {
    mExposureCompensation = glm::clamp(exposureCompensation, kExposureCompensationMin, kExposureCompensationMax);
    mUpdateToneMapPass = true;
}

void ToneMapperPass::setAutoExposure(bool autoExposure) {
    if(mAutoExposure == autoExposure) return;
    mAutoExposure = autoExposure;
    mRecreateToneMapPass = true;
}

void ToneMapperPass::setExposureValue(float exposureValue) {
    auto _exposureValue = glm::clamp(exposureValue, kExposureValueMin, kExposureValueMax);
    if(mExposureValue == _exposureValue) return;
    mExposureValue = _exposureValue;
    mUpdateToneMapPass = true;
}

void ToneMapperPass::setFilmSpeed(float filmSpeed) {
    _filmSpeed = glm::clamp(filmSpeed, kFilmSpeedMin, kFilmSpeedMax);
    if(mFilmSpeed == _filmSpeed) return;
    mFilmSpeed = _filmSpeed;
    mUpdateToneMapPass = true;
}

void ToneMapperPass::setWhiteBalance(bool whiteBalance) {
    if(mWhiteBalance == whiteBalance) return;
    mWhiteBalance = whiteBalance;
    mUpdateToneMapPass = true;
}

void ToneMapperPass::setWhitePoint(float whitePoint) {
    auto _whitePoint = glm::clamp(whitePoint, kWhitePointMin, kWhitePointMax);
    if(mWhitePoint == _whitePoint) return;
    mWhitePoint = _whitePoint;
    mUpdateToneMapPass = true;
}

void ToneMapperPass::setOperator(Operator op) {
    if(mOperator == op) return;
    mOperator = op;
    mRecreateToneMapPass = true;
}

void ToneMapperPass::setClamp(bool clamp) {
    if(mClamp == clamp) return;
    mClamp = clamp;
    mRecreateToneMapPass = true;
}

void ToneMapperPass::setWhiteMaxLuminance(float maxLuminance) {
    if(mWhiteMaxLuminance == maxLuminance) return;
    mWhiteMaxLuminance = maxLuminance;
    mUpdateToneMapPass = true;
}

void ToneMapperPass::setWhiteScale(float whiteScale) {
    mWhiteScale = std::max(0.001f, whiteScale);
    mUpdateToneMapPass = true;
}

void ToneMapperPass::createLuminancePass(std::shared_ptr<Device> pDevice) {
    Program::Desc desc;
    desc.addShaderLibrary(kLuminanceShaderFile).setShaderModel(kShaderModel).csEntry("main");

    Program::DefineList defines;
    mpLuminancePass = ComputePass::create(pDevice, desc, defines, true);
}

void ToneMapperPass::createToneMapPass(std::shared_ptr<Device> pDevice) {
    Program::Desc desc;
    desc.addShaderLibrary(kToneMappingShaderFile).setShaderModel(kShaderModel).csEntry("main");

    Program::DefineList defines;
    defines.add("_TONE_MAPPER_OPERATOR", std::to_string(static_cast<uint32_t>(mOperator)));
    if (mAutoExposure) defines.add("_TONE_MAPPER_AUTO_EXPOSURE");
    if (mClamp) defines.add("_TONE_MAPPER_CLAMP");

    mpToneMapPass = ComputePass::create(pDevice, desc, defines, true);
}

void ToneMapperPass::updateWhiteBalanceTransform() {
    // Calculate color transform for the current white point.
    mWhiteBalanceTransform = mWhiteBalance ? calculateWhiteBalanceTransformRGB_Rec709(mWhitePoint) : glm::identity<float3x3>();
    // Calculate source illuminant, i.e. the color that transforms to a pure white (1, 1, 1) output at the current color settings.
    mSourceWhite = inverse(mWhiteBalanceTransform) * float3(1, 1, 1);
}

void ToneMapperPass::updateColorTransform() {
    // Exposure scale due to exposure compensation.
    float exposureScale = pow(2.f, mExposureCompensation);
    // Exposure scale due to manual exposure (only if auto exposure is disabled).
    float manualExposureScale = mAutoExposure ? 1.f : pow(2.f, -mExposureValue) * mFilmSpeed / 100.f;
    // Calculate final transform.
    mColorTransform = mWhiteBalanceTransform * exposureScale * manualExposureScale;
}
