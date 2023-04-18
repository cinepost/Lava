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
#include "Experimental/Scene/Lights/EmissiveUniformSampler.h"

#include "PathTracer.h"

#define USE_PIXELSTATS 0

const RenderPass::Info PathTracer::kInfo { "PathTracer", "Reference path tracer." };

namespace {
    const std::string kGeneratePathsFilename = "RenderPasses/PathTracer/GeneratePaths.cs.slang";
    const std::string kTracePassFilename = "RenderPasses/PathTracer/TracePass.rt.slang";
    const std::string kResolvePassFilename = "RenderPasses/PathTracer/ResolvePass.cs.slang";
    const std::string kReflectTypesFile = "RenderPasses/PathTracer/ReflectTypes.cs.slang";

    const std::string kShaderModel = "6_5";

    // Render pass inputs and outputs.
    const std::string kInputVBuffer = "vbuffer";
    const std::string kInputMotionVectors = "mvec";
    const std::string kInputViewDir = "viewW";
    const std::string kInputSampleCount = "sampleCount";

    const Falcor::ChannelList kInputChannels = {
        //{ kInputVBuffer,        "gVBuffer",         "Visibility buffer in packed format" },
        { kInputVBuffer,        "gVBuffer",         "Visibility buffer in packed format", false, ResourceFormat::RGBA32Uint},
        { kInputMotionVectors,  "gMotionVectors",   "Motion vector buffer (float format)", true /* optional */ },
        { kInputViewDir,        "gViewW",           "World-space view direction (xyz float format)", true /* optional */ },
        { kInputSampleCount,    "gSampleCount",     "Sample count buffer (integer format)", true /* optional */, ResourceFormat::R8Uint },
    };

    const std::string kOutputColor = "color";
    const std::string kOutputAlbedo = "albedo";
    const std::string kOutputSpecularAlbedo = "specularAlbedo";
    const std::string kOutputIndirectAlbedo = "indirectAlbedo";
    const std::string kOutputNormal = "normal";
    const std::string kOutputReflectionPosW = "reflectionPosW";
    const std::string kOutputRayCount = "rayCount";
    const std::string kOutputPathLength = "pathLength";
    const std::string kOutputNRDDiffuseRadianceHitDist = "nrdDiffuseRadianceHitDist";
    const std::string kOutputNRDSpecularRadianceHitDist = "nrdSpecularRadianceHitDist";
    const std::string kOutputNRDEmission = "nrdEmission";
    const std::string kOutputNRDDiffuseReflectance = "nrdDiffuseReflectance";
    const std::string kOutputNRDSpecularReflectance = "nrdSpecularReflectance";
    const std::string kOutputNRDDeltaReflectionRadianceHitDist = "nrdDeltaReflectionRadianceHitDist";
    const std::string kOutputNRDDeltaReflectionReflectance = "nrdDeltaReflectionReflectance";
    const std::string kOutputNRDDeltaReflectionEmission = "nrdDeltaReflectionEmission";
    const std::string kOutputNRDDeltaReflectionNormWRoughMaterialID = "nrdDeltaReflectionNormWRoughMaterialID";
    const std::string kOutputNRDDeltaReflectionPathLength = "nrdDeltaReflectionPathLength";
    const std::string kOutputNRDDeltaReflectionHitDist = "nrdDeltaReflectionHitDist";
    const std::string kOutputNRDDeltaTransmissionRadianceHitDist = "nrdDeltaTransmissionRadianceHitDist";
    const std::string kOutputNRDDeltaTransmissionReflectance = "nrdDeltaTransmissionReflectance";
    const std::string kOutputNRDDeltaTransmissionEmission = "nrdDeltaTransmissionEmission";
    const std::string kOutputNRDDeltaTransmissionNormWRoughMaterialID = "nrdDeltaTransmissionNormWRoughMaterialID";
    const std::string kOutputNRDDeltaTransmissionPathLength = "nrdDeltaTransmissionPathLength";
    const std::string kOutputNRDDeltaTransmissionPosW = "nrdDeltaTransmissionPosW";
    const std::string kOutputNRDResidualRadianceHitDist = "nrdResidualRadianceHitDist";

    const Falcor::ChannelList kOutputChannels = {
        { kOutputColor,                                     "",     "Output color (linear)", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputAlbedo,                                    "",     "Output albedo (linear)", true /* optional */, ResourceFormat::RGBA8Unorm },
        { kOutputSpecularAlbedo,                            "",     "Output specular albedo (linear)", true /* optional */, ResourceFormat::RGBA8Unorm },
        { kOutputIndirectAlbedo,                            "",     "Output indirect albedo (linear)", true /* optional */, ResourceFormat::RGBA8Unorm },
        { kOutputNormal,                                    "",     "Output normal (linear)", true /* optional */, ResourceFormat::RGBA16Float },
        { kOutputReflectionPosW,                            "",     "Output reflection pos (world space)", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputRayCount,                                  "",     "Per-pixel ray count", true /* optional */, ResourceFormat::R32Uint },
        { kOutputPathLength,                                "",     "Per-pixel path length", true /* optional */, ResourceFormat::R32Uint },
        // NRD outputs
        { kOutputNRDDiffuseRadianceHitDist,                 "",     "Output demodulated diffuse color (linear) and hit distance", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDSpecularRadianceHitDist,                "",     "Output demodulated specular color (linear) and hit distance", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDEmission,                               "",     "Output primary surface emission", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDDiffuseReflectance,                     "",     "Output primary surface diffuse reflectance", true /* optional */, ResourceFormat::RGBA16Float },
        { kOutputNRDSpecularReflectance,                    "",     "Output primary surface specular reflectance", true /* optional */, ResourceFormat::RGBA16Float },
        { kOutputNRDDeltaReflectionRadianceHitDist,         "",     "Output demodulated delta reflection color (linear)", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDDeltaReflectionReflectance,             "",     "Output delta reflection reflectance color (linear)", true /* optional */, ResourceFormat::RGBA16Float },
        { kOutputNRDDeltaReflectionEmission,                "",     "Output delta reflection emission color (linear)", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDDeltaReflectionNormWRoughMaterialID,    "",     "Output delta reflection world normal, roughness, and material ID", true /* optional */, ResourceFormat::RGB10A2Unorm },
        { kOutputNRDDeltaReflectionPathLength,              "",     "Output delta reflection path length", true /* optional */, ResourceFormat::R16Float },
        { kOutputNRDDeltaReflectionHitDist,                 "",     "Output delta reflection hit distance", true /* optional */, ResourceFormat::R16Float },
        { kOutputNRDDeltaTransmissionRadianceHitDist,       "",     "Output demodulated delta transmission color (linear)", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDDeltaTransmissionReflectance,           "",     "Output delta transmission reflectance color (linear)", true /* optional */, ResourceFormat::RGBA16Float },
        { kOutputNRDDeltaTransmissionEmission,              "",     "Output delta transmission emission color (linear)", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDDeltaTransmissionNormWRoughMaterialID,  "",     "Output delta transmission world normal, roughness, and material ID", true /* optional */, ResourceFormat::RGB10A2Unorm },
        { kOutputNRDDeltaTransmissionPathLength,            "",     "Output delta transmission path length", true /* optional */, ResourceFormat::R16Float },
        { kOutputNRDDeltaTransmissionPosW,                  "",     "Output delta transmission position", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDResidualRadianceHitDist,                "",     "Output residual color (linear) and hit distance", true /* optional */, ResourceFormat::RGBA32Float },
    };

