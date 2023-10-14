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
#include "glm/gtx/transform.hpp"
#include "glm/gtx/euler_angles.hpp"

#include <pybind11/embed.h>

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"
#include "Falcor/Utils/Debug/debug.h"

#include "Falcor/Scene/Lights/LightData.slang"

const RenderPass::Info EnvPass::kInfo { "EnvPass", "Render a backdrop image and/or scene lights." };


// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

static void regEnvPass(pybind11::module& m) {
    pybind11::class_<EnvPass, RenderPass, EnvPass::SharedPtr> pass(m, "EnvPass");
    pass.def_property("scale", &EnvPass::getScale, &EnvPass::setScale);
    pass.def_property("filter", &EnvPass::getFilter, &EnvPass::setFilter);
    pass.def_property("intensity", &EnvPass::getIntensity, &EnvPass::setIntensity);
    pass.def_property("opacity", &EnvPass::getOpacity, &EnvPass::setOpacity);
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(EnvPass::kInfo, EnvPass::create);
    ScriptBindings::registerBinding(regEnvPass);
}

namespace {
    const char kShaderFile[] = "RenderPasses/EnvironmentPass/EnvPass.cs.slang";
    const std::string kShaderModel = "6_5";

    const std::string kBackdropTexture = "gBackdropTexture";

    const std::string kOutputColor = "target";
    const std::string kDepth = "depth";

    // Dictionary keys
    const std::string kBackdropImageName = "backdropImagePath";
    const std::string kLoadAsSrgb = "loadAsSrgb";
    const std::string kFilter = "filter";
    const std::string kIntensity = "intensity";
    const std::string kOpacity = "opacity";
    const std::string kUseDOF = "useDOF";

    //
    const std::string kLightsBufferName = "gLights";
    const std::string kLightIDsBufferName = "gLightIDs";
}

EnvPass::EnvPass(Device::SharedPtr pDevice): RenderPass(pDevice, kInfo) {
    assert(pDevice);

    setFilter((uint32_t)mFilter);
    setupCamera();
}

EnvPass::SharedPtr EnvPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    SharedPtr pEnvPass = SharedPtr(new EnvPass(pRenderContext->device()));
    
    auto pDevice = pRenderContext->device();

    std::string backdropImageName;

    for (const auto& [key, value] : dict) {
        if (key == kBackdropImageName) backdropImageName = value.operator std::string();
        else if (key == kLoadAsSrgb) pEnvPass->mBackdropImageLoadSrgb = value;
        else if (key == kFilter) pEnvPass->setFilter(value);
        else if (key == kIntensity) pEnvPass->setIntensity(value);
        else if (key == kOpacity) pEnvPass->setOpacity(value);
        else LLOG_WRN << "Unknown field '" << key << "' in an EnvPass dictionary";
    }

    pEnvPass->setBackdropImage(backdropImageName, pEnvPass->mBackdropImageLoadSrgb);
    
    return pEnvPass;
}

Dictionary EnvPass::getScriptingDictionary() {
    Dictionary dict;
    dict[kBackdropImageName] = mpBackdropTexture ? mpBackdropTexture->getSourceFilename() : std::string();
    dict[kLoadAsSrgb] = mBackdropImageLoadSrgb;
    dict[kFilter] = mFilter;
    dict[kIntensity] = mIntensity;
    dict[kOpacity] = mOpacity;
    dict[kUseDOF] = mUseDOF;
    return dict;
}

RenderPassReflection EnvPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    reflector.addOutput(kOutputColor, "Color buffer");
    auto& depthField = reflector.addInputOutput(kDepth, "Depth-buffer. Should be pre-initialized or cleared before calling the pass");//.bindFlags(Resource::BindFlags::DepthStencil);
    return reflector;
}

