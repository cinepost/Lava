#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Utils/SampleGenerators/StratifiedSamplePattern.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Utils/Textures/BlueNoiseTexture.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"

#include "glm/gtc/random.hpp"

#include "glm/gtx/string_cast.hpp"

#include "DeferredLightingPass.h"


const RenderPass::Info DeferredLightingPass::kInfo
{
    "DeferredLightingPass",

    "Computes direct and indirect illumination and applies shadows for the current scene (if visibility map is provided).\n"
    "The pass can output the world-space normals and screen-space motion vectors, both are optional."
};

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(DeferredLightingPass::kInfo, DeferredLightingPass::create);
}

namespace {
    const char kShaderFile[] = "RenderPasses/DeferredLightingPass/DeferredLightingPass.cs.slang";

    const std::string kInputColor = "color";
    const std::string kInputVBuffer = "vbuffer";
    const std::string kInputTexGrads = "texGrads";
    const std::string kInputMotionVectors = "mvec";

    const std::string kShaderModel = "6_5";

    const ChannelList kExtraInputChannels = {
        { kInputTexGrads,           "gTextureGrads",            "Texture gradients", true /* optional */, ResourceFormat::Unknown },
        //{ kInputMotionVectors,      "gMotionVector",            "Motion vector buffer (float format)", true /* optional */ },
    };

    const ChannelList kExtraOutputChannels = {
        { "posW",             "gOutPosition",       "World position buffer",         true /* optional */, ResourceFormat::RGBA32Float },
        { "albedo",           "gOutAlbedo",         "Albedo color buffer",           true /* optional */, ResourceFormat::RGBA16Float },
        { "normals",          "gOutNormals",        "Normals buffer",                true /* optional */, ResourceFormat::RGBA16Float },
        { "shadows",          "gOutShadows",        "Shadows buffer",                true /* optional */, ResourceFormat::RGBA16Float },
        { "occlusion",        "gOutOcclusion",      "Ambient occlusion buffer",      true /* optional */, ResourceFormat::R16Float },
        { "motion_vecs",      "gOutMotionVecs",     "Motion vectors buffer",         true /* optional */, ResourceFormat::RG16Float },
        { "prim_id",          "gPrimID",            "Primitive id buffer",           true /* optional */, ResourceFormat::R32Float },
        { "op_id",            "gOpID",              "Operator id buffer",            true /* optional */, ResourceFormat::R32Float },
    };

    const std::string kFrameSampleCount = "frameSampleCount";
    const std::string kSuperSampleCount = "superSampleCount";
    const std::string kSuperSampling = "enableSuperSampling";
}

DeferredLightingPass::SharedPtr DeferredLightingPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    auto pThis = SharedPtr(new DeferredLightingPass(pRenderContext->device()));
        
    for (const auto& [key, value] : dict) {
        if (key == kSuperSampleCount) pThis->setSuperSampleCount(value);
        else if (key == kFrameSampleCount) pThis->setFrameSampleCount(value);
        else if (key == kSuperSampling) pThis->setSuperSampling(value);
        else LLOG_WRN << "Unknown field '" << key << "' in a DeferredLightingPass dictionary";
    }

    return pThis;
}

Dictionary DeferredLightingPass::getScriptingDictionary() {
    Dictionary d;
    d[kFrameSampleCount] = mFrameSampleCount;
    d[kSuperSampleCount] = mSuperSampleCount;
    d[kSuperSampling] = mEnableSuperSampling;
    return d;
}

DeferredLightingPass::DeferredLightingPass(Device::SharedPtr pDevice): RenderPass(pDevice, kInfo) {
    // Create a GPU sample generator.
    mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_UNIFORM);
    
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point)
        .setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap);
    mpNoiseSampler = Sampler::create(pDevice, samplerDesc);

    mSampleNumber = 0;
}

RenderPassReflection DeferredLightingPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;

    const auto& texDims = compileData.defaultTexDims;

    reflector.addInputOutput(kInputColor, "Color buffer").format(ResourceFormat::Unknown);
    reflector.addInput(kInputVBuffer, "Visibility buffer in packed format").format(ResourceFormat::RGBA32Uint);
    
    addRenderPassInputs(reflector, kExtraInputChannels);
    addRenderPassOutputs(reflector, kExtraOutputChannels, Resource::BindFlags::UnorderedAccess);

    return reflector;
}

