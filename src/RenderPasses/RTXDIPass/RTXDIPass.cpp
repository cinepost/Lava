/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
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
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"
#include "Falcor/RenderGraph/RenderPassStandardFlags.h"
#include "Falcor/Utils/Timing/Profiler.h"

#include "RTXDIPass.h"

using namespace Falcor;

const RenderPass::Info RTXDIPass::kInfo { "RTXDIPass", "Standalone pass for direct lighting using RTXDI." };

namespace {
    const std::string kPrepareSurfaceDataFile = "RenderPasses/RTXDIPass/PrepareSurfaceData.cs.slang";
    const std::string kFinalShadingFile = "RenderPasses/RTXDIPass/FinalShading.cs.slang";

    const std::string kShaderModel = "6_5";

    const std::string kInputVBuffer = "vbuffer";
    const std::string kInputTexGrads = "texGrads";
    const std::string kInputMotionVectors = "mvec";


    const Falcor::ChannelList kInputChannels = {
      //{ kInputVBuffer,            "gVBuffer",                 "Visibility buffer in packed format" false, ResourceFormat::RGBA32Uint },
        { kInputTexGrads,           "gTextureGrads",            "Texture gradients", true /* optional */                   },
        { kInputMotionVectors,      "gMotionVector",            "Motion vector buffer (float format)", true /* optional */ },
    };

    const Falcor::ChannelList kOutputChannels = {
        { "color",                  "gColor",                   "Final color",              true /* optional */, ResourceFormat::RGBA32Float },
        { "emission",               "gEmission",                "Emissive color",           true /* optional */, ResourceFormat::RGBA32Float },
        { "diffuseIllumination",    "gDiffuseIllumination",     "Diffuse illumination",     true /* optional */, ResourceFormat::RGBA32Float },
        { "diffuseReflectance",     "gDiffuseReflectance",      "Diffuse reflectance",      true /* optional */, ResourceFormat::RGBA32Float },
        { "specularIllumination",   "gSpecularIllumination",    "Specular illumination",    true /* optional */, ResourceFormat::RGBA32Float },
        { "specularReflectance",    "gSpecularReflectance",     "Specular reflectance",     true /* optional */, ResourceFormat::RGBA32Float },
    };

    // Scripting options.
    const char* kOptions = "options";
}


// This is required for DLL and shader hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

// What passes does this DLL expose?  Register them here
extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(RTXDIPass::kInfo, RTXDIPass::create);
}

RTXDIPass::SharedPtr RTXDIPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    return RTXDIPass::SharedPtr(new RTXDIPass(pRenderContext->device(), dict));
}

RenderPassReflection RTXDIPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;

    reflector.addInput(kInputVBuffer, "Visibility buffer in packed format").format(ResourceFormat::RGBA32Uint);

    addRenderPassOutputs(reflector, kOutputChannels);
    addRenderPassInputs(reflector, kInputChannels);

    return reflector;
}

void RTXDIPass::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    // Clear outputs if no scene is loaded.
    if (!mpScene) {
        clearRenderPassChannels(pRenderContext, kOutputChannels, renderData);
        return;
    }

    FALCOR_ASSERT(mpRTXDI);

    const auto& pVBuffer = renderData[kInputVBuffer]->asTexture();
    const auto& pMotionVectors = renderData[kInputMotionVectors]->asTexture();

    auto& dict = renderData.getDictionary();

    // Update refresh flag if changes that affect the output have occured.
    if (mOptionsChanged) {
        auto flags = dict.getValue(kRenderPassRefreshFlags, Falcor::RenderPassRefreshFlags::None);
        flags |= Falcor::RenderPassRefreshFlags::RenderOptionsChanged;
        dict[Falcor::kRenderPassRefreshFlags] = flags;
        mOptionsChanged = false;
    }

    // Check if GBuffer has adjusted shading normals enabled.
    mGBufferAdjustShadingNormals = dict.getValue(Falcor::kRenderPassGBufferAdjustShadingNormals, false);

    mpRTXDI->beginFrame(pRenderContext, mFrameDim);

    prepareSurfaceData(pRenderContext, pVBuffer);

    mpRTXDI->update(pRenderContext, pMotionVectors);

    finalShading(pRenderContext, pVBuffer, renderData);

    mpRTXDI->endFrame(pRenderContext);
}

void RTXDIPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    mpScene = pScene;
    mpRTXDI = nullptr;
    mpPrepareSurfaceDataPass = nullptr;
    mpFinalShadingPass = nullptr;

    if (mpScene) {
        if (pScene->hasProceduralGeometry()) {
            LLOG_WRN << "RTXDIPass: This render pass only supports triangles. Other types of geometry will be ignored.";
        }

        mpRTXDI = RTXDI::create(mpScene, mOptions);
    }
}