void EnvPass::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    if(!mpScene) return;

    Texture::SharedPtr pDst = renderData[kOutputColor]->asTexture();


    bool computeDOF = mUseDOF && mpScene->getCamera()->getApertureRadius() > 0.f;
    if(mComputeDOF != computeDOF) {
        mComputeDOF = computeDOF;
        mDirty = true;
    }

    if (!mpComputePass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("main");
        desc.addTypeConformances(mpScene->getTypeConformances());
        
        //auto defines = Program::DefineList();
        auto defines = mpScene->getSceneDefines();

        defines.add("COMPUTE_DEPTH_OF_FIELD", mComputeDOF ? "1" : "0");
        defines.add("is_valid_" + kBackdropTexture, mpBackdropTexture != nullptr ? "1" : "0");

        mpComputePass = ComputePass::create(mpDevice, desc, defines, true);

        mpComputePass["gScene"] = mpScene->getParameterBlock();
        
        mpComputePass[kLightIDsBufferName] = lightsIDsBuffer();
        mpComputePass[kBackdropTexture] = mpBackdropTexture;

        // Bind mandatory input channels
        mpComputePass["gOutColor"] = pDst;

        mpComputePass["gSampler"] = mpSampler;
    }

    const uint2 frameDim = uint2(pDst->getWidth(), pDst->getHeight());

    auto cb_var = mpComputePass["PerFrameCB"];
    cb_var["frameDim"] = frameDim;
    cb_var["backTextureDim"] = mpBackdropTexture ? uint2({mpBackdropTexture->getWidth(), mpBackdropTexture->getHeight()}) : uint2({1, 1});
    cb_var["gScale"] = mScale;
    cb_var["frameNumber"] = mFrameNumber++;

    mpCamera->setShaderData(cb_var["gCamera"]);

    cb_var["gIntensity"] = mIntensity;
    cb_var["gOpacity"] = mOpacity;
    cb_var["lightsCount"] = mpLightIDsBuffer ? mpLightIDsBuffer->getElementCount() : 0;

    mpComputePass->execute(pRenderContext, frameDim.x, frameDim.y);
    
    mDirty = false;
}

void EnvPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    if(pScene && (mpScene == pScene)) return;

    mpScene = pScene;
    mSceneLights.clear();

    if (mpScene) {
        const std::vector<Light::SharedPtr>& lights = mpScene->getActiveLights();
        for(const auto& light: lights) {
            if(!light) continue;
            switch(light->getType()) {
                case LightType::Env:
                case LightType::PhysSunSky:
                    mSceneLights.push_back(light);
                    break;
                default:
                    break;
            }
        }
    setupCamera();
    }

    mDirty = true;
}

void EnvPass::setupCamera() {
    if(!mDirty && mpCamera) return;
    
    Camera::SharedPtr pCamera;
    if(mpScene) {
        pCamera = mpScene->getCamera();
        setBackdropImage(pCamera->getBackgroundImageFilename(), mBackdropImageLoadSrgb);
    } else {
        pCamera = Camera::create();
    }

    if(mpCamera != pCamera) mDirty = true;
    mpCamera = pCamera;
}

Buffer::SharedPtr EnvPass::lightsIDsBuffer() {
    if(!mDirty && mpLightIDsBuffer) return mpLightIDsBuffer;

    if(mSceneLights.empty()) {
        mpLightIDsBuffer = nullptr;
    } else {
        std::vector<uint32_t> lightIDs;
        for(size_t i = 0; i < mpScene->getLightCount(); ++i) {
            const auto& pLight = mpScene->getLight(i);
            if(!pLight) continue;
            switch(pLight->getType()) {
                case LightType::Env:
                case LightType::PhysSunSky:
                    lightIDs.push_back(i);
                    break;
                default:
                    break;
            }
        }
        mpLightIDsBuffer = Buffer::createStructured(mpDevice, sizeof(uint32_t), (uint32_t)lightIDs.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, lightIDs.data(), false);
        mpLightIDsBuffer->setName("EnvPass::mpLightIDsBuffer");
    }
    return mpLightIDsBuffer;
}

void EnvPass::setBackdropImage(const std::string& imageName, bool loadAsSrgb) {
    std::shared_ptr<Texture> pTexture;
    if (!imageName.empty()) {
        setBackdropTexture(Texture::createFromFile(mpDevice, imageName, false, loadAsSrgb));
    }
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
