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
#include "ToneMapperPass.h"

#include "Falcor/Utils/Color/ColorUtils.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/Utils/Scripting/ScriptBindings.h"


static void regToneMapperPass(pybind11::module& m) {
    pybind11::class_<ToneMapperPass, RenderPass> pass(m, "ToneMapperPass");
    //pybind11::class_<ToneMapperPass, RenderPass, ToneMapperPass::SharedPtr> pass(m, "ToneMapperPass");
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry) {
    //registry.registerClass<RenderPass, ToneMapperPass>();
    ScriptBindings::registerBinding(regToneMapperPass);
}

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

void ToneMapperPass::parseProperties(const Properties& props) {
    for (const auto& [key, value] : props) {
        if (key == kExposureCompensation) setExposureCompensation(value);
        else if (key == kAutoExposure) setAutoExposure(value);
        else if (key == kExposureValue) setExposureValue(value);
        else if (key == kFilmSpeed) setFilmSpeed(value);
        else if (key == kWhiteBalance) setWhiteBalance(value);
        else if (key == kWhitePoint) setWhitePoint(value);
        else if (key == kOperator) setOperator(static_cast<ToneMapperOperator>((uint32_t)value));
        else if (key == kClamp) setClamp(value);
        else if (key == kWhiteMaxLuminance) setWhiteMaxLuminance(value);
        else if (key == kWhiteScale) setWhiteScale(value);
        else if (key == kOutputFormat) setOutputFormat(value);
    }
}

ToneMapperPass::ToneMapperPass(Device::SharedPtr pDevice, ToneMapperPass::Operator op, ResourceFormat outputFormat, const Properties& props)
    : RenderPass(pDevice)
    , mOutputFormat(outputFormat)
    , mOperator(op) 
{
    if (!mpDevice->isShaderModelSupported(ShaderModel::SM6_5)) {
        FALCOR_THROW("ToneMapperPass requires Shader Model 6.5 support.");
    }

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpPointSampler = mpDevice->createSampler(samplerDesc);
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
    mpLinearSampler = mpDevice->createSampler(samplerDesc);

    parseProperties(props);

    createLuminancePass();
    createToneMapPass();

    updateWhiteBalanceTransform();
}

ToneMapperPass::SharedPtr ToneMapperPass::create(RenderContext* pRenderContext, const Properties& props) {
    return SharedPtr(new ToneMapperPass(pRenderContext->getDevice(), Operator::HableUc2, ResourceFormat::Unknown, props));
}

Properties ToneMapperPass::getProperties() const {
    Properties props;
    props[kOutputFormat] = mOutputFormat;
    props[kExposureCompensation] = mExposureCompensation;
    props[kAutoExposure] = mAutoExposure;
    props[kExposureValue] = mExposureValue;
    props[kFilmSpeed] = mFilmSpeed;
    props[kWhiteBalance] = mWhiteBalance;
    props[kWhitePoint] = mWhitePoint;
    props[kOperator] = static_cast<uint32_t>(mOperator);
    props[kClamp] = mClamp;
    props[kWhiteMaxLuminance] = mWhiteMaxLuminance;
    props[kWhiteScale] = mWhiteScale;
    return props;
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
    auto pDevice = pRenderContext->getDevice();
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
        auto var = mpLuminancePass->getRootVar();

        var["gColorTex"] = pSrc;
        var["gColorSampler"] = mpLinearSampler;
        var["gLuminanceOutColor"] = pLuminanceTex;

        auto cb_var = var["PerFrameCB"];
        cb_var["gFrameDim"] = dims;

        mpLuminancePass->execute(pRenderContext, dims.x, dims.y);
        pLuminanceTex->generateMips(pRenderContext);
    }

    // Run main pass
    if (mRecreateToneMapPass) {
        createToneMapPass();
        mUpdateToneMapPass = true;
        mRecreateToneMapPass = false;
    }

    auto var = mpToneMapPass->getRootVar();

    if (mUpdateToneMapPass) {
        updateWhiteBalanceTransform();
        updateColorTransform();

        ToneMapperParams params;
        params.whiteScale = mWhiteScale;
        params.whiteMaxLuminance = mWhiteMaxLuminance;
        params.colorTransform = static_cast<float3x4>(mColorTransform);
        
        auto cb_var = var["PerFrameCB"];
        cb_var["gFrameDim"] = dims;
        cb_var["gParams"].setBlob(&params, sizeof(params));

        mUpdateToneMapPass = false;
    }

    var["gColorTex"] = pSrc;
    var["gDstColorTex"] = pDst;
    var["gColorSampler"] = mpPointSampler;

    if (mAutoExposure) {
        var["gLuminanceTexSampler"] = mpLinearSampler;
        var["gLuminanceTex"] = pLuminanceTex;
    }

    mpToneMapPass->execute(pRenderContext, dims.x, dims.y);
}

