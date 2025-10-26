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
#include "EnvPass.h"

#include <pybind11/embed.h>

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/Utils/Debug/debug.h"

#include "Falcor/Scene/Lights/LightData.slang"

static void regEnvPass(pybind11::module& m) {
    pybind11::class_<EnvPass, RenderPass> pass(m, "EnvPass");
    //pybind11::class_<EnvPass, RenderPass, EnvPass::SharedPtr> pass(m, "EnvPass");

    pass.def_property("scale", &EnvPass::getScale, &EnvPass::setScale);
    pass.def_property("filter", &EnvPass::getFilter, &EnvPass::setFilter);
    pass.def_property("intensity", &EnvPass::getIntensity, &EnvPass::setIntensity);
    pass.def_property("opacity", &EnvPass::getOpacity, &EnvPass::setOpacity);
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry) {
    //registry.registerClass<RenderPass, EnvPass>();
    ScriptBindings::registerBinding(regEnvPass);
}

namespace {
    const char kShaderFile[] = "RenderPasses/EnvironmentPass/EnvPass.cs.slang";

    const std::string kBackdropTexture = "gBackdropTexture";
    const std::string kOutputColor = "color";
    const std::string kOutputDepth = "depth";

    // Dictionary keys
    const std::string kBackdropImageName = "backdropImagePath";
    const std::string kLoadAsSrgb = "loadAsSrgb";
    const std::string kFilter = "filter";
    const std::string kIntensity = "intensity";
    const std::string kOpacity = "opacity";
    const std::string kUseDOF = "useDOF";

    //
    const std::string kLightsBufferName = "gLights";

    const ChannelList kExtraOutputChannels = {
        { kOutputDepth,            "gDepth",         "Depth buffer",                         true /* optional */, ResourceFormat::R32Float },
    };
}

EnvPass::EnvPass(Device::SharedPtr pDevice): RenderPass(pDevice) {
    if (!mpDevice->isShaderModelSupported(ShaderModel::SM6_5)) {
        FALCOR_THROW("EnvPass: requires Shader Model 6.5 support.");
    }

    setFilter((uint32_t)mFilter);
    setupCamera();
}

void EnvPass::parseProperties(const Properties& props) {
    for (const auto& [key, value] : props) {
        if (key == kBackdropImageName) setBackdropImagePath(value);
        else if (key == kLoadAsSrgb) setLoadBackdropAsSRGB(value);
        else if (key == kFilter) setFilter(value);
        else if (key == kIntensity) setIntensity(value);
        else if (key == kOpacity) setOpacity(value);
        else LLOG_WRN << "Unknown field '" << key << "' in an EnvPass dictionary";
    }
}

EnvPass::SharedPtr EnvPass::create(RenderContext* pRenderContext, const Properties& props) {
    return SharedPtr(new EnvPass(pRenderContext->getDevice()));
}

Properties EnvPass::getProperties() const {
    Properties props;
    props[kBackdropImageName] = mpBackdropTexture ? mpBackdropTexture->getSourceFilename() : mBackdropImagePath;
    props[kLoadAsSrgb] = mBackdropImageLoadSrgb;
    props[kFilter] = mFilter;
    props[kIntensity] = mIntensity;
    props[kOpacity] = mOpacity;
    props[kUseDOF] = mUseDOF;
    return props;
}

RenderPassReflection EnvPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    reflector.addOutput(kOutputColor, "Color buffer");
    addRenderPassOutputs(reflector, kExtraOutputChannels, ResourceBindFlags::UnorderedAccess);
    return reflector;
}

