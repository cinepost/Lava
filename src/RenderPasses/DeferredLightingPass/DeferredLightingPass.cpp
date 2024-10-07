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
    const std::string kInputDepth = "depth";
    const std::string kInputTexGrads = "texGrads";
    const std::string kInputNormalW = "normW";

    const std::string kInputMotionVectors = "mvec";
    const std::string kUseDOF = "useDOF";

    const std::string kShaderModel = "6_5";

    const std::string kVisibilityContainerParameterBlockName = "gVisibilityContainer";

    const ChannelList kExtraInputChannels = {
        { kInputDepth,            "gDepth",         "Depth buffer",                  true /* optional */, ResourceFormat::Unknown },
        { kInputTexGrads,         "gTextureGrads",  "Texture gradients",             true /* optional */, ResourceFormat::Unknown },
        { kInputNormalW,          "gNormW",         "Shading normal in world space", true /* optional */, ResourceFormat::Unknown },
        //{ kInputMotionVectors,    "gMotionVector",       "Motion vector buffer (float format)", true /* optional */ },
    };

    const ChannelList kExtraInputOutputChannels = {
    };

    const ChannelList kExtraOutputChannels = {
        { "normals",          "gOutNormals",        "Normals buffer",                true /* optional */, ResourceFormat::RGBA16Float },
        { "face_normals",     "gOutFaceNormals",    "Face Normals buffer",           true /* optional */, ResourceFormat::RGBA16Float    },
        { "Pz",               "gOutPz",             "Shading depth",                 true /* optional */, ResourceFormat::R32Float },
        { "posW",             "gOutPosition",       "Shading position",              true /* optional */, ResourceFormat::RGBA32Float },
        { "albedo",           "gOutAlbedo",         "Albedo color buffer",           true /* optional */, ResourceFormat::RGBA16Float },
        { "emission",         "gOutEmission",       "Emission color buffer",         true /* optional */, ResourceFormat::RGBA16Float },
        { "roughness",        "gOutRoughness",      "Roughness buffer",              true /* optional */, ResourceFormat::R16Float },
        { "tangent_normals",  "gOutTangentNormals", "Tangent space normals buffer",  true /* optional */, ResourceFormat::RGBA16Float },
        { "shadows",          "gOutShadows",        "Shadows buffer",                true /* optional */, ResourceFormat::RGBA16Float },
        { "occlusion",        "gOutOcclusion",      "Ambient occlusion buffer",      true /* optional */, ResourceFormat::R16Float },
        { "fresnel",          "gOutFresnel",        "Surface fresnel buffer",        true /* optional */, ResourceFormat::R16Float },
        { "motion_vecs",      "gOutMotionVecs",     "Motion vectors buffer",         true /* optional */, ResourceFormat::RG16Float },
        
        // Service outputs
        { "prim_id",          "gPrimID",            "Primitive id buffer",           true /* optional */, ResourceFormat::R32Float },
        { "op_id",            "gOpID",              "Operator id buffer",            true /* optional */, ResourceFormat::R32Float },
        { "variance",         "gVariance",          "Ray variance buffer",           true /* optional */, ResourceFormat::R16Float },
        { "uv",               "gUV",                "Texture coordinates buffer",    true /* optional */, ResourceFormat::RG16Float },
    };

    const std::string kFrameSampleCount = "frameSampleCount";
    const std::string kSuperSampleCount = "superSampleCount";
    const std::string kSuperSampling = "enableSuperSampling";
    const std::string kColorLimit = "colorLimit";
    const std::string kIndirectColorLimit = "indirectColorLimit";
    const std::string kUseSTBN = "useSTBN";
    const std::string kRayBias = "rayBias";
    const std::string kShadingRate = "shadingRate";
    const std::string kRayReflectLimit = "rayReflectLimit";
    const std::string kRayRefractLimit = "rayRefractLimit";
    const std::string kRayDiffuseLimit = "rayDiffuseLimit";
    const std::string kAreaLightsSamplingMode = "areaLightsSamplingMode";
    const std::string kRussianRouletteLevel = "russRoulleteLevel";
    const std::string kRayContributionThreshold = "rayContribThreshold";
}

