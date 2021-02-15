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

#include "Falcor/Utils/Debug/debug.h"

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

static void regSkyBox(pybind11::module& m) {
    pybind11::class_<SkyBox, RenderPass, SkyBox::SharedPtr> pass(m, "SkyBox");
    pass.def_property("scale", &SkyBox::getScale, &SkyBox::setScale);
    pass.def_property("filter", &SkyBox::getFilter, &SkyBox::setFilter);
    pass.def_property("intensity", &SkyBox::getIntensity, &SkyBox::setIntensity);
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerClass("SkyBox", "Render an environment map", SkyBox::create);
    ScriptBindings::registerBinding(regSkyBox);
}
const char* SkyBox::kDesc = "Render an environment-map. The map can be provided by the user or taken from a scene";

namespace {

    const Gui::DropdownList kFilterList = {
        { (uint32_t)Sampler::Filter::Linear, "Linear" },
        { (uint32_t)Sampler::Filter::Point, "Point" },
    };

    const std::string kTarget = "target";
    const std::string kDepth = "depth";

    // Dictionary keys
    const std::string kTexName = "texName";
    const std::string kLoadAsSrgb = "loadAsSrgb";
    const std::string kFilter = "filter";
    const std::string kIntensity = "intensity";

}

SkyBox::SkyBox(Device::SharedPtr pDevice): RenderPass(pDevice) {
    LOG_DBG("SkyBox::SkyBox");

    mpCubeScene = Scene::create(pDevice, "cube.obj");
    if (mpCubeScene == nullptr) throw std::runtime_error("SkyBox::SkyBox - Failed to load cube model");

    mpProgram = GraphicsProgram::createFromFile(pDevice, "RenderPasses/SkyBox/SkyBox.slang", "vs", "ps");
    mpProgram->addDefines(mpCubeScene->getSceneDefines());
    mpVars = GraphicsVars::create(pDevice, mpProgram->getReflector());
    mpFbo = Fbo::create(pDevice);

    // Create state
    mpState = GraphicsState::create(pDevice);
    BlendState::Desc blendDesc(pDevice);

    for (uint32_t i = 1; i < Fbo::getMaxColorTargetCount(pDevice); i++) {
        blendDesc.setRenderTargetWriteMask(i, false, false, false, false);
    }

    blendDesc.setIndependentBlend(true);
    mpState->setBlendState(BlendState::create(blendDesc));

    // Create the rasterizer state
    RasterizerState::Desc rastDesc;
    rastDesc.setCullMode(RasterizerState::CullMode::Front).setDepthClamp(true);
    mpState->setRasterizerState(RasterizerState::create(rastDesc));

    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthWriteMask(false).setDepthFunc(DepthStencilState::Func::LessEqual);
    mpState->setDepthStencilState(DepthStencilState::create(dsDesc));
    mpState->setProgram(mpProgram);

    assert(mpDevice);
    setFilter((uint32_t)mFilter);
    LOG_DBG("SkyBox::SkyBox done");
}

SkyBox::SharedPtr SkyBox::create(RenderContext* pRenderContext, const Dictionary& dict) {
    SharedPtr pSkyBox = SharedPtr(new SkyBox(pRenderContext->device()));
    for (const auto& [key, value] : dict)
    {
        if (key == kTexName) pSkyBox->mTexName = value.operator std::string();
        else if (key == kLoadAsSrgb) pSkyBox->mLoadSrgb = value;
        else if (key == kFilter) pSkyBox->setFilter(value);
        else if (key == kIntensity) pSkyBox->setIntensity(value);
        else logWarning("Unknown field '" + key + "' in a SkyBox dictionary");
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
    return dict;
}

RenderPassReflection SkyBox::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    reflector.addOutput(kTarget, "Color buffer").format(ResourceFormat::RGBA32Float);
    auto& depthField = reflector.addInputOutput(kDepth, "Depth-buffer. Should be pre-initialized or cleared before calling the pass").bindFlags(Resource::BindFlags::DepthStencil);
    return reflector;
}

void SkyBox::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    LOG_DBG("SkyBox::execute");
    mpFbo->attachColorTarget(renderData[kTarget]->asTexture(), 0);
    mpFbo->attachDepthStencilTarget(renderData[kDepth]->asTexture());

    pRenderContext->clearRtv(mpFbo->getRenderTargetView(0).get(), float4(0));

    if (!mpScene) return;

    auto skyRotation = glm::eulerAngleYXZ(0.0, -90.0, 0.0);
    glm::mat4 world = glm::translate(mpScene->getCamera()->getPosition());
    mpVars["PerFrameCB"]["gWorld"] = world;
    mpVars["PerFrameCB"]["gScale"] = mScale;
    mpVars["PerFrameCB"]["gViewMat"] = mpScene->getCamera()->getViewMatrix();
    mpVars["PerFrameCB"]["gProjMat"] = mpScene->getCamera()->getProjMatrix();
    mpVars["PerFrameCB"]["gIntensity"] = mIntensity;
    mpState->setFbo(mpFbo);
    LOG_DBG("SkyBox::execute render");
    mpCubeScene->render(pRenderContext, mpState.get(), mpVars.get(), Scene::RenderFlags::UserRasterizerState);
    LOG_DBG("SkyBox::SkyBox done");
}

void SkyBox::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    mpScene = pScene;

    if (mpScene) {
        mpCubeScene->setCamera(mpScene->getCamera());
        if (mpScene->getEnvMap()) setTexture(mpScene->getEnvMap()->getEnvMap());
    }
}

void SkyBox::renderUI(Gui::Widgets& widget) {
    float scale = mScale;
    if (widget.var("Scale", scale, 0.f)) setScale(scale);

    if (widget.button("Load Image")) { loadImage(); }

    uint32_t filter = (uint32_t)mFilter;
    if (widget.dropdown("Filter", kFilterList, filter)) setFilter(filter);
}

void SkyBox::loadImage() {
    std::string filename;
    FileDialogFilterVec filters = { {"bmp"}, {"jpg"}, {"dds"}, {"png"}, {"tiff"}, {"tif"}, {"tga"}, {"exr"} };
    if (openFileDialog(filters, filename)) {
        mpTexture = Texture::createFromFile(mpDevice, filename, false, mLoadSrgb);
        setTexture(mpTexture);
    }
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
        assert(mpTexture->getType() == Texture::Type::TextureCube || mpTexture->getType() == Texture::Type::Texture2D);
        (mpTexture->getType() == Texture::Type::Texture2D) ? mpProgram->addDefine("_SPHERICAL_MAP") : mpProgram->removeDefine("_SPHERICAL_MAP");
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
