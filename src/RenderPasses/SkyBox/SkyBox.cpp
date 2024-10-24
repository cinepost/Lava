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
#include "SkyBox.h"
#include "glm/gtx/transform.hpp"
#include "glm/gtx/euler_angles.hpp"

#include <pybind11/embed.h>

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"
#include "Falcor/Utils/Debug/debug.h"

#include "Falcor/Scene/Lights/LightData.slang"

const RenderPass::Info SkyBox::kInfo { "SkyBox", "Render an environment map. The map can be provided by the user or taken from a scene." };


// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

static void regSkyBox(pybind11::module& m) {
    pybind11::class_<SkyBox, RenderPass, SkyBox::SharedPtr> pass(m, "SkyBox");
    pass.def_property("scale", &SkyBox::getScale, &SkyBox::setScale);
    pass.def_property("filter", &SkyBox::getFilter, &SkyBox::setFilter);
    pass.def_property("intensity", &SkyBox::getIntensity, &SkyBox::setIntensity);
    pass.def_property("opacity", &SkyBox::getOpacity, &SkyBox::setOpacity);
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(SkyBox::kInfo, SkyBox::create);
    ScriptBindings::registerBinding(regSkyBox);
}

namespace {

    const std::string kTarget = "target";
    const std::string kDepth = "depth";

    // Dictionary keys
    const std::string kTexName = "texName";
    const std::string kLoadAsSrgb = "loadAsSrgb";
    const std::string kFilter = "filter";
    const std::string kIntensity = "intensity";
    const std::string kOpacity = "opacity";

    //
    const std::string kLightsBufferName = "gLights";

}

SkyBox::SkyBox(Device::SharedPtr pDevice): RenderPass(pDevice, kInfo) {
    assert(pDevice);

    mpCubeScene = Scene::create(pDevice, "SkyBox/cube.obj");
    if (mpCubeScene == nullptr) throw std::runtime_error("SkyBox::SkyBox - Failed to load cube model");

    mpProgram = GraphicsProgram::createFromFile(pDevice, "RenderPasses/SkyBox/SkyBox.3d.slang", "vsMain", "psMain");
    mpProgram->addDefines(mpCubeScene->getSceneDefines());
    mpProgram->addDefine("_SKYBOX_SOLID_MODE");
    mpVars = GraphicsVars::create(pDevice, mpProgram->getReflector());
    mpFbo = Fbo::create(pDevice);

    // Create state
    mpState = GraphicsState::create(pDevice);
    BlendState::Desc blendDesc(pDevice);

    for (uint32_t i = 1; i < Fbo::getMaxColorTargetCount(); i++) {
        blendDesc.setRenderTargetWriteMask(i, false, false, false, false);
    }

    blendDesc.setIndependentBlend(true);
    mpState->setBlendState(BlendState::create(blendDesc));

    // Create the rasterizer state
    RasterizerState::Desc rastDesc;
    rastDesc.setCullMode(RasterizerState::CullMode::None).setDepthClamp(true);
    mpRsState = RasterizerState::create(rastDesc);

    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthWriteMask(false).setDepthFunc(DepthStencilState::Func::LessEqual);
    mpState->setDepthStencilState(DepthStencilState::create(dsDesc));
    mpState->setProgram(mpProgram);

    setFilter((uint32_t)mFilter);
    setupCamera();
}

SkyBox::SharedPtr SkyBox::create(RenderContext* pRenderContext, const Dictionary& dict) {
    SharedPtr pSkyBox = SharedPtr(new SkyBox(pRenderContext->device()));
    
    auto pDevice = pRenderContext->device();

    for (const auto& [key, value] : dict) {
        if (key == kTexName) pSkyBox->mTexName = value.operator std::string();
        else if (key == kLoadAsSrgb) pSkyBox->mLoadSrgb = value;
        else if (key == kFilter) pSkyBox->setFilter(value);
        else if (key == kIntensity) pSkyBox->setIntensity(value);
        else if (key == kOpacity) pSkyBox->setOpacity(value);
        else LLOG_WRN << "Unknown field '" << key << "' in a SkyBox dictionary";
    }

    std::shared_ptr<Texture> pTexture;
    if (pSkyBox->mTexName.size() != 0) {
        pTexture = Texture::createFromFile(pRenderContext->device(), pSkyBox->mTexName, false, pSkyBox->mLoadSrgb);
        if (pTexture == nullptr) throw std::runtime_error("SkyBox::create - Error creating texture from file");
        pSkyBox->setTexture(pTexture);
    }
    return pSkyBox;
}

Dictionary SkyBox::getScriptingDictionary() {
    Dictionary dict;
    dict[kTexName] = mTexName;
    dict[kLoadAsSrgb] = mLoadSrgb;
    dict[kFilter] = mFilter;
    dict[kIntensity] = mIntensity;
    dict[kOpacity] = mOpacity;
    return dict;
}