DeferredLightingPass::SharedPtr DeferredLightingPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    auto pThis = SharedPtr(new DeferredLightingPass(pRenderContext->device()));
        
    for (const auto& [key, value] : dict) {
        if (key == kFrameSampleCount) pThis->setFrameSampleCount(value);
        else if (key == kColorLimit) pThis->setColorLimit(value);
        else if (key == kIndirectColorLimit) pThis->setIndirectColorLimit(value);
        else if (key == kUseSTBN) pThis->setSTBNSampling(value);
        else if (key == kRayBias) pThis->setRayBias(value);
        else if (key == kShadingRate) pThis->setShadingRate(value);
        else if (key == kRayReflectLimit) pThis->setRayReflectLimit(value);
        else if (key == kRayRefractLimit) pThis->setRayRefractLimit(value);
        else if (key == kRayDiffuseLimit) pThis->setRayDiffuseLimit(value);
        #ifdef _WIN32
        // else if (key == kAreaLightsSamplingMode) pThis->setAreaLightsSamplingMode(std::string{value});
        else if (key == kAreaLightsSamplingMode) pThis->setAreaLightsSamplingMode(value.operator std::string());
        // else if (key == kAreaLightsSamplingMode) pThis->setAreaLightsSamplingMode(static_cast<const std::string&>(value));
        #else
        else if (key == kAreaLightsSamplingMode) pThis->setAreaLightsSamplingMode(std::string(value));
        #endif
        else if (key == kRussianRouletteLevel) pThis->setRussRoulleteLevel((uint)value);
        else if (key == kRayContributionThreshold) pThis->setRayContribThreshold(value);
        else if (key == kUseDOF) pThis->enableDepthOfField(static_cast<bool>(value));
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
    addRenderPassInputOutputs(reflector, kExtraInputOutputChannels, Resource::BindFlags::UnorderedAccess);
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
    mpShadingPass = nullptr;
    mDirty = true;
}