void ToneMapperPass::setOutputFormat(ResourceFormat format) {
    if(mOutputFormat == format) return;
    mOutputFormat = format;
    mPassChangedCB();
}

void ToneMapperPass::setExposureCompensation(float exposureCompensation) {
    auto _exposureCompensation = std::clamp(exposureCompensation, kExposureCompensationMin, kExposureCompensationMax);
    if(mExposureCompensation == _exposureCompensation) return;
    mExposureCompensation = _exposureCompensation;
    mUpdateToneMapPass = true;
}

void ToneMapperPass::setAutoExposure(bool autoExposure) {
    if(mAutoExposure == autoExposure) return;
    mAutoExposure = autoExposure;
    mRecreateToneMapPass = true;
}

void ToneMapperPass::setExposureValue(float exposureValue) {
    auto _exposureValue = std::clamp(exposureValue, kExposureValueMin, kExposureValueMax);
    if(mExposureValue == _exposureValue) return;
    mExposureValue = _exposureValue;
    mUpdateToneMapPass = true;
}

void ToneMapperPass::setFilmSpeed(float filmSpeed) {
    auto _filmSpeed = std::clamp(filmSpeed, kFilmSpeedMin, kFilmSpeedMax);
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
    auto _whitePoint = std::clamp(whitePoint, kWhitePointMin, kWhitePointMax);
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
    auto _whiteScale = std::max(0.001f, whiteScale);
    if( mWhiteScale == _whiteScale) return;
    mWhiteScale = _whiteScale;
    mUpdateToneMapPass = true;
}

void ToneMapperPass::createLuminancePass() {
    Program::Desc desc;
    desc.addShaderLibrary(kLuminanceShaderFile).csEntry("main");

    Program::DefineList defines;
    mpLuminancePass = ComputePass::create(mpDevice, desc, defines, true);
}

void ToneMapperPass::createToneMapPass() {
    Program::Desc desc;
    desc.addShaderLibrary(kToneMappingShaderFile).csEntry("main");

    Program::DefineList defines;
    defines.add("_TONE_MAPPER_OPERATOR", std::to_string(static_cast<uint32_t>(mOperator)));
    if (mAutoExposure) defines.add("_TONE_MAPPER_AUTO_EXPOSURE");
    if (mClamp) defines.add("_TONE_MAPPER_CLAMP");

    mpToneMapPass = ComputePass::create(mpDevice, desc, defines, true);
}

void ToneMapperPass::updateWhiteBalanceTransform() {
    // Calculate color transform for the current white point.
    mWhiteBalanceTransform = mWhiteBalance ? calculateWhiteBalanceTransformRGB_Rec709(mWhitePoint) : float3x3::identity();
    // Calculate source illuminant, i.e. the color that transforms to a pure white (1, 1, 1) output at the current color settings.
    mSourceWhite = mul(inverse(mWhiteBalanceTransform), float3(1, 1, 1));
}

void ToneMapperPass::updateColorTransform() {
    // Exposure scale due to exposure compensation.
    float exposureScale = pow(2.f, mExposureCompensation);
    // Exposure scale due to manual exposure (only if auto exposure is disabled).
    float manualExposureScale = mAutoExposure ? 1.f : pow(2.f, -mExposureValue) * mFilmSpeed / 100.f;
    // Calculate final transform.
    mColorTransform = mWhiteBalanceTransform * exposureScale * manualExposureScale;
}