    // Scripting options.
    const std::string kSamplesPerPixel = "samplesPerPixel";
    const std::string kMaxSurfaceBounces = "maxSurfaceBounces";
    const std::string kMaxDiffuseBounces = "maxDiffuseBounces";
    const std::string kMaxSpecularBounces = "maxSpecularBounces";
    const std::string kMaxTransmissionBounces = "maxTransmissionBounces";

    const std::string kSampleGenerator = "sampleGenerator";
    const std::string kFixedSeed = "fixedSeed";
    const std::string kUseBSDFSampling = "useBSDFSampling";
    const std::string kUseRussianRoulette = "useRussianRoulette";
    const std::string kUseNEE = "useNEE";
    const std::string kUseMIS = "useMIS";
    const std::string kMISHeuristic = "misHeuristic";
    const std::string kMISPowerExponent = "misPowerExponent";
    const std::string kEmissiveSampler = "emissiveSampler";
    const std::string kLightBVHOptions = "lightBVHOptions";
    const std::string kUseRTXDI = "useRTXDI";
    const std::string kRTXDIOptions = "RTXDIOptions";

    const std::string kUseAlphaTest = "useAlphaTest";
    const std::string kAdjustShadingNormals = "adjustShadingNormals";
    const std::string kMaxNestedMaterials = "maxNestedMaterials";
    const std::string kUseLightsInDielectricVolumes = "useLightsInDielectricVolumes";
    const std::string kDisableCaustics = "disableCaustics";
    const std::string kSpecularRoughnessThreshold = "specularRoughnessThreshold";
    const std::string kPrimaryLodMode = "primaryLodMode";
    const std::string kLODBias = "lodBias";

    const std::string kOutputSize = "outputSize";
    const std::string kFixedOutputSize = "fixedOutputSize";
    const std::string kColorFormat = "colorFormat";

    const std::string kUseNRDDemodulation = "useNRDDemodulation";
}

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(PathTracer::kInfo, PathTracer::create);
    ScriptBindings::registerBinding(PathTracer::registerBindings);
}

void PathTracer::registerBindings(pybind11::module& m) {
    pybind11::enum_<ColorFormat> colorFormat(m, "ColorFormat");
    colorFormat.value("RGBA32F", ColorFormat::RGBA32F);
    colorFormat.value("LogLuvHDR", ColorFormat::LogLuvHDR);

    pybind11::enum_<MISHeuristic> misHeuristic(m, "MISHeuristic");
    misHeuristic.value("Balance", MISHeuristic::Balance);
    misHeuristic.value("PowerTwo", MISHeuristic::PowerTwo);
    misHeuristic.value("PowerExp", MISHeuristic::PowerExp);

    pybind11::class_<PathTracer, RenderPass, PathTracer::SharedPtr> pass(m, "PathTracer");
    pass.def_property_readonly("pixelStats", &PathTracer::getPixelStats);

    pass.def_property("useFixedSeed",
        [](const PathTracer* pt) { return pt->mParams.useFixedSeed ? true : false; },
        [](PathTracer* pt, bool value) { pt->mParams.useFixedSeed = value ? 1 : 0; }
    );
    pass.def_property("fixedSeed",
        [](const PathTracer* pt) { return pt->mParams.fixedSeed; },
        [](PathTracer* pt, uint32_t value) { pt->mParams.fixedSeed = value; }
    );
}

PathTracer::SharedPtr PathTracer::create(RenderContext* pRenderContext, const Dictionary& dict) {
    return SharedPtr(new PathTracer(pRenderContext->device(), dict));
}

PathTracer::PathTracer(Device::SharedPtr pDevice, const Dictionary& dict): RenderPass(pDevice, kInfo) {
    if (!pDevice->isShaderModelSupported(Device::ShaderModel::SM6_5)) {
        throw std::runtime_error("PathTracer: Shader Model 6.5 is not supported by the current device");
    }

    if (!pDevice->isFeatureSupported(Device::SupportedFeatures::RaytracingTier1_1)) {
        throw std::runtime_error("PathTracer: Raytracing Tier 1.1 is not supported by the current device");
    }

    parseDictionary(dict);
    validateOptions();

    // Create sample generator.
    mpSampleGenerator = SampleGenerator::create(mStaticParams.sampleGenerator);

    // Create programs.
    auto defines = mStaticParams.getDefines(*this);

    mpGeneratePaths = ComputePass::create(mpDevice, Program::Desc(kGeneratePathsFilename).setShaderModel(kShaderModel).csEntry("main"), defines, false);
    mpResolvePass = ComputePass::create(mpDevice, Program::Desc(kResolvePassFilename).setShaderModel(kShaderModel).csEntry("main"), defines, false);
    mpReflectTypes = ComputePass::create(mpDevice, Program::Desc(kReflectTypesFile).setShaderModel(kShaderModel).csEntry("main"), defines, false);

    // Note: The trace pass program is lazily created in updatePrograms() because a scene needs to be present when creating it.

#if USE_PIXELSTATS
    mpPixelStats = PixelStats::create(mpDevice);
#endif

    mpPixelDebug = PixelDebug::create(mpDevice);
}

void PathTracer::parseDictionary(const Dictionary& dict) {
    for (const auto& [key, value] : dict) {
        // Rendering parameters
        if (key == kSamplesPerPixel) mStaticParams.samplesPerPixel = value;
        else if (key == kMaxSurfaceBounces) mStaticParams.maxSurfaceBounces = value;
        else if (key == kMaxDiffuseBounces) mStaticParams.maxDiffuseBounces = value;
        else if (key == kMaxSpecularBounces) mStaticParams.maxSpecularBounces = value;
        else if (key == kMaxTransmissionBounces) mStaticParams.maxTransmissionBounces = value;

        // Sampling parameters
        else if (key == kSampleGenerator) mStaticParams.sampleGenerator = value;
        else if (key == kFixedSeed) { mParams.fixedSeed = value; mParams.useFixedSeed = true; }
        else if (key == kUseBSDFSampling) mStaticParams.useBSDFSampling = value;
        else if (key == kUseRussianRoulette) mStaticParams.useRussianRoulette = value;
        else if (key == kUseNEE) mStaticParams.useNEE = value;
        else if (key == kUseMIS) mStaticParams.useMIS = value;
        else if (key == kMISHeuristic) mStaticParams.misHeuristic = value;
        else if (key == kMISPowerExponent) mStaticParams.misPowerExponent = value;
        else if (key == kEmissiveSampler) mStaticParams.emissiveSampler = value;
        else if (key == kLightBVHOptions) mLightBVHOptions = value;
        else if (key == kUseRTXDI) mStaticParams.useRTXDI = value;
        else if (key == kRTXDIOptions) mRTXDIOptions = value;

        // Material parameters
        else if (key == kUseAlphaTest) mStaticParams.useAlphaTest = value;
        else if (key == kAdjustShadingNormals) mStaticParams.adjustShadingNormals = value;
        else if (key == kMaxNestedMaterials) mStaticParams.maxNestedMaterials = value;
        else if (key == kUseLightsInDielectricVolumes) mStaticParams.useLightsInDielectricVolumes = value;
        else if (key == kDisableCaustics) mStaticParams.disableCaustics = value;
        else if (key == kSpecularRoughnessThreshold) mParams.specularRoughnessThreshold = value;
        else if (key == kPrimaryLodMode) mStaticParams.primaryLodMode = value;
        else if (key == kLODBias) mParams.lodBias = value;

        // Denoising parameters
        else if (key == kUseNRDDemodulation) mStaticParams.useNRDDemodulation = value;

        // Output parameters
        else if (key == kFixedOutputSize) mFixedOutputSize = value;
        else if (key == kColorFormat) mStaticParams.colorFormat = value;

        else LLOG_WRN << "Unknown field '" << key << "' in PathTracer dictionary.";
    }

    if (dict.keyExists(kMaxSurfaceBounces)) {
        // Initialize bounce counts to 'maxSurfaceBounces' if they weren't explicitly set.
        if (!dict.keyExists(kMaxDiffuseBounces)) mStaticParams.maxDiffuseBounces = mStaticParams.maxSurfaceBounces;
        if (!dict.keyExists(kMaxSpecularBounces)) mStaticParams.maxSpecularBounces = mStaticParams.maxSurfaceBounces;
        if (!dict.keyExists(kMaxTransmissionBounces)) mStaticParams.maxTransmissionBounces = mStaticParams.maxSurfaceBounces;
    } else {
        // Initialize surface bounces.
        mStaticParams.maxSurfaceBounces = std::max(mStaticParams.maxDiffuseBounces, std::max(mStaticParams.maxSpecularBounces, mStaticParams.maxTransmissionBounces));
    }

    bool maxSurfaceBouncesNeedsAdjustment =
        mStaticParams.maxSurfaceBounces < mStaticParams.maxDiffuseBounces ||
        mStaticParams.maxSurfaceBounces < mStaticParams.maxSpecularBounces ||
        mStaticParams.maxSurfaceBounces < mStaticParams.maxTransmissionBounces;

    // Show a warning if maxSurfaceBounces will be adjusted in validateOptions().
    if (dict.keyExists(kMaxSurfaceBounces) && maxSurfaceBouncesNeedsAdjustment) {
        LLOG_WRN << kMaxSurfaceBounces << " is set lower than '" << kMaxDiffuseBounces << "', '" << kMaxSpecularBounces << "' or '" << kMaxTransmissionBounces << "' and will be increased.";
    }
}