void DeferredLightingPass::execute(RenderContext* pContext, const RenderData& renderData) {
    if (!mpScene) return;

    if (!mpDevice->isFeatureSupported(Device::SupportedFeatures::RaytracingTier1_1)) {
        throw std::runtime_error("DeferredLightingPass: Raytracing Tier 1.1 is not supported by the current device");
    }

    mUseVariance = false;

    createBuffers(pContext, renderData);

    const bool shadingRateInShader = true;

    auto createShadingPass = [this, renderData, shadingRateInShader](const Program::Desc& desc, bool transparentPass = false) {
        if(transparentPass && !mpVisibilitySamplesContainer) return ComputePass::SharedPtr(nullptr);

        auto defines = mpScene->getSceneDefines();
        defines.add(getValidResourceDefines(kExtraInputChannels, renderData));
        defines.add(getValidResourceDefines(kExtraInputOutputChannels, renderData));
        defines.add(getValidResourceDefines(kExtraOutputChannels, renderData));

        // AOV channels processing
        Texture::SharedPtr pAovNormalsTex = renderData["normals"]->asTexture();
        if(pAovNormalsTex) {
            if(isFloatFormat(pAovNormalsTex->getFormat())) defines.add("_AOV_NORMALS_FLOAT", "");
        }

        if (mpEmissiveSampler) defines.add(mpEmissiveSampler->getDefines());

        // Sampling / Shading
        if ((mShadingRate > 1) && shadingRateInShader) defines.add("_SHADING_RATE", std::to_string(mShadingRate));

        defines.add("_USE_STBN_SAMPLING", mUseSTBN ? "1" : "0");
        defines.add("_USE_VARIANCE", mUseVariance ? "1" : "0");
        defines.add("USE_ANALYTIC_LIGHTS", mpScene->useAnalyticLights() ? "1" : "0");
        defines.add("USE_EMISSIVE_LIGHTS", mpScene->useEmissiveLights() ? "1" : "0");
        defines.add("USE_DEPTH_OF_FIELD", mUseDOF ? "1" : "0");

        // Visibility container
        if(mpVisibilitySamplesContainer) {
            if(transparentPass) defines.add("TRANSPARENT_SHADING_PASS");
            defines.add("USE_VISIBILITY_CONTAINER", "1");
            defines.add("GROUP_SIZE_X", to_string(mpVisibilitySamplesContainer->getShadingThreadGroupSize().x));
            defines.add(mpVisibilitySamplesContainer->getDefines());
        } else {
            defines.remove("TRANSPARENT_SHADING_PASS");
            defines.remove("USE_VISIBILITY_CONTAINER");
        }

        defines.add("is_valid_gLastFrameSum", mpLastFrameSum != nullptr ? "1" : "0");
        if (mEnableSuperSampling) defines.add("INTERPOLATION_MODE", "sample");
        
        uint maxRayLevel = std::max(std::max(mRayDiffuseLimit, mRayReflectLimit), mRayRefractLimit);
        if(maxRayLevel > 0) defines.add("_MAX_RAY_LEVEL", std::to_string(maxRayLevel));

        uint32_t scene_lights_count = mpScene->getLightCount();
        defines.add("_SCENE_LIGHT_COUNT", (scene_lights_count > 0) ? std::to_string(scene_lights_count) : "0");

        defines.add("AREA_LIGHTS_SAMPLING_SPHERICAL_SOLID_ANGLE_URENA", 
            (mAreaLightsSamplingMode == AnalyticAreaLight::LightSamplingMode::SPHERICAL_SOLID_ANGLE) ? "1" : "0");

        if (mpSampleGenerator) defines.add(mpSampleGenerator->getDefines());

        ComputePass::SharedPtr pPass = ComputePass::create(mpDevice, desc, defines, true);

        pPass["gScene"] = mpScene->getParameterBlock();

        pPass["gNoiseSampler"] = mpNoiseSampler;
        pPass["gNoiseTex"]     = mpBlueNoiseTexture;

        // Bind mandatory input channels
        pPass["gInOutColor"] = renderData[kInputColor]->asTexture();
        pPass["gVbuffer"] = renderData[kInputVBuffer]->asTexture();
        pPass["gLastFrameSum"] = mpLastFrameSum;

        // Bind extra input channels
        for (const auto& channel : kExtraInputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            pPass[channel.texname] = pTex;
        }

        // Bind extra output channels as UAV buffers.
        for (const auto& channel : kExtraOutputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            pPass[channel.texname] = pTex;
        }

        if(mpVisibilitySamplesContainer) {
            pPass[kVisibilityContainerParameterBlockName].setParameterBlock(mpVisibilitySamplesContainer->getParameterBlock());
        }

        return pPass;
    };
    
    // Prepare program and vars. This may trigger shader compilation.
    // The program should have all necessary defines set at this point.
    if (!mpShadingPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("main");
        desc.addTypeConformances(mpScene->getTypeConformances());

        mpShadingPass = createShadingPass(desc);
        mpTransparentShadingPass = createShadingPass(desc, true);
    }

    float2 f = mpNoiseOffsetGenerator->next();
    uint2 noiseOffset = {64 * (f[0] + 0.5f), 64 * (f[1] + 0.5f)};

    auto setVar = [this, noiseOffset](ShaderVar var, bool transparentPass = false) {
        var["gFrameDim"] = mFrameDim;
        var["gNoiseOffset"] = noiseOffset;
        var["gSamplesPerFrame"]  = mFrameSampleCount;
        var["gSampleNumber"] = mSampleNumber++;
        var["gColorLimit"] = mColorLimit;
        var["gIndirectColorLimit"] = mIndirectColorLimit;
        var["gRayDiffuseLimit"] = mRayDiffuseLimit;
        var["gRayReflectLimit"] = mRayReflectLimit;
        var["gRayRefractLimit"] = mRayRefractLimit;
        var["gRayBias"] = mRayBias;
        var["gRayContribThresh"] = mRayContribThreshold;
        var["gRussRouletteLevel"] = mRussRouletteLevel;
    };

    setVar(mpShadingPass["PerFrameCB"]);
    if(mpTransparentShadingPass) setVar(mpTransparentShadingPass["PerFrameCB"], true);

    if(shadingRateInShader) {
        if(mpVisibilitySamplesContainer) {
            // Visibility container mode
            mpShadingPass->executeIndirect(pContext, mpVisibilitySamplesContainer->getOpaquePassIndirectionArgsBuffer().get());
            mpTransparentShadingPass->executeIndirect(pContext, mpVisibilitySamplesContainer->getTransparentPassIndirectionArgsBuffer().get());
        } else {
            // Legacy (visibility buffer) mode shading
            mpShadingPass->execute(pContext, mFrameDim.x, mFrameDim.y);    
        }
    } else {
        for(uint32_t i = 0; i < mShadingRate; ++i){
            if(mpVisibilitySamplesContainer) {
                mpShadingPass->executeIndirect(pContext, mpVisibilitySamplesContainer->getOpaquePassIndirectionArgsBuffer().get());
                mpTransparentShadingPass->executeIndirect(pContext, mpVisibilitySamplesContainer->getTransparentPassIndirectionArgsBuffer().get());
                mpTransparentShadingPass["PerFrameCB"]["gSampleNumber"] = mSampleNumber;
            } else {
                // Legacy (visibility buffer) mode shading
                mpShadingPass->execute(pContext, mFrameDim.x, mFrameDim.y);
                mpShadingPass["PerFrameCB"]["gSampleNumber"] = mSampleNumber;
            }
            mSampleNumber++;
        }
    }
    mDirty = false;
}

