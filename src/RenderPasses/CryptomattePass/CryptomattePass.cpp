#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"

#include "glm/gtc/random.hpp"

#include "glm/gtx/string_cast.hpp"

#include "CryptomattePass.h"


const RenderPass::Info CryptomattePass::kInfo
{
    "CryptomattePass",

    "Computes direct and indirect illumination and applies shadows for the current scene (if visibility map is provided).\n"
    "The pass can output the world-space normals and screen-space motion vectors, both are optional."
};

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(CryptomattePass::kInfo, CryptomattePass::create);
}

namespace {
    const char kShaderFile[] = "RenderPasses/CryptomattePass/CryptomattePass.cs.slang";

    const std::string kInputVBuffer = "vbuffer";
    
    const std::string kShaderModel = "6_5";

    const ChannelList kExtraOutputChannels = {
        { "material_color",   "gMatColor",          "Cryptomatte material false color",         true /* optional */, ResourceFormat::RGBA16Float },
        { "instance_color",   "gObjColor",          "Cryptomatte object false color",           true /* optional */, ResourceFormat::RGBA16Float },
        { "custattr_color",   "gAttribColor",       "Cryptomatte custom attribute false color", true /* optional */, ResourceFormat::RGBA16Float },
    };
}

CryptomattePass::SharedPtr CryptomattePass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    auto pThis = SharedPtr(new CryptomattePass(pRenderContext->device()));
        
    for (const auto& [key, value] : dict) {
        
    }

    return pThis;
}

Dictionary CryptomattePass::getScriptingDictionary() {
    Dictionary d;
    return d;
}

CryptomattePass::CryptomattePass(Device::SharedPtr pDevice): RenderPass(pDevice, kInfo) {

}

RenderPassReflection CryptomattePass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;

    const auto& texDims = compileData.defaultTexDims;

    reflector.addInput(kInputVBuffer, "Visibility buffer in packed format").format(ResourceFormat::RGBA32Uint);
    addRenderPassOutputs(reflector, kExtraOutputChannels, Resource::BindFlags::UnorderedAccess);

    return reflector;
}

void CryptomattePass::compile(RenderContext* pRenderContext, const CompileData& compileData) {
    mDirty = true;
    mFrameDim = compileData.defaultTexDims;    
}

void CryptomattePass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    if(mpScene == pScene) return;
    mpScene = pScene;
    
}

void CryptomattePass::execute(RenderContext* pContext, const RenderData& renderData) {
    if (!mpScene) return;
    
    // Prepare program and vars. This may trigger shader compilation.
    // The program should have all necessary defines set at this point.
    if (mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("main");
        desc.addTypeConformances(mpScene->getTypeConformances());

        auto defines = mpScene->getSceneDefines();
        defines.add(getValidResourceDefines(kExtraOutputChannels, renderData));
        
        mpPass = ComputePass::create(mpDevice, desc, defines, true);

        mpPass["gScene"] = mpScene->getParameterBlock();

        // Bind mandatory input channels
        mpPass["gVbuffer"] = renderData[kInputVBuffer]->asTexture();

        // Bind extra output channels as UAV buffers.
        for (const auto& channel : kExtraOutputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            mpPass[channel.texname] = pTex;
        }
        mDirty = false;
    }

    auto cb_var = mpPass["PerFrameCB"];

    cb_var["gFrameDim"] = mFrameDim;
    mpPass->execute(pContext, mFrameDim.x, mFrameDim.y);
}