void PathTracer::validateOptions() {
    if (mParams.specularRoughnessThreshold < 0.f || mParams.specularRoughnessThreshold > 1.f) {
        LLOG_WRN << "'specularRoughnessThreshold' has invalid value. Clamping to range [0,1].";
        mParams.specularRoughnessThreshold = clamp(mParams.specularRoughnessThreshold, 0.f, 1.f);
    }

    // Static parameters.
    if (mStaticParams.samplesPerPixel < 1 || mStaticParams.samplesPerPixel > kMaxSamplesPerPixel) {
        LLOG_WRN << "'samplesPerPixel' must be in the range [1, " << std::to_string(kMaxSamplesPerPixel) << "]. Clamping to this range.";
        mStaticParams.samplesPerPixel = std::clamp(mStaticParams.samplesPerPixel, 1u, kMaxSamplesPerPixel);
    }

    auto clampBounces = [] (uint32_t& bounces, const std::string& name) {
        if (bounces > kMaxBounces) {
            LLOG_WRN << name << " exceeds the maximum supported bounces. Clamping to " << std::to_string(kMaxBounces);
            bounces = kMaxBounces;
        }
    };

    clampBounces(mStaticParams.maxSurfaceBounces, kMaxSurfaceBounces);
    clampBounces(mStaticParams.maxDiffuseBounces, kMaxDiffuseBounces);
    clampBounces(mStaticParams.maxSpecularBounces, kMaxSpecularBounces);
    clampBounces(mStaticParams.maxTransmissionBounces, kMaxTransmissionBounces);

    // Make sure maxSurfaceBounces is at least as many as any of diffuse, specular or transmission.
    uint32_t minSurfaceBounces = std::max(mStaticParams.maxDiffuseBounces, std::max(mStaticParams.maxSpecularBounces, mStaticParams.maxTransmissionBounces));
    mStaticParams.maxSurfaceBounces = std::max(mStaticParams.maxSurfaceBounces, minSurfaceBounces);

    if (mStaticParams.primaryLodMode == TexLODMode::RayCones) {
        LLOG_WRN << "Unsupported tex lod mode. Defaulting to Mip0.";
        mStaticParams.primaryLodMode = TexLODMode::Mip0;
    }
}

Dictionary PathTracer::getScriptingDictionary() {
    if (auto lightBVHSampler = std::dynamic_pointer_cast<LightBVHSampler>(mpEmissiveSampler)) {
        mLightBVHOptions = lightBVHSampler->getOptions();
    }

    Dictionary d;

    // Rendering parameters
    d[kSamplesPerPixel] = mStaticParams.samplesPerPixel;
    d[kMaxSurfaceBounces] = mStaticParams.maxSurfaceBounces;
    d[kMaxDiffuseBounces] = mStaticParams.maxDiffuseBounces;
    d[kMaxSpecularBounces] = mStaticParams.maxSpecularBounces;
    d[kMaxTransmissionBounces] = mStaticParams.maxTransmissionBounces;

    // Sampling parameters
    d[kSampleGenerator] = mStaticParams.sampleGenerator;
    if (mParams.useFixedSeed) d[kFixedSeed] = mParams.fixedSeed;
    d[kUseBSDFSampling] = mStaticParams.useBSDFSampling;
    d[kUseRussianRoulette] = mStaticParams.useRussianRoulette;
    d[kUseNEE] = mStaticParams.useNEE;
    d[kUseMIS] = mStaticParams.useMIS;
    d[kMISHeuristic] = mStaticParams.misHeuristic;
    d[kMISPowerExponent] = mStaticParams.misPowerExponent;
    d[kEmissiveSampler] = mStaticParams.emissiveSampler;
    if (mStaticParams.emissiveSampler == EmissiveLightSamplerType::LightBVH) d[kLightBVHOptions] = mLightBVHOptions;
    d[kUseRTXDI] = mStaticParams.useRTXDI;
    d[kRTXDIOptions] = mRTXDIOptions;

    // Material parameters
    d[kUseAlphaTest] = mStaticParams.useAlphaTest;
    d[kAdjustShadingNormals] = mStaticParams.adjustShadingNormals;
    d[kMaxNestedMaterials] = mStaticParams.maxNestedMaterials;
    d[kUseLightsInDielectricVolumes] = mStaticParams.useLightsInDielectricVolumes;
    d[kDisableCaustics] = mStaticParams.disableCaustics;
    d[kSpecularRoughnessThreshold] = mParams.specularRoughnessThreshold;
    d[kPrimaryLodMode] = mStaticParams.primaryLodMode;
    d[kLODBias] = mParams.lodBias;

    // Denoising parameters
    d[kUseNRDDemodulation] = mStaticParams.useNRDDemodulation;

    // Output parameters
    d[kColorFormat] = mStaticParams.colorFormat;

    return d;
}

RenderPassReflection PathTracer::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    
    addRenderPassInputs(reflector, kInputChannels);
    addRenderPassOutputs(reflector, kOutputChannels, ResourceBindFlags::UnorderedAccess);
    return reflector;
}

void PathTracer::setFrameDim(const uint2 frameDim) {
    auto prevFrameDim = mParams.frameDim;
    auto prevScreenTiles = mParams.screenTiles;

    mParams.frameDim = frameDim;
    if (mParams.frameDim.x > kMaxFrameDimension || mParams.frameDim.y > kMaxFrameDimension) {
        throw std::runtime_error("Frame dimensions up to " + std::to_string(kMaxFrameDimension) + " pixels width/height are supported.");
    }

    // Tile dimensions have to be powers-of-two.
    assert(isPowerOf2(kScreenTileDim.x) && isPowerOf2(kScreenTileDim.y));
    assert(kScreenTileDim.x == (1 << kScreenTileBits.x) && kScreenTileDim.y == (1 << kScreenTileBits.y));
    mParams.screenTiles = div_round_up(mParams.frameDim, kScreenTileDim);

    if (mParams.frameDim != prevFrameDim || mParams.screenTiles != prevScreenTiles) {
        mVarsChanged = true;
    }
}