void DeferredLightingPass::createBuffers(RenderContext* pContext, const RenderData& renderData) {
    if(!mDirty) return;

    if(mUseVariance) {
        // RGB - Last fram sum, A - variance
        mpLastFrameSum = Texture::create2D(mpDevice, mFrameDim.x, mFrameDim.y, ResourceFormat::RGBA16Float, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
        pContext->clearUAV(mpLastFrameSum->getUAV().get(), float4(0.f, 0.f, 0.f, 1.f));
    } else {
        mpLastFrameSum = nullptr;
    }
}

void DeferredLightingPass::setVisibilitySamplesContainer(VisibilitySamplesContainer::SharedConstPtr pVisibilitySamplesContainer) {
    if(mpVisibilitySamplesContainer == pVisibilitySamplesContainer) return;
    mpVisibilitySamplesContainer = pVisibilitySamplesContainer;
    mDirty = true;
}

DeferredLightingPass& DeferredLightingPass::setRayReflectLimit(int limit) {
    uint _limit = std::max(0u, std::min(10u, static_cast<uint>(limit)));
    if(mRayReflectLimit == _limit) return *this;
    mRayReflectLimit = _limit;
    mDirty = true;
    return *this;
}

DeferredLightingPass& DeferredLightingPass::setRayRefractLimit(int limit) {
    uint _limit = std::max(0u, std::min(10u, static_cast<uint>(limit)));
    if(mRayRefractLimit == _limit) return *this;
    mRayRefractLimit = _limit;
    mDirty = true;
    return *this;
}

DeferredLightingPass& DeferredLightingPass::setRayDiffuseLimit(int limit) {
    uint _limit = std::max(0u, std::min(10u, static_cast<uint>(limit)));
    if(mRayDiffuseLimit == _limit) return *this;
    mRayDiffuseLimit = _limit;
    mDirty = true;
    return *this;
}

DeferredLightingPass& DeferredLightingPass::setRayBias(float bias) {
    mRayBias = bias;
    return *this;
}

DeferredLightingPass& DeferredLightingPass::setColorLimit(const float3& limit) {
    mColorLimit = (float16_t3)limit;
    return *this;
}

DeferredLightingPass& DeferredLightingPass::setIndirectColorLimit(const float3& limit) {
    mIndirectColorLimit = (float16_t3)limit;
    return *this;
}

DeferredLightingPass& DeferredLightingPass::setShadingRate(int rate) {
    rate = std::max(1u, static_cast<uint>(rate));
    if(mShadingRate == rate) return *this;
    mShadingRate = rate;
    mDirty = true;
    return *this;
}

DeferredLightingPass& DeferredLightingPass::setSTBNSampling(bool enable) {
    if (mUseSTBN == enable) return *this;
    mUseSTBN = enable;
    mDirty = true;
    return *this;
}

DeferredLightingPass& DeferredLightingPass::setAreaLightsSamplingMode(const std::string& areaLightsSamplingModeName) {
    if (areaLightsSamplingModeName == "sa") {
        return setAreaLightsSamplingMode(AnalyticAreaLight::LightSamplingMode::SOLID_ANGLE);
    } else if (areaLightsSamplingModeName == "urena") {
        return setAreaLightsSamplingMode(AnalyticAreaLight::LightSamplingMode::SPHERICAL_SOLID_ANGLE);
    } else {
        return setAreaLightsSamplingMode(AnalyticAreaLight::LightSamplingMode::LTC);
    }
}

DeferredLightingPass& DeferredLightingPass::setAreaLightsSamplingMode(AnalyticAreaLight::LightSamplingMode areaLightsSamplingMode) {
    if(mAreaLightsSamplingMode == areaLightsSamplingMode) return *this;
    mAreaLightsSamplingMode = areaLightsSamplingMode;
    mDirty = true;
    return *this;
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

void DeferredLightingPass::setRayContribThreshold(float value) {
    float _value = std::min(1.f, std::max(0.f, value));
    if(mRayContribThreshold == _value) return;
    mRayContribThreshold = _value;
}

void DeferredLightingPass::setRussRoulleteLevel(uint value) {
    if(mRussRouletteLevel == value) return;
    mRussRouletteLevel = value;
}

void DeferredLightingPass::enableDepthOfField(bool value) {
    if(mUseDOF == value) return;
    mUseDOF = value;
    mDirty = true;
}