void EnvPass::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    if(!mpScene) return;

    Texture::SharedPtr pOutColor = renderData[kOutputColor]->asTexture();
    
    bool computeDOF = mUseDOF && mpScene->getCamera()->getApertureRadius() > 0.f;
    if(mComputeDOF != computeDOF) {
        mComputeDOF = computeDOF;
        mDirty = true;
    }

    if (!mpComputePass || mDirty) {
        loadBackdropImage();

        Program::Desc desc;
        desc.addShaderLibrary(kShaderFile).csEntry("main");
        desc.addTypeConformances(mpScene->getTypeConformances());
        
        //auto defines = Program::DefineList();
        auto defines = mpScene->getSceneDefines();

        defines.add("COMPUTE_DEPTH_OF_FIELD", mComputeDOF ? "1" : "0");
        defines.add("is_valid_" + kBackdropTexture, mpBackdropTexture ? "1" : "0");
        defines.add(getValidResourceDefines(kExtraOutputChannels, renderData));

        mpComputePass = ComputePass::create(mpDevice, desc, defines, true);

        auto var = mpComputePass->getRootVar();

        var["gScene"] = mpScene->getParameterBlock();
        var[kBackdropTexture] = mpBackdropTexture;
        var["gSampler"] = mpSampler;

        // Bind mandatory input channels
        var["gOutColor"] = pOutColor;

        // Bind extra output channels as UAV buffers.
        for (const auto& channel : kExtraOutputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            var[channel.texname] = pTex;
        }
    }

    const uint2 frameDim = uint2(pOutColor->getWidth(), pOutColor->getHeight());

    auto cb_var = mpComputePass->getRootVar()["PerFrameCB"];
    cb_var["frameDim"] = frameDim;
    cb_var["backTextureDim"] = mpBackdropTexture ? uint2({mpBackdropTexture->getWidth(), mpBackdropTexture->getHeight()}) : uint2({1, 1});
    cb_var["gScale"] = mScale;
    cb_var["frameNumber"] = mFrameNumber++;

    mpCamera->bindShaderData(cb_var["gCamera"]);

    cb_var["gIntensity"] = mIntensity;
    cb_var["gOpacity"] = mOpacity;
    cb_var["lightsCount"] = mpScene->getLightCount();

    mpComputePass->execute(pRenderContext, frameDim.x, frameDim.y);
    
    mDirty = false;
}

void EnvPass::loadBackdropImage() {
    if (!mDirty || mBackdropImagePath.empty()) return;

    setBackdropTexture(Texture::createFromFile(mpDevice, mBackdropImagePath, false, mBackdropImageLoadSrgb));
}

void EnvPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    if(mpScene == pScene) return;

    mpScene = pScene;
    if (mpScene) {
        setupCamera();
    }

    mDirty = true;
}

void EnvPass::setupCamera() {
    if(!mDirty && mpCamera) return;
    
    Camera::SharedPtr pCamera;
    if(mpScene) {
        pCamera = mpScene->getCamera();
        setBackdropImagePath(pCamera->getBackgroundImageFilename());
    } else {
        pCamera = Camera::create();
    }

    if(mpCamera != pCamera) mDirty = true;
    mpCamera = pCamera;
}

void EnvPass::setLoadBackdropAsSRGB(bool mode) {
    if(mBackdropImageLoadSrgb == mode) return;
    mBackdropImageLoadSrgb = mode;
    mDirty = true;
}

void EnvPass::setBackdropImagePath(const std::string& imageName) {
    if (mBackdropImagePath == imageName) return;
    mBackdropImagePath = imageName;
    mDirty = true;
}

void EnvPass::setBackdropTexture(const Texture::SharedPtr& pTexture) {
    if(mpBackdropTexture == pTexture) return;
    mpBackdropTexture = pTexture;
    mDirty = true;
}

void EnvPass::setFilter(uint32_t filter) {
    Sampler::Filter _filter = static_cast<Sampler::Filter>(filter);
    if((mFilter == _filter) && mpSampler) return;

    mFilter = _filter;
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(mFilter, mFilter, mFilter)
        .setAddressingMode(Sampler::AddressMode::Border, Sampler::AddressMode::Border, Sampler::AddressMode::Border)
        .setBorderColor({0.0, 0.0, 0.0, 0.0});
    mpSampler = Sampler::create(mpDevice, samplerDesc);
    mDirty = true;
}