RTXDIPass::RTXDIPass(Device::SharedPtr pDevice, const Dictionary& dict): RenderPass(pDevice, kInfo) {
    parseDictionary(dict);
}

void RTXDIPass::parseDictionary(const Dictionary& dict) {
    for (const auto& [key, value] : dict) {
        if (key == kOptions) mOptions = value;
        else LLOG_WRN << "Unknown field '" << key << "' in RTXDIPass dictionary.";
    }
}

Dictionary RTXDIPass::getScriptingDictionary() {
    Dictionary d;
    d[kOptions] = mOptions;
    return d;
}

void RTXDIPass::compile(RenderContext* pRenderContext, const CompileData& compileData) {
    mFrameDim = compileData.defaultTexDims;
}

void RTXDIPass::prepareSurfaceData(RenderContext* pRenderContext, const Texture::SharedPtr& pVBuffer) {
    assert(mpRTXDI);
    assert(pVBuffer);

    PROFILE(mpDevice, "prepareSurfaceData");

    if (!mpPrepareSurfaceDataPass) {
        Program::Desc desc;
        desc.addShaderLibrary(kPrepareSurfaceDataFile).setShaderModel(kShaderModel).csEntry("main");
        desc.addTypeConformances(mpScene->getTypeConformances());

        auto defines = mpScene->getSceneDefines();
        defines.add(mpRTXDI->getDefines());
        defines.add("GBUFFER_ADJUST_SHADING_NORMALS", mGBufferAdjustShadingNormals ? "1" : "0");

        mpPrepareSurfaceDataPass = ComputePass::create(mpDevice, desc, defines, true);
    }

    mpPrepareSurfaceDataPass->addDefine("GBUFFER_ADJUST_SHADING_NORMALS", mGBufferAdjustShadingNormals ? "1" : "0");

    mpPrepareSurfaceDataPass["gScene"] = mpScene->getParameterBlock();

    auto var = mpPrepareSurfaceDataPass["gPrepareSurfaceData"];

    var["vbuffer"] = pVBuffer;
    var["frameDim"] = mFrameDim;
    mpRTXDI->setShaderData(mpPrepareSurfaceDataPass->getRootVar());

    mpPrepareSurfaceDataPass->execute(pRenderContext, mFrameDim.x, mFrameDim.y);
}

void RTXDIPass::finalShading(RenderContext* pRenderContext, const Texture::SharedPtr& pVBuffer, const RenderData& renderData) {
    assert(mpRTXDI);
    assert(pVBuffer);

    PROFILE(mpDevice, "finalShading");

    if (!mpFinalShadingPass) {
        Program::Desc desc;
        desc.addShaderLibrary(kFinalShadingFile).setShaderModel(kShaderModel).csEntry("main");
        desc.addTypeConformances(mpScene->getTypeConformances());

        auto defines = mpScene->getSceneDefines();
        defines.add(mpRTXDI->getDefines());
        defines.add("GBUFFER_ADJUST_SHADING_NORMALS", mGBufferAdjustShadingNormals ? "1" : "0");
        defines.add("USE_ENV_BACKGROUND", mpScene->useEnvBackground() ? "1" : "0");
        defines.add(getValidResourceDefines(kOutputChannels, renderData));

        mpFinalShadingPass = ComputePass::create(mpDevice, desc, defines, true);
    }

    mpFinalShadingPass->addDefine("GBUFFER_ADJUST_SHADING_NORMALS", mGBufferAdjustShadingNormals ? "1" : "0");
    mpFinalShadingPass->addDefine("USE_ENV_BACKGROUND", mpScene->useEnvBackground() ? "1" : "0");

    // For optional I/O resources, set 'is_valid_<name>' defines to inform the program of which ones it can access.
    // TODO: This should be moved to a more general mechanism using Slang.
    mpFinalShadingPass->getProgram()->addDefines(getValidResourceDefines(kOutputChannels, renderData));

    mpFinalShadingPass["gScene"] = mpScene->getParameterBlock();

    auto var = mpFinalShadingPass["gFinalShading"];

    var["vbuffer"] = pVBuffer;
    var["frameDim"] = mFrameDim;
    mpRTXDI->setShaderData(mpFinalShadingPass->getRootVar());

    // Bind output channels as UAV buffers.
    var = mpFinalShadingPass->getRootVar();
    auto bind = [&](const ChannelDesc& channel) {
        Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
        var[channel.texname] = pTex;
    };
    for (const auto& channel : kOutputChannels) bind(channel);

    mpFinalShadingPass->execute(pRenderContext, mFrameDim.x, mFrameDim.y);
}