void PathTracer::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    mpScene = pScene;
    mParams.frameCount = 0;
    mParams.frameDim = {};
    mParams.screenTiles = {};

    // Need to recreate the RTXDI module when the scene changes.
    mpRTXDI = nullptr;

    // Need to recreate the trace passes because the shader binding table changes.
    mpTracePass = nullptr;
    mpTraceDeltaReflectionPass = nullptr;
    mpTraceDeltaTransmissionPass = nullptr;

    resetLighting();

    if (mpScene) {
        if (pScene->hasGeometryType(Scene::GeometryType::Custom)) {
            LLOG_WRN << "PathTracer: This render pass does not support custom primitives.";
        }

        validateOptions();

        mRecompile = true;
    }
}

void PathTracer::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    if (!beginFrame(pRenderContext, renderData)) return;

    // Update shader program specialization.
    updatePrograms();

    // Prepare resources.
    prepareResources(pRenderContext, renderData);

    // Prepare the path tracer parameter block.
    // This should be called after all resources have been created.
    preparePathTracer(renderData);

    // Generate paths at primary hits.
    generatePaths(pRenderContext, renderData);

    // Update RTXDI.
    if (mpRTXDI) {
        const auto& pMotionVectors = renderData[kInputMotionVectors]->asTexture();
        mpRTXDI->update(pRenderContext, pMotionVectors);
    }

    // Trace pass.
    assert(mpTracePass);
    tracePass(pRenderContext, renderData, *mpTracePass);

    // Launch separate passes to trace delta reflection and transmission paths to generate respective guide buffers.
    if (mOutputNRDAdditionalData) {
        assert(mpTraceDeltaReflectionPass && mpTraceDeltaTransmissionPass);
        tracePass(pRenderContext, renderData, *mpTraceDeltaReflectionPass);
        tracePass(pRenderContext, renderData, *mpTraceDeltaTransmissionPass);
    }

    // Resolve pass.
    resolvePass(pRenderContext, renderData);

    endFrame(pRenderContext, renderData);
}

PathTracer::TracePass::TracePass(Device::SharedPtr pDevice, const std::string& name, const std::string& passDefine, const Scene::SharedPtr& pScene, const Program::DefineList& defines, const Program::TypeConformanceList& globalTypeConformances)
    : mpDevice(pDevice)
    , name(name)
    , passDefine(passDefine)
{
    const uint32_t kRayTypeScatter = 0;
    const uint32_t kMissScatter = 0;

    RtProgram::Desc desc;
    desc.addShaderLibrary(kTracePassFilename);
    desc.setShaderModel(kShaderModel);
    desc.setMaxPayloadSize(160); // This is conservative but the required minimum is 140 bytes.
    desc.setMaxAttributeSize(pScene->getRaytracingMaxAttributeSize());
    desc.setMaxTraceRecursionDepth(1);
    if (!pScene->hasProceduralGeometry()) desc.setPipelineFlags(RtPipelineFlags::SkipProceduralPrimitives);

    // Create ray tracing binding table.
    pBindingTable = RtBindingTable::create(1, 1, pScene->getGeometryCount());

    // Specify entry point for raygen and miss shaders.
    // The raygen shader needs type conformances for *all* materials in the scene.
    // The miss shader doesn't need type conformances as it doesn't access any materials.
    pBindingTable->setRayGen(desc.addRayGen("rayGen", globalTypeConformances));
    pBindingTable->setMiss(kMissScatter, desc.addMiss("scatterMiss"));

    // Specify hit group entry points for every combination of geometry and material type.
    // The code for each hit group gets specialized for the actual types it's operating on.
    // First query which material types the scene has.
    auto materialTypes = pScene->getMaterialSystem()->getMaterialTypes();

    for (const auto materialType : materialTypes) {
        auto typeConformances = pScene->getMaterialSystem()->getTypeConformances(materialType);

        // Add hit groups for triangles.
        if (auto geometryIDs = pScene->getGeometryIDs(Scene::GeometryType::TriangleMesh, materialType); !geometryIDs.empty()) {
            auto shaderID = desc.addHitGroup("scatterTriangleClosestHit", "scatterTriangleAnyHit", "", typeConformances, to_string(materialType));
            pBindingTable->setHitGroup(kRayTypeScatter, geometryIDs, shaderID);
        }

        // Add hit groups for displaced triangle meshes.
        if (auto geometryIDs = pScene->getGeometryIDs(Scene::GeometryType::DisplacedTriangleMesh, materialType); !geometryIDs.empty()) {
            auto shaderID = desc.addHitGroup("scatterDisplacedTriangleMeshClosestHit", "", "displacedTriangleMeshIntersection", typeConformances, to_string(materialType));
            pBindingTable->setHitGroup(kRayTypeScatter, geometryIDs, shaderID);
        }

        // Add hit groups for curves.
        if (auto geometryIDs = pScene->getGeometryIDs(Scene::GeometryType::Curve, materialType); !geometryIDs.empty()) {
            auto shaderID = desc.addHitGroup("scatterCurveClosestHit", "", "curveIntersection", typeConformances, to_string(materialType));
            pBindingTable->setHitGroup(kRayTypeScatter, geometryIDs, shaderID);
        }

        // Add hit groups for SDF grids.
        if (auto geometryIDs = pScene->getGeometryIDs(Scene::GeometryType::SDFGrid, materialType); !geometryIDs.empty()) {
            auto shaderID = desc.addHitGroup("scatterSdfGridClosestHit", "", "sdfGridIntersection", typeConformances, to_string(materialType));
            pBindingTable->setHitGroup(kRayTypeScatter, geometryIDs, shaderID);
        }
    }

    pProgram = RtProgram::create(mpDevice, desc, defines);
}

void PathTracer::TracePass::prepareProgram(const Program::DefineList& defines) {
    assert(pProgram != nullptr && pBindingTable != nullptr);
    pProgram->setDefines(defines);
    if (!passDefine.empty()) pProgram->addDefine(passDefine);
    pVars = RtProgramVars::create(mpDevice, pProgram, pBindingTable);
}

void PathTracer::updatePrograms() {
    assert(mpScene);

    if (mRecompile == false) return;

    auto defines = mStaticParams.getDefines(*this);
    auto globalTypeConformances = mpScene->getMaterialSystem()->getTypeConformances();

    // Create trace passes lazily.
    if (!mpTracePass) mpTracePass = std::make_unique<TracePass>(mpDevice, "tracePass", "", mpScene, defines, globalTypeConformances);
    if (mOutputNRDAdditionalData) {
        if (!mpTraceDeltaReflectionPass) mpTraceDeltaReflectionPass = std::make_unique<TracePass>(mpDevice, "traceDeltaReflectionPass", "DELTA_REFLECTION_PASS", mpScene, defines, globalTypeConformances);
        if (!mpTraceDeltaTransmissionPass) mpTraceDeltaTransmissionPass = std::make_unique<TracePass>(mpDevice, "traceDeltaTransmissionPass", "DELTA_TRANSMISSION_PASS", mpScene, defines, globalTypeConformances);
    }

    // Create program vars for trace programs.
    // We only need to set defines for program specialization here. Type conformances have already been setup on construction.
    mpTracePass->prepareProgram(defines);
    if (mpTraceDeltaReflectionPass) mpTraceDeltaReflectionPass->prepareProgram(defines);
    if (mpTraceDeltaTransmissionPass) mpTraceDeltaTransmissionPass->prepareProgram(defines);

    // Perform program specialization.
    // Note that we must use set instead of add functions to replace any stale state.
    auto prepareProgram = [&](Program::SharedPtr program) {
        program->setDefines(defines);
        program->setTypeConformances(globalTypeConformances);
    };
    prepareProgram(mpGeneratePaths->getProgram());
    prepareProgram(mpResolvePass->getProgram());
    prepareProgram(mpReflectTypes->getProgram());

    // Create program vars for the specialized programs.
    mpGeneratePaths->setVars(nullptr);
    mpResolvePass->setVars(nullptr);
    mpReflectTypes->setVars(nullptr);

    mVarsChanged = true;
    mRecompile = false;
}