RenderPassReflection SkyBox::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    reflector.addOutput(kTarget, "Color buffer");//.format(ResourceFormat::RGBA16Float);
    auto& depthField = reflector.addInputOutput(kDepth, "Depth-buffer. Should be pre-initialized or cleared before calling the pass");//.bindFlags(Resource::BindFlags::DepthStencil);
    return reflector;
}

void SkyBox::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    mpFbo->attachColorTarget(renderData[kTarget]->asTexture(), 0);
    mpFbo->attachDepthStencilTarget(renderData[kDepth]->asTexture());

    pRenderContext->clearRtv(mpFbo->getRenderTargetView(0).get(), float4(0));

    if (!mpScene) return;
    if (!mpCamera) return;

    mpVars["PerFrameCB"]["gScale"] = mScale;

    mpVars["PerFrameCB"]["gWorld"] = glm::translate(mpCamera->getPosition()) * mTransformMatrix;
    mpVars["PerFrameCB"]["gViewMat"] = mpScene->getCamera()->getViewMatrix();
    mpVars["PerFrameCB"]["gProjMat"] = mpScene->getCamera()->getProjMatrix();
    mpCamera->setShaderData(mpVars["PerFrameCB"]["gCamera"]);

    mpVars["PerFrameCB"]["gIntensity"] = mIntensity;
    mpVars["PerFrameCB"]["gOpacity"] = mOpacity;
    mpVars["PerFrameCB"]["lightsCount"] = static_cast<uint32_t>(mSceneLights.size());
    mpVars[kLightsBufferName] = lightsBuffer();
    mpState->setFbo(mpFbo);
    mpCubeScene->rasterize(pRenderContext, mpState.get(), mpVars.get(), mpRsState, mpRsState);

    mDirty = false;
}

void SkyBox::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    if(mpScene == pScene) return;

    mpScene = pScene;
    mSceneLights.clear();

    if (mpScene) {
        //mpCubeScene->setCamera(mpScene->getCamera());
        
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

        auto pEnvMap = mpScene->getEnvMap();
        if (pEnvMap) {
            setTexture(pEnvMap->getEnvMap());
            setIntensity(pEnvMap->getTint());
        }
    }
    setupCamera();
    
    mDirty = true;
}

void SkyBox::setupCamera() {
    if(!mDirty && mpCamera) return;
    
    Camera::SharedPtr pCamera;
    if(mpScene) {
        pCamera = mpScene->getCamera();
    } else {
        pCamera = Camera::create();
    }

    if(mpCamera != pCamera) mDirty = true;
    mpCamera = pCamera;
}

Buffer::SharedPtr SkyBox::lightsBuffer() {
    if(!mDirty && mpLightsBuffer) return mpLightsBuffer;

    if(mSceneLights.empty()) {
        mpLightsBuffer = nullptr;
    } else {
        std::vector<LightData> lightsData(mSceneLights.size());
        for(size_t i = 0; i < mSceneLights.size(); ++i) {
            lightsData[i] = mSceneLights[i]->getData();
        }
        mpLightsBuffer = Buffer::createStructured(mpDevice, sizeof(LightData), (uint32_t)lightsData.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, lightsData.data(), false);
        mpLightsBuffer->setName("SkyBox::mpLightsBuffer");
    }
    return mpLightsBuffer;
}

void SkyBox::loadImage() {
    // std::string filename;
    // FileDialogFilterVec filters = { {"bmp"}, {"jpg"}, {"dds"}, {"png"}, {"tiff"}, {"tif"}, {"tga"}, {"exr"} };
    // if (openFileDialog(filters, filename)) {
    //     mpTexture = Texture::createFromFile(mpDevice, filename, false, mLoadSrgb);
    //     setTexture(mpTexture);
    // }
}

void SkyBox::setTexture(const std::string& texName, bool loadAsSrgb) {
    std::shared_ptr<Texture> pTexture;
    if (texName.size() != 0) {
        mTexName = texName;
        pTexture = Texture::createFromFile(mpDevice, mTexName, false, loadAsSrgb);
        if (pTexture == nullptr) throw std::runtime_error("SkyBox::create - Error creating texture from file");
        setTexture(pTexture);
    }
}

void SkyBox::setTexture(const Texture::SharedPtr& pTexture) {
    mpTexture = pTexture;
    if (mpTexture) {
        mSolidMode = false;
        mpProgram->removeDefine("_SKYBOX_SOLID_MODE");
        assert(mpTexture->getType() == Texture::Type::TextureCube || mpTexture->getType() == Texture::Type::Texture2D);
        (mpTexture->getType() == Texture::Type::Texture2D) ? mpProgram->addDefine("_SPHERICAL_MAP") : mpProgram->removeDefine("_SPHERICAL_MAP");
    } else {
        mSolidMode = true;
        mpProgram->addDefine("_SKYBOX_SOLID_MODE");
    }
    mpVars["gTexture"] = mpTexture;
}

void SkyBox::setFilter(uint32_t filter) {
    mFilter = (Sampler::Filter)filter;
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(mFilter, mFilter, mFilter);
    mpSampler = Sampler::create(mpDevice, samplerDesc);
    mpVars["gSampler"] = mpSampler;
}