void DeferredLightingPass::compile(RenderContext* pRenderContext, const CompileData& compileData) {
    mDirty = true;
    mFrameDim = compileData.defaultTexDims;
    auto pDevice = pRenderContext->device();

    mpNoiseOffsetGenerator = StratifiedSamplePattern::create(mFrameSampleCount);
    mpBlueNoiseTexture = BlueNoiseTexture::create(pDevice);
}

void DeferredLightingPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    if(mpScene == pScene) return;
    mpScene = pScene;
    mpLightingPass = nullptr;
}

void DeferredLightingPass::execute(RenderContext* pContext, const RenderData& renderData) {
    if (!mpScene) return;
    
    // Prepare program and vars. This may trigger shader compilation.
    // The program should have all necessary defines set at this point.
    if (!mpLightingPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("main");
        desc.addTypeConformances(mpScene->getTypeConformances());

        auto defines = mpScene->getSceneDefines();
        defines.add(getValidResourceDefines(kExtraInputChannels, renderData));
        defines.add(getValidResourceDefines(kExtraOutputChannels, renderData));
        
        // AOV channels processing
        //const auto& pAovNormalsTex = renderData.getTexture("normals");
        Texture::SharedPtr pAovNormalsTex = renderData["normals"]->asTexture();
        if(pAovNormalsTex) {
            if(isFloatFormat(pAovNormalsTex->getFormat())) defines.add("_AOV_NORMALS_FLOAT", "");
        }

        // Super sampling
        if (mEnableSuperSampling) defines.add("INTERPOLATION_MODE", "sample");

        // Sample generator
        if (mpSampleGenerator) defines.add(mpSampleGenerator->getDefines());

        mpLightingPass = ComputePass::create(mpDevice, desc, defines, true);

        mpLightingPass["gScene"] = mpScene->getParameterBlock();

        mpLightingPass["gNoiseSampler"] = mpNoiseSampler;
        mpLightingPass["gNoiseTex"]     = mpBlueNoiseTexture;

        // Bind mandatory input channels
        mpLightingPass["gInOutColor"] = renderData[kInputColor]->asTexture();
        mpLightingPass["gVbuffer"] = renderData[kInputVBuffer]->asTexture();
    
        // Bind extra input channels
        for (const auto& channel : kExtraInputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            mpLightingPass[channel.texname] = pTex;
        }

        // Bind extra output channels as UAV buffers.
        for (const auto& channel : kExtraOutputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            mpLightingPass[channel.texname] = pTex;
        }
        mDirty = false;
    }

    float2 f = mpNoiseOffsetGenerator->next();
    uint2 noiseOffset = {64 * (f[0] + 0.5f), 64 * (f[1] + 0.5f)};

    auto cb_var = mpLightingPass["PerFrameCB"];

    cb_var["gFrameDim"] = mFrameDim;
    cb_var["gNoiseOffset"] = noiseOffset;
    cb_var["gViewInvMat"] = glm::inverse(mpScene->getCamera()->getViewMatrix());
    cb_var["gSamplesPerFrame"]  = mFrameSampleCount;
    cb_var["gSampleNumber"] = mSampleNumber++;
    cb_var["gBias"] = 0.001f;
    
    mpLightingPass->execute(pContext, mFrameDim.x, mFrameDim.y);
}

DeferredLightingPass& DeferredLightingPass::setFrameSampleCount(uint32_t samples) {
    if (mFrameSampleCount == samples) return *this;

    mFrameSampleCount = samples;
    mDirty = true;
    return *this;
}

DeferredLightingPass& DeferredLightingPass::setSuperSampleCount(uint32_t samples) {
    if (mFrameSampleCount == samples) return *this;

    mSuperSampleCount = samples;
    mDirty = true;
    return *this;
}

DeferredLightingPass& DeferredLightingPass::setSuperSampling(bool enable) {
    if(mEnableSuperSampling == enable) return *this;
    mEnableSuperSampling = enable;
    mDirty = true;
    return *this;
}