void PathTracer::prepareResources(RenderContext* pRenderContext, const RenderData& renderData) {
    // Compute allocation requirements for paths and output samples.
    // Note that the sample buffers are padded to whole tiles, while the max path count depends on actual frame dimension.
    // If we don't have a fixed sample count, assume the worst case.
    uint32_t spp = mFixedSampleCount ? mStaticParams.samplesPerPixel : kMaxSamplesPerPixel;
    uint32_t tileCount = mParams.screenTiles.x * mParams.screenTiles.y;
    const uint32_t sampleCount = tileCount * kScreenTileDim.x * kScreenTileDim.y * spp;
    const uint32_t screenPixelCount = mParams.frameDim.x * mParams.frameDim.y;
    const uint32_t pathCount = screenPixelCount * spp;

    // Allocate output sample offset buffer if needed.
    // This buffer stores the output offset to where the samples for each pixel are stored consecutively.
    // The offsets are local to the current tile, so 16-bit format is sufficient and reduces bandwidth usage.
    if (!mFixedSampleCount) {
        if (!mpSampleOffset || mpSampleOffset->getWidth() != mParams.frameDim.x || mpSampleOffset->getHeight() != mParams.frameDim.y) {
            assert(kScreenTileDim.x * kScreenTileDim.y * kMaxSamplesPerPixel <= (1u << 16));
            mpSampleOffset = Texture::create2D(mpDevice, mParams.frameDim.x, mParams.frameDim.y, ResourceFormat::R16Uint, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
            mVarsChanged = true;
        }
    }

    auto var = mpReflectTypes->getRootVar();

    // Allocate per-sample buffers.
    // For the special case of fixed 1 spp, the output is written out directly and this buffer is not needed.
    if (!mFixedSampleCount || mStaticParams.samplesPerPixel > 1) {
        if (!mpSampleColor || mpSampleColor->getElementCount() < sampleCount || mVarsChanged) {
            mpSampleColor = Buffer::createStructured(mpDevice, var["sampleColor"], sampleCount, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
            mVarsChanged = true;
        }
    }

    if (mOutputGuideData && (!mpSampleGuideData || mpSampleGuideData->getElementCount() < sampleCount || mVarsChanged)) {
        mpSampleGuideData = Buffer::createStructured(mpDevice, var["sampleGuideData"], sampleCount, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
        mVarsChanged = true;
    }

    if (mOutputNRDData && (!mpSampleNRDRadiance || mpSampleNRDRadiance->getElementCount() < sampleCount || mVarsChanged)) {
        mpSampleNRDRadiance = Buffer::createStructured(mpDevice, var["sampleNRDRadiance"], sampleCount, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
        mpSampleNRDHitDist = Buffer::createStructured(mpDevice, var["sampleNRDHitDist"], sampleCount, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
        mpSampleNRDEmission = Buffer::createStructured(mpDevice, var["sampleNRDEmission"], sampleCount, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
        mpSampleNRDReflectance = Buffer::createStructured(mpDevice, var["sampleNRDReflectance"], sampleCount, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
        mVarsChanged = true;
    }
}

void PathTracer::preparePathTracer(const RenderData& renderData) {
    // Create path tracer parameter block if needed.
    if (!mpPathTracerBlock || mVarsChanged) {
        auto reflector = mpReflectTypes->getProgram()->getReflector()->getParameterBlock("pathTracer");
        mpPathTracerBlock = ParameterBlock::create(mpDevice, reflector);
        assert(mpPathTracerBlock);
        mVarsChanged = true;
    }

    // Bind resources.
    auto var = mpPathTracerBlock->getRootVar();
    setShaderData(var, renderData);
}

void PathTracer::resetLighting() {
    // Retain the options for the emissive sampler.
    if (auto lightBVHSampler = std::dynamic_pointer_cast<LightBVHSampler>(mpEmissiveSampler)) {
        mLightBVHOptions = lightBVHSampler->getOptions();
    }

    mpEmissiveSampler = nullptr;
    mpEnvMapSampler = nullptr;
    mRecompile = true;
}

void PathTracer::prepareMaterials(RenderContext* pRenderContext) {
    // This functions checks for material changes and performs any necessary update.
    // For now all we need to do is to trigger a recompile so that the right defines get set.
    // In the future, we might want to do additional material-specific setup here.

    if (is_set(mpScene->getUpdates(), Scene::UpdateFlags::MaterialsChanged)) {
        mRecompile = true;
    }
}

bool PathTracer::prepareLighting(RenderContext* pRenderContext) {
    bool lightingChanged = false;

    if (is_set(mpScene->getUpdates(), Scene::UpdateFlags::RenderSettingsChanged)) {
        lightingChanged = true;
        mRecompile = true;
    }

    if (is_set(mpScene->getUpdates(), Scene::UpdateFlags::SDFGridConfigChanged)) {
        mRecompile = true;
    }

    if (is_set(mpScene->getUpdates(), Scene::UpdateFlags::EnvMapChanged)) {
        mpEnvMapSampler = nullptr;
        lightingChanged = true;
        mRecompile = true;
    }

    if (mpScene->useEnvLight()) {
        if (!mpEnvMapSampler) {
            mpEnvMapSampler = EnvMapSampler::create(pRenderContext, mpScene->getEnvMap());
            lightingChanged = true;
            mRecompile = true;
        }
    } else {
        if (mpEnvMapSampler) {
            mpEnvMapSampler = nullptr;
            lightingChanged = true;
            mRecompile = true;
        }
    }

    // Request the light collection if emissive lights are enabled.
    if (mpScene->getRenderSettings().useEmissiveLights) {
        mpScene->getLightCollection(pRenderContext);
    }

    if (mpScene->useEmissiveLights()) {
        if (!mpEmissiveSampler) {
            const auto& pLights = mpScene->getLightCollection(pRenderContext);
            assert(pLights && pLights->getActiveLightCount() > 0);
            assert(!mpEmissiveSampler);

            switch (mStaticParams.emissiveSampler) {
                case EmissiveLightSamplerType::Uniform:
                    mpEmissiveSampler = EmissiveUniformSampler::create(pRenderContext, mpScene);
                    break;
                case EmissiveLightSamplerType::LightBVH:
                    mpEmissiveSampler = LightBVHSampler::create(pRenderContext, mpScene, mLightBVHOptions);
                    break;
                case EmissiveLightSamplerType::Power:
                    mpEmissiveSampler = EmissivePowerSampler::create(pRenderContext, mpScene);
                    break;
                default:
                    throw std::runtime_error("Unknown emissive light sampler type");
            }
            lightingChanged = true;
            mRecompile = true;
        }
    } else {
        if (mpEmissiveSampler) {
            // Retain the options for the emissive sampler.
            if (auto lightBVHSampler = std::dynamic_pointer_cast<LightBVHSampler>(mpEmissiveSampler)) {
                mLightBVHOptions = lightBVHSampler->getOptions();
            }

            mpEmissiveSampler = nullptr;
            lightingChanged = true;
            mRecompile = true;
        }
    }

    if (mpEmissiveSampler) {
        lightingChanged |= mpEmissiveSampler->update(pRenderContext);
        auto defines = mpEmissiveSampler->getDefines();
        if (mpTracePass && mpTracePass->pProgram->addDefines(defines)) mRecompile = true;
    }

    return lightingChanged;
}

void PathTracer::prepareRTXDI(RenderContext* pRenderContext) {
    if (mStaticParams.useRTXDI) {
        if (!mpRTXDI) mpRTXDI = RTXDI::create(mpScene, mRTXDIOptions);

        // Emit warning if enabled while using spp != 1.
        if (!mFixedSampleCount || mStaticParams.samplesPerPixel != 1) {
            LLOG_WRN << "Using RTXDI with samples/pixel != 1 will only generate one RTXDI sample reused for all pixel samples.";
        }
    } else {
        mpRTXDI = nullptr;
    }
}

void PathTracer::setNRDData(const ShaderVar& var, const RenderData& renderData) const {
    var["sampleRadiance"] = mpSampleNRDRadiance;
    var["sampleHitDist"] = mpSampleNRDHitDist;
    var["sampleEmission"] = mpSampleNRDEmission;
    var["sampleReflectance"] = mpSampleNRDReflectance;
    var["primaryHitEmission"] = renderData[kOutputNRDEmission]->asTexture();
    var["primaryHitDiffuseReflectance"] = renderData[kOutputNRDDiffuseReflectance]->asTexture();
    var["primaryHitSpecularReflectance"] = renderData[kOutputNRDSpecularReflectance]->asTexture();
    var["deltaReflectionReflectance"] = renderData[kOutputNRDDeltaReflectionReflectance]->asTexture();
    var["deltaReflectionEmission"] = renderData[kOutputNRDDeltaReflectionEmission]->asTexture();
    var["deltaReflectionNormWRoughMaterialID"] = renderData[kOutputNRDDeltaReflectionNormWRoughMaterialID]->asTexture();
    var["deltaReflectionPathLength"] = renderData[kOutputNRDDeltaReflectionPathLength]->asTexture();
    var["deltaReflectionHitDist"] = renderData[kOutputNRDDeltaReflectionHitDist]->asTexture();
    var["deltaTransmissionReflectance"] = renderData[kOutputNRDDeltaTransmissionReflectance]->asTexture();
    var["deltaTransmissionEmission"] = renderData[kOutputNRDDeltaTransmissionEmission]->asTexture();
    var["deltaTransmissionNormWRoughMaterialID"] = renderData[kOutputNRDDeltaTransmissionNormWRoughMaterialID]->asTexture();
    var["deltaTransmissionPathLength"] = renderData[kOutputNRDDeltaTransmissionPathLength]->asTexture();
    var["deltaTransmissionPosW"] = renderData[kOutputNRDDeltaTransmissionPosW]->asTexture();
}

void PathTracer::setShaderData(const ShaderVar& var, const RenderData& renderData, bool useLightSampling) const {
    // Bind static resources that don't change per frame.
    if (mVarsChanged) {
        if (useLightSampling && mpEnvMapSampler) mpEnvMapSampler->setShaderData(var["envMapSampler"]);

        var["sampleOffset"] = mpSampleOffset; // Can be nullptr
        var["sampleColor"] = mpSampleColor;
        var["sampleGuideData"] = mpSampleGuideData;
    }

    // Bind runtime data.
    setNRDData(var["outputNRD"], renderData);

    Texture::SharedPtr pViewDir;
    if (mpScene->getCamera()->getApertureRadius() > 0.f) {
        pViewDir = renderData[kInputViewDir]->asTexture();
        if (!pViewDir) {
            LLOG_WRN << "Depth-of-field requires the '" << kInputViewDir << "' input. Expect incorrect rendering.";
        }
    }

    Texture::SharedPtr pSampleCount;
    if (!mFixedSampleCount) {
        pSampleCount = renderData[kInputSampleCount]->asTexture();
        if (!pSampleCount) throw std::runtime_error("PathTracer: Missing sample count input texture");
    }

    var["params"].setBlob(mParams);
    var["vbuffer"] = renderData[kInputVBuffer]->asTexture();
    var["viewDir"] = pViewDir; // Can be nullptr
    var["sampleCount"] = pSampleCount; // Can be nullptr
    var["outputColor"] = renderData[kOutputColor]->asTexture();

    if (useLightSampling && mpEmissiveSampler) {
        // TODO: Do we have to bind this every frame?
        mpEmissiveSampler->setShaderData(var["emissiveSampler"]);
    }
}

bool PathTracer::beginFrame(RenderContext* pRenderContext, const RenderData& renderData) {
    const auto& pOutputColor = renderData[kOutputColor]->asTexture();
    assert(pOutputColor);

    // Set output frame dimension.
    setFrameDim(uint2(pOutputColor->getWidth(), pOutputColor->getHeight()));

    // Validate all I/O sizes match the expected size.
    // If not, we'll disable the path tracer to give the user a chance to fix the configuration before re-enabling it.
    bool resolutionMismatch = false;
    auto validateChannels = [&](const auto& channels) {
        for (const auto& channel : channels) {
            auto pTexture = renderData[channel.name]->asTexture();
            if (pTexture && (pTexture->getWidth() != mParams.frameDim.x || pTexture->getHeight() != mParams.frameDim.y)) resolutionMismatch = true;
        }
    };
    validateChannels(kInputChannels);
    validateChannels(kOutputChannels);

    if (mEnabled && resolutionMismatch) {
        LLOG_ERR << "PathTracer I/O sizes don't match. The pass will be disabled.";
        mEnabled = false;
    }

    if (mpScene == nullptr || !mEnabled) {
        pRenderContext->clearUAV(pOutputColor->getUAV().get(), float4(0.f));

        // Set refresh flag if changes that affect the output have occured.
        // This is needed to ensure other passes get notified when the path tracer is enabled/disabled.
        if (mOptionsChanged) {
            auto& dict = renderData.getDictionary();
            auto flags = dict.getValue(kRenderPassRefreshFlags, Falcor::RenderPassRefreshFlags::None);
            if (mOptionsChanged) flags |= Falcor::RenderPassRefreshFlags::RenderOptionsChanged;
            dict[Falcor::kRenderPassRefreshFlags] = flags;
        }

        return false;
    }

    // Update materials.
    prepareMaterials(pRenderContext);

    // Update the env map and emissive sampler to the current frame.
    bool lightingChanged = prepareLighting(pRenderContext);

    // Prepare RTXDI.
    prepareRTXDI(pRenderContext);
    if (mpRTXDI) mpRTXDI->beginFrame(pRenderContext, mParams.frameDim);

    // Update refresh flag if changes that affect the output have occured.
    auto& dict = renderData.getDictionary();
    if (mOptionsChanged || lightingChanged) {
        auto flags = dict.getValue(kRenderPassRefreshFlags, Falcor::RenderPassRefreshFlags::None);
        if (mOptionsChanged) flags |= Falcor::RenderPassRefreshFlags::RenderOptionsChanged;
        if (lightingChanged) flags |= Falcor::RenderPassRefreshFlags::LightingChanged;
        dict[Falcor::kRenderPassRefreshFlags] = flags;
        mOptionsChanged = false;
    }

    // Check if GBuffer has adjusted shading normals enabled.
    bool gbufferAdjustShadingNormals = dict.getValue(Falcor::kRenderPassGBufferAdjustShadingNormals, false);
    if (gbufferAdjustShadingNormals != mGBufferAdjustShadingNormals) {
        mGBufferAdjustShadingNormals = gbufferAdjustShadingNormals;
        mRecompile = true;
    }

    // Check if fixed sample count should be used. When the sample count input is connected we load the count from there instead.
    mFixedSampleCount = renderData[kInputSampleCount] == nullptr;

    // Check if guide data should be generated.
    mOutputGuideData = renderData[kOutputAlbedo] != nullptr || renderData[kOutputSpecularAlbedo] != nullptr
        || renderData[kOutputIndirectAlbedo] != nullptr || renderData[kOutputNormal] != nullptr
        || renderData[kOutputReflectionPosW] != nullptr;

    // Check if NRD data should be generated.
    mOutputNRDData =
        renderData[kOutputNRDDiffuseRadianceHitDist] != nullptr
        || renderData[kOutputNRDSpecularRadianceHitDist] != nullptr
        || renderData[kOutputNRDResidualRadianceHitDist] != nullptr
        || renderData[kOutputNRDEmission] != nullptr
        || renderData[kOutputNRDDiffuseReflectance] != nullptr
        || renderData[kOutputNRDSpecularReflectance] != nullptr;

    // Check if additional NRD data should be generated.
    bool prevOutputNRDAdditionalData = mOutputNRDAdditionalData;
    mOutputNRDAdditionalData =
        renderData[kOutputNRDDeltaReflectionRadianceHitDist] != nullptr
        || renderData[kOutputNRDDeltaTransmissionRadianceHitDist] != nullptr
        || renderData[kOutputNRDDeltaReflectionReflectance] != nullptr
        || renderData[kOutputNRDDeltaReflectionEmission] != nullptr
        || renderData[kOutputNRDDeltaReflectionNormWRoughMaterialID] != nullptr
        || renderData[kOutputNRDDeltaReflectionPathLength] != nullptr
        || renderData[kOutputNRDDeltaReflectionHitDist] != nullptr
        || renderData[kOutputNRDDeltaTransmissionReflectance] != nullptr
        || renderData[kOutputNRDDeltaTransmissionEmission] != nullptr
        || renderData[kOutputNRDDeltaTransmissionNormWRoughMaterialID] != nullptr
        || renderData[kOutputNRDDeltaTransmissionPathLength] != nullptr
        || renderData[kOutputNRDDeltaTransmissionPosW] != nullptr;
    if (mOutputNRDAdditionalData != prevOutputNRDAdditionalData) mRecompile = true;

    // Enable pixel stats if rayCount or pathLength outputs are connected.
#if USE_PIXELSTATS
    if (renderData[kOutputRayCount] != nullptr || renderData[kOutputPathLength] != nullptr) {
        mpPixelStats->setEnabled(true);
    }

    mpPixelStats->beginFrame(pRenderContext, mParams.frameDim);
#endif

    mpPixelDebug->beginFrame(pRenderContext, mParams.frameDim);

    // Update the random seed.
    mParams.seed = mParams.useFixedSeed ? mParams.fixedSeed : mParams.frameCount;

    return true;
}

void PathTracer::endFrame(RenderContext* pRenderContext, const RenderData& renderData) {
#if USE_PIXELSTATS
    mpPixelStats->endFrame(pRenderContext);
#endif

    mpPixelDebug->endFrame(pRenderContext);

    auto copyTexture = [pRenderContext](Texture* pDst, const Texture* pSrc) {
        if (pDst && pSrc) {
            assert(pDst && pSrc);
            assert(pDst->getFormat() == pSrc->getFormat());
            assert(pDst->getWidth() == pSrc->getWidth() && pDst->getHeight() == pSrc->getHeight());
            pRenderContext->copyResource(pDst, pSrc);
        } else if (pDst) {
            pRenderContext->clearUAV(pDst->getUAV().get(), uint4(0, 0, 0, 0));
        }
    };

    // Copy pixel stats to outputs if available.
#if USE_PIXELSTATS
    copyTexture(renderData[kOutputRayCount]->asTexture().get(), mpPixelStats->getRayCountTexture(pRenderContext).get());
    copyTexture(renderData[kOutputPathLength]->asTexture().get(), mpPixelStats->getPathLengthTexture().get());
#endif

    if (mpRTXDI) mpRTXDI->endFrame(pRenderContext);

    mVarsChanged = false;
    mParams.frameCount++;
}

void PathTracer::generatePaths(RenderContext* pRenderContext, const RenderData& renderData)
{
    PROFILE(mpDevice, "generatePaths");

    // Check shader assumptions.
    // We launch one thread group per screen tile, with threads linearly indexed.
    const uint32_t tileSize = kScreenTileDim.x * kScreenTileDim.y;
    assert(kScreenTileDim.x == 16 && kScreenTileDim.y == 16); // TODO: Remove this temporary limitation when Slang bug has been fixed, see comments in shader.
    assert(kScreenTileBits.x <= 4 && kScreenTileBits.y <= 4); // Since we use 8-bit deinterleave.
    assert(mpGeneratePaths->getThreadGroupSize().x == tileSize);
    assert(mpGeneratePaths->getThreadGroupSize().y == 1 && mpGeneratePaths->getThreadGroupSize().z == 1);

    // Additional specialization. This shouldn't change resource declarations.
    mpGeneratePaths->addDefine("USE_VIEW_DIR", (mpScene->getCamera()->getApertureRadius() > 0 && renderData[kInputViewDir] != nullptr) ? "1" : "0");
    mpGeneratePaths->addDefine("OUTPUT_GUIDE_DATA", mOutputGuideData ? "1" : "0");
    mpGeneratePaths->addDefine("OUTPUT_NRD_DATA", mOutputNRDData ? "1" : "0");
    mpGeneratePaths->addDefine("OUTPUT_NRD_ADDITIONAL_DATA", mOutputNRDAdditionalData ? "1" : "0");

    // Bind resources.
    auto var = mpGeneratePaths->getRootVar()["CB"]["gPathGenerator"];
    setShaderData(var, renderData, false);

    mpGeneratePaths["gScene"] = mpScene->getParameterBlock();

    if (mpRTXDI) mpRTXDI->setShaderData(mpGeneratePaths->getRootVar());

    // Launch one thread per pixel.
    // The dimensions are padded to whole tiles to allow re-indexing the threads in the shader.
    mpGeneratePaths->execute(pRenderContext, { mParams.screenTiles.x * tileSize, mParams.screenTiles.y, 1u });
}

void PathTracer::tracePass(RenderContext* pRenderContext, const RenderData& renderData, TracePass& tracePass)
{
    PROFILE(mpDevice, tracePass.name);

    assert(tracePass.pProgram != nullptr && tracePass.pBindingTable != nullptr && tracePass.pVars != nullptr);

    // Additional specialization. This shouldn't change resource declarations.
    tracePass.pProgram->addDefine("USE_VIEW_DIR", (mpScene->getCamera()->getApertureRadius() > 0 && renderData[kInputViewDir] != nullptr) ? "1" : "0");
    tracePass.pProgram->addDefine("OUTPUT_GUIDE_DATA", mOutputGuideData ? "1" : "0");
    tracePass.pProgram->addDefine("OUTPUT_NRD_DATA", mOutputNRDData ? "1" : "0");
    tracePass.pProgram->addDefine("OUTPUT_NRD_ADDITIONAL_DATA", mOutputNRDAdditionalData ? "1" : "0");

    // Bind global resources.
    auto var = tracePass.pVars->getRootVar();
    mpScene->setRaytracingShaderData(pRenderContext, var);

    if (mVarsChanged) mpSampleGenerator->setShaderData(var);
    if (mpRTXDI) mpRTXDI->setShaderData(var);

#if USE_PIXELSTATS
    mpPixelStats->prepareProgram(tracePass.pProgram, var);
#endif

    mpPixelDebug->prepareProgram(tracePass.pProgram, var);

    // Bind the path tracer.
    var["gPathTracer"] = mpPathTracerBlock;

    // Full screen dispatch.
    mpScene->raytrace(pRenderContext, tracePass.pProgram.get(), tracePass.pVars, uint3(mParams.frameDim, 1));
}

void PathTracer::resolvePass(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mOutputGuideData && !mOutputNRDData && mFixedSampleCount && mStaticParams.samplesPerPixel == 1) return;

    PROFILE(mpDevice, "resolvePass");

    // This pass is executed when multiple samples per pixel are used.
    // We launch one thread per pixel that computes the resolved color by iterating over the samples.
    // The samples are arranged in tiles with pixels in Morton order, with samples stored consecutively for each pixel.
    // With adaptive sampling, an extra sample offset lookup table computed by the path generation pass is used to
    // locate the samples for each pixel.

    // Additional specialization. This shouldn't change resource declarations.
    mpResolvePass->addDefine("OUTPUT_GUIDE_DATA", mOutputGuideData ? "1" : "0");
    mpResolvePass->addDefine("OUTPUT_NRD_DATA", mOutputNRDData ? "1" : "0");

    // Bind resources.
    auto var = mpResolvePass->getRootVar()["CB"]["gResolvePass"];
    var["params"].setBlob(mParams);
    var["sampleCount"] = renderData[kInputSampleCount]->asTexture(); // Can be nullptr
    var["outputColor"] = renderData[kOutputColor]->asTexture();
    var["outputAlbedo"] = renderData[kOutputAlbedo]->asTexture();
    var["outputSpecularAlbedo"] = renderData[kOutputSpecularAlbedo]->asTexture();
    var["outputIndirectAlbedo"] = renderData[kOutputIndirectAlbedo]->asTexture();
    var["outputNormal"] = renderData[kOutputNormal]->asTexture();
    var["outputReflectionPosW"] = renderData[kOutputReflectionPosW]->asTexture();
    var["outputNRDDiffuseRadianceHitDist"] = renderData[kOutputNRDDiffuseRadianceHitDist]->asTexture();
    var["outputNRDSpecularRadianceHitDist"] = renderData[kOutputNRDSpecularRadianceHitDist]->asTexture();
    var["outputNRDDeltaReflectionRadianceHitDist"] = renderData[kOutputNRDDeltaReflectionRadianceHitDist]->asTexture();
    var["outputNRDDeltaTransmissionRadianceHitDist"] = renderData[kOutputNRDDeltaTransmissionRadianceHitDist]->asTexture();
    var["outputNRDResidualRadianceHitDist"] = renderData[kOutputNRDResidualRadianceHitDist]->asTexture();

    if (mVarsChanged)
    {
        var["sampleOffset"] = mpSampleOffset; // Can be nullptr
        var["sampleColor"] = mpSampleColor;
        var["sampleGuideData"] = mpSampleGuideData;
        var["sampleNRDRadiance"] = mpSampleNRDRadiance;
        var["sampleNRDHitDist"] = mpSampleNRDHitDist;
        var["sampleNRDEmission"] = mpSampleNRDEmission;
        var["sampleNRDReflectance"] = mpSampleNRDReflectance;
    }

    // Launch one thread per pixel.
    mpResolvePass->execute(pRenderContext, { mParams.frameDim, 1u });
}

Program::DefineList PathTracer::StaticParams::getDefines(const PathTracer& owner) const
{
    Program::DefineList defines;

    // Path tracer configuration.
    defines.add("SAMPLES_PER_PIXEL", (owner.mFixedSampleCount ? std::to_string(samplesPerPixel) : "0")); // 0 indicates a variable sample count
    defines.add("MAX_SURFACE_BOUNCES", std::to_string(maxSurfaceBounces));
    defines.add("MAX_DIFFUSE_BOUNCES", std::to_string(maxDiffuseBounces));
    defines.add("MAX_SPECULAR_BOUNCES", std::to_string(maxSpecularBounces));
    defines.add("MAX_TRANSMISSON_BOUNCES", std::to_string(maxTransmissionBounces));
    defines.add("ADJUST_SHADING_NORMALS", adjustShadingNormals ? "1" : "0");
    defines.add("USE_BSDF_SAMPLING", useBSDFSampling ? "1" : "0");
    defines.add("USE_NEE", useNEE ? "1" : "0");
    defines.add("USE_MIS", useMIS ? "1" : "0");
    defines.add("USE_RUSSIAN_ROULETTE", useRussianRoulette ? "1" : "0");
    defines.add("USE_RTXDI", useRTXDI ? "1" : "0");
    defines.add("USE_ALPHA_TEST", useAlphaTest ? "1" : "0");
    defines.add("USE_LIGHTS_IN_DIELECTRIC_VOLUMES", useLightsInDielectricVolumes ? "1" : "0");
    defines.add("DISABLE_CAUSTICS", disableCaustics ? "1" : "0");
    defines.add("PRIMARY_LOD_MODE", std::to_string((uint32_t)primaryLodMode));
    defines.add("USE_NRD_DEMODULATION", useNRDDemodulation ? "1" : "0");
    defines.add("COLOR_FORMAT", std::to_string((uint32_t)colorFormat));
    defines.add("MIS_HEURISTIC", std::to_string((uint32_t)misHeuristic));
    defines.add("MIS_POWER_EXPONENT", std::to_string(misPowerExponent));

    // Sampling utilities configuration.
    assert(owner.mpSampleGenerator);
    defines.add(owner.mpSampleGenerator->getDefines());

    if (owner.mpEmissiveSampler) defines.add(owner.mpEmissiveSampler->getDefines());
    if (owner.mpRTXDI) defines.add(owner.mpRTXDI->getDefines());

    defines.add("INTERIOR_LIST_SLOT_COUNT", std::to_string(maxNestedMaterials));

    defines.add("GBUFFER_ADJUST_SHADING_NORMALS", owner.mGBufferAdjustShadingNormals ? "1" : "0");

    // Scene-specific configuration.
    const auto& scene = owner.mpScene;
    if (scene) defines.add(scene->getSceneDefines());
    defines.add("USE_ENV_LIGHT", scene && scene->useEnvLight() ? "1" : "0");
    defines.add("USE_ANALYTIC_LIGHTS", scene && scene->useAnalyticLights() ? "1" : "0");
    defines.add("USE_EMISSIVE_LIGHTS", scene && scene->useEmissiveLights() ? "1" : "0");
    defines.add("USE_CURVES", scene && scene->hasGeometryType(Scene::GeometryType::Curve) ? "1" : "0");
	defines.add("USE_SDF_GRIDS", scene && scene->hasGeometryType(Scene::GeometryType::SDFGrid) ? "1" : "0");
    defines.add("USE_HAIR_MATERIAL", scene && scene->getMaterialCountByType(MaterialType::Hair) > 0u ? "1" : "0");

    // Set default (off) values for additional features.
    defines.add("USE_VIEW_DIR", "0");
    defines.add("OUTPUT_GUIDE_DATA", "0");
    defines.add("OUTPUT_NRD_DATA", "0");
    defines.add("OUTPUT_NRD_ADDITIONAL_DATA", "0");

    return defines;
}
