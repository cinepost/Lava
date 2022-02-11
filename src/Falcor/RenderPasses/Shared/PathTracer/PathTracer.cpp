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
#include "stdafx.h"
#include "PathTracer.h"
#include "Experimental/Scene/Material/TexLODTypes.slang"
#include <sstream>

namespace Falcor
{
    namespace
    {
        // Declare the names of channels that we need direct access to. The rest are bound in bulk based on the lists below.
        // TODO: Figure out a cleaner design for this. Should we have enums for all the channels?
        const std::string kViewDirInput = "viewW";

        const std::string kRayCountOutput = "rayCount";
        const std::string kPathLengthOutput = "pathLength";

        const Falcor::ChannelList kGBufferInputChannels =
        {
            { "posW",           "gWorldPosition",             "World-space position (xyz) and foreground flag (w)"       },
            { "normalW",        "gWorldShadingNormal",        "World-space shading normal (xyz)"                         },
            { "tangentW",       "gWorldShadingTangent",       "World-space shading tangent (xyz) and sign (w)", true /* optional */ },
            { "faceNormalW",    "gWorldFaceNormal",           "Face normal in world space (xyz)",                        },
            { kViewDirInput,    "gWorldView",                 "World-space view direction (xyz)", true /* optional */    },
            { "mtlDiffOpacity", "gMaterialDiffuseOpacity",    "Material diffuse color (xyz) and opacity (w)"             },
            { "mtlSpecRough",   "gMaterialSpecularRoughness", "Material specular color (xyz) and roughness (w)"          },
            { "mtlEmissive",    "gMaterialEmissive",          "Material emissive color (xyz)"                            },
            { "mtlParams",      "gMaterialExtraParams",       "Material parameters (IoR, flags etc)"                     },
            { "vbuffer",        "gVBuffer",                   "Visibility buffer in packed format",  true /* optional */, ResourceFormat::Unknown },
        };

        const Falcor::ChannelList kVBufferInputChannels =
        {
            { "vbuffer",        "gVBuffer",                   "Visibility buffer in packed format", false, ResourceFormat::Unknown },
        };

        const Falcor::ChannelList kPixelStatsOutputChannels =
        {
            { kRayCountOutput,  "",                           "Per-pixel ray count", true /* optional */, ResourceFormat::R32Uint    },
            { kPathLengthOutput,"",                           "Per-pixel path length", true /* optional */, ResourceFormat::R32Uint  },
        };

    };

    static_assert(has_vtable<PathTracerParams>::value == false, "PathTracerParams must be non-virtual");
    static_assert(sizeof(PathTracerParams) % 16 == 0, "PathTracerParams size should be a multiple of 16");
    static_assert(kMaxPathLength > 0 && ((kMaxPathLength & (kMaxPathLength + 1)) == 0), "kMaxPathLength should be 2^N-1");

    PathTracer::PathTracer(Device::SharedPtr pDevice, const Dictionary& dict, const ChannelList& outputs)
        : mOutputChannels(outputs)
    {
        // Deserialize pass from dictionary.
        serializePass<true>(dict);
        validateParameters();

        mInputChannels = mSharedParams.useVBuffer ? kVBufferInputChannels : kGBufferInputChannels;

        // Create a sample generator.
        mpSampleGenerator = SampleGenerator::create(mSelectedSampleGenerator);
        assert(mpSampleGenerator);

        // Stats and debugging utils.
        mpPixelStats = PixelStats::create();
        assert(mpPixelStats);
        mpPixelDebug = PixelDebug::create(pDevice);
        assert(mpPixelDebug);
    }

    Dictionary PathTracer::getScriptingDictionary()
    {
        Dictionary dict;
        serializePass<false>(dict);
        return dict;
    }

    RenderPassReflection PathTracer::reflect(const CompileData& compileData)
    {
        RenderPassReflection reflector;

        addRenderPassInputs(reflector, mInputChannels);
        addRenderPassOutputs(reflector, mOutputChannels);
        addRenderPassOutputs(reflector, kPixelStatsOutputChannels);

        return reflector;
    }

    void PathTracer::compile(RenderContext* pRenderContext, const CompileData& compileData)
    {
        mSharedParams.frameDim = compileData.defaultTexDims;
    }

    void PathTracer::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
    {
        mpScene = pScene;
        mSharedParams.frameCount = 0;

        // Lighting setup. This clears previous data if no scene is given.
        if (!initLights(pRenderContext)) throw std::runtime_error("Failed to initialize lights");

        recreateVars(); // Trigger recreation of the program vars.
    }

    void PathTracer::validateParameters()
    {
        if (mSharedParams.lightSamplesPerVertex < 1 || mSharedParams.lightSamplesPerVertex > kMaxLightSamplesPerVertex)
        {
            logError("Unsupported number of light samples per path vertex. Clamping to the range [1," + std::to_string(kMaxLightSamplesPerVertex) + "].");
            mSharedParams.lightSamplesPerVertex = std::clamp(mSharedParams.lightSamplesPerVertex, 1u, kMaxLightSamplesPerVertex);
            recreateVars();
        }

        if (mSharedParams.maxBounces > kMaxPathLength)
        {
            logError("'maxBounces' exceeds the maximum supported path length. Clamping to " + std::to_string(kMaxPathLength));
            mSharedParams.maxBounces = kMaxPathLength;
        }

        if (mSharedParams.maxNonSpecularBounces > mSharedParams.maxBounces)
        {
            logWarning("'maxNonSpecularBounces' exceeds 'maxBounces'. Clamping to " + std::to_string(mSharedParams.maxBounces));
            mSharedParams.maxNonSpecularBounces = mSharedParams.maxBounces;
        }

        if (mSharedParams.specularRoughnessThreshold < 0.f || mSharedParams.specularRoughnessThreshold > 1.f)
        {
            logError("'specularRoughnessThreshold' has invalid value. Clamping to the range [0,1].");
            mSharedParams.specularRoughnessThreshold = std::clamp(mSharedParams.specularRoughnessThreshold, 0.f, 1.f);
        }
    }

    bool PathTracer::initLights(RenderContext* pRenderContext)
    {
        // Clear lighting data for previous scene.
        mpEnvMapSampler = nullptr;
        mpEmissiveSampler = nullptr;
        mUseEmissiveLights = mUseEmissiveSampler = mUseAnalyticLights = mUseEnvLight = false;

        // If we have no scene, we're done.
        if (mpScene == nullptr) return true;

        return true;
    }

    bool PathTracer::updateLights(RenderContext* pRenderContext)
    {
        // If no scene is loaded, we disable everything.
        if (!mpScene)
        {
            mUseAnalyticLights = false;
            mUseEnvLight = false;
            mUseEmissiveLights = mUseEmissiveSampler = false;
            mpEmissiveSampler = nullptr;
            return false;
        }

        bool lightingChanged = false;

        if (is_set(mpScene->getUpdates(), Scene::UpdateFlags::EnvMapChanged))
        {
            mpEnvMapSampler = nullptr;
            lightingChanged = true;
        }

        // Configure light sampling.
        mUseAnalyticLights = mpScene->useAnalyticLights();

        // Configure env map sampling.
        if (mpScene->useEnvLight())
        {
            if (!mpEnvMapSampler)
            {
                mpEnvMapSampler = EnvMapSampler::create(pRenderContext, mpScene->getEnvMap());
                lightingChanged = true;
            }
        }
        else
        {
            if (mpEnvMapSampler)
            {
                mpEnvMapSampler = nullptr;
                lightingChanged = true;
            }
        }
        mUseEnvLight = mpScene->useEnvLight() && mpEnvMapSampler != nullptr;

        // Request the light collection if emissive lights are enabled.
        if (mpScene->getRenderSettings().useEmissiveLights)
        {
            mpScene->getLightCollection(pRenderContext);
        }

        if (!mpScene->useEmissiveLights())
        {
            mUseEmissiveLights = mUseEmissiveSampler = false;
            mpEmissiveSampler = nullptr;
        }
        else
        {
            mUseEmissiveLights = true;
            mUseEmissiveSampler = mSharedParams.useNEE;

            if (!mUseEmissiveSampler)
            {
                mpEmissiveSampler = nullptr;
            }
            else
            {
                // Create emissive light sampler if it doesn't already exist.
                if (mpEmissiveSampler == nullptr)
                {
                    switch (mSelectedEmissiveSampler)
                    {
                    case EmissiveLightSamplerType::Uniform:
                        mpEmissiveSampler = EmissiveUniformSampler::create(pRenderContext, mpScene, mUniformSamplerOptions);
                        break;
                    case EmissiveLightSamplerType::LightBVH:
                        mpEmissiveSampler = LightBVHSampler::create(pRenderContext, mpScene, mLightBVHSamplerOptions);
                        break;
                    case EmissiveLightSamplerType::Power:
                        mpEmissiveSampler = EmissivePowerSampler::create(pRenderContext, mpScene);
                        break;
                    default:
                        logError("Unknown emissive light sampler type");
                    }
                    if (!mpEmissiveSampler) throw std::runtime_error("Failed to create emissive light sampler");

                    recreateVars(); // Trigger recreation of the program vars.
                }

                // Update the emissive sampler to the current frame.
                assert(mpEmissiveSampler);
                lightingChanged = mpEmissiveSampler->update(pRenderContext);
            }
        }

        return lightingChanged;
    }

    // Compute the maximum number of rays per pixel we'll trace. This depends on the current config and scene.
    // This function should be called just before rendering, when everything has been updated.
    uint32_t PathTracer::maxRaysPerPixel() const
    {
        if (!mpScene) return 0;

        // Logic for determining what rays we need to trace. This should match what the shaders are doing.
        bool traceShadowRays = mUseAnalyticLights || mUseEnvLight || mUseEmissiveSampler;
        bool traceScatterRayFromLastPathVertex =
            (mUseEnvLight && mSharedParams.useMIS) ||
            (mUseEmissiveLights && (!mSharedParams.useNEE || mSharedParams.useMIS)) ||
            (mSharedParams.useLegacyBSDF == false); // New BSDF supports delta and transmission events, requiring an extra scatter ray.

        uint32_t shadowRays = traceShadowRays ? mSharedParams.lightSamplesPerVertex * (mSharedParams.maxBounces + 1) : 0;
        uint32_t scatterRays = mSharedParams.maxBounces + (traceScatterRayFromLastPathVertex ? 1 : 0);
        uint32_t raysPerPath = shadowRays + scatterRays;

        return raysPerPath * mSharedParams.samplesPerPixel;
    }

    bool PathTracer::beginFrame(RenderContext* pRenderContext, const RenderData& renderData)
    {
        // Update lights. Returns true if emissive lights have changed.
        bool lightingChanged = updateLights(pRenderContext);

        mMaxRaysPerPixel = maxRaysPerPixel();

        // Update refresh flag if changes that affect the output have occured.
        auto& dict = renderData.getDictionary();
        if (mOptionsChanged || lightingChanged)
        {
            auto flags = dict.getValue(kRenderPassRefreshFlags, Falcor::RenderPassRefreshFlags::None);
            if (mOptionsChanged) flags |= Falcor::RenderPassRefreshFlags::RenderOptionsChanged;
            if (lightingChanged) flags |= Falcor::RenderPassRefreshFlags::LightingChanged;
            dict[Falcor::kRenderPassRefreshFlags] = flags;
            mOptionsChanged = false;
        }

        // Check if GBuffer has adjusted shading normals enabled.
        mGBufferAdjustShadingNormals = dict.getValue(Falcor::kRenderPassGBufferAdjustShadingNormals, false);

        // If we have no scene, just clear the outputs and return.
        if (!mpScene)
        {
            for (auto it : mOutputChannels)
            {
                Texture* pDst = renderData[it.name]->asTexture().get();
                if (pDst) pRenderContext->clearTexture(pDst);
            }
            return false;
        }

        // Configure depth-of-field.
        if (mpScene->getCamera()->getApertureRadius() > 0.f)
        {
            if (!mSharedParams.useVBuffer && renderData[kViewDirInput] == nullptr)
            {
                // The GBuffer path currently expects the view-dir input, give a warning if it is not available.
                logWarning("Depth-of-field requires the '" + std::string(kViewDirInput) + "' G-buffer input. Expect incorrect shading.");
            }
            else if (mSharedParams.useVBuffer)
            {
                // TODO: Add the view-dir input or better compute it in the shader. Until then, show a warning.
                logWarning("Depth-of-field is currently not supported with V-buffer input. Expect incorrect shading.");
            }
        }

        // Get the PRNG start dimension from the dictionary as preceeding passes may have used some dimensions for lens sampling.
        mSharedParams.prngDimension = dict.keyExists(kRenderPassPRNGDimension) ? dict[kRenderPassPRNGDimension] : 0u;

        // Enable pixel stats if rayCount or pathLength outputs are connected.
        if (renderData[kRayCountOutput] != nullptr || renderData[kPathLengthOutput] != nullptr) mpPixelStats->setEnabled(true);

        // Check a vBuffer is attached for ray footprint.
        if (mIsRayFootprintSupported && renderData["vbuffer"] == nullptr)
        {
            logWarning("Disabling ray footprint since it requires a vbuffer input.");
            mIsRayFootprintSupported = false;
            mSharedParams.rayFootprintMode = 0;
        }

        // Update the spread angle parameter for ray footprint.
        const uint2 targetDim = renderData.getDefaultTextureDims();
        assert(targetDim.x > 0 && targetDim.y > 0);
        mSharedParams.screenSpacePixelSpreadAngle = mpScene->getCamera()->computeScreenSpacePixelSpreadAngle(targetDim.y);

        mpPixelDebug->beginFrame(pRenderContext, renderData.getDefaultTextureDims());
        mpPixelStats->beginFrame(pRenderContext, renderData.getDefaultTextureDims());

        return true;
    }

    void PathTracer::endFrame(RenderContext* pRenderContext, const RenderData& renderData)
    {
        mpPixelDebug->endFrame(pRenderContext);
        mpPixelStats->endFrame(pRenderContext);

        auto copyTexture = [pRenderContext](Texture* pDst, const Texture* pSrc)
        {
            if (pDst && pSrc)
            {
                assert(pDst && pSrc);
                assert(pDst->getFormat() == pSrc->getFormat());
                assert(pDst->getWidth() == pSrc->getWidth() && pDst->getHeight() == pSrc->getHeight());
                pRenderContext->copyResource(pDst, pSrc);
            }
            else if (pDst)
            {
                pRenderContext->clearUAV(pDst->getUAV().get(), uint4(0, 0, 0, 0));
            }
        };

        // Copy pixel stats to outputs if available.
        copyTexture(renderData[kRayCountOutput]->asTexture().get(), mpPixelStats->getRayCountTexture(pRenderContext).get());
        copyTexture(renderData[kPathLengthOutput]->asTexture().get(), mpPixelStats->getPathLengthTexture().get());

        mSharedParams.frameCount++;
    }

    void PathTracer::setStaticParams(Program* pProgram) const
    {
        // Set compile-time constants on the given program.
        // TODO: It's unnecessary to set these every frame. It should be done lazily, but the book-keeping is complicated.
        Program::DefineList defines;
        defines.add("SAMPLES_PER_PIXEL", std::to_string(mSharedParams.samplesPerPixel));
        defines.add("LIGHT_SAMPLES_PER_VERTEX", std::to_string(mSharedParams.lightSamplesPerVertex));
        defines.add("MAX_BOUNCES", std::to_string(mSharedParams.maxBounces));
        defines.add("MAX_NON_SPECULAR_BOUNCES", std::to_string(mSharedParams.maxNonSpecularBounces));
        defines.add("USE_ALPHA_TEST", mSharedParams.useAlphaTest ? "1" : "0");
        defines.add("ADJUST_SHADING_NORMALS", mSharedParams.adjustShadingNormals ? "1" : "0");
        defines.add("FORCE_ALPHA_ONE", mSharedParams.forceAlphaOne ? "1" : "0");
        defines.add("USE_ANALYTIC_LIGHTS", mUseAnalyticLights ? "1" : "0");
        defines.add("USE_EMISSIVE_LIGHTS", mUseEmissiveLights ? "1" : "0");
        defines.add("USE_ENV_LIGHT", mUseEnvLight ? "1" : "0");
        defines.add("USE_ENV_BACKGROUND", mpScene->useEnvBackground() ? "1" : "0");
        defines.add("USE_BRDF_SAMPLING", mSharedParams.useBRDFSampling ? "1" : "0");
        defines.add("USE_NEE", mSharedParams.useNEE ? "1" : "0");
        defines.add("USE_MIS", mSharedParams.useMIS ? "1" : "0");
        defines.add("MIS_HEURISTIC", std::to_string(mSharedParams.misHeuristic));
        defines.add("USE_RUSSIAN_ROULETTE", mSharedParams.useRussianRoulette ? "1" : "0");
        defines.add("USE_VBUFFER", mSharedParams.useVBuffer ? "1" : "0");
        defines.add("USE_NESTED_DIELECTRICS", mSharedParams.useNestedDielectrics ? "1" : "0");
        defines.add("USE_LIGHTS_IN_DIELECTRIC_VOLUMES", mSharedParams.useLightsInDielectricVolumes ? "1" : "0");
        defines.add("DISABLE_CAUSTICS", mSharedParams.disableCaustics ? "1" : "0");

        // Defines in MaterialShading.slang.
        defines.add("_USE_LEGACY_SHADING_CODE", mSharedParams.useLegacyBSDF ? "1" : "0");

        defines.add("GBUFFER_ADJUST_SHADING_NORMALS", mGBufferAdjustShadingNormals ? "1" : "0");

        // Defines for ray footprint.
        defines.add("RAY_FOOTPRINT_MODE", std::to_string(mSharedParams.rayFootprintMode));
        defines.add("RAY_CONE_MODE", std::to_string(mSharedParams.rayConeMode));
        defines.add("RAY_FOOTPRINT_USE_MATERIAL_ROUGHNESS", std::to_string(mSharedParams.rayFootprintUseRoughness));

        pProgram->addDefines(defines);
    }

#ifdef SCRIPTING
    SCRIPT_BINDING(PathTracer)
    {
        // Register our parameters struct.
        ScriptBindings::SerializableStruct<PathTracerParams> params(m, "PathTracerParams");
#define field(f_) field(#f_, &PathTracerParams::f_)
        // General
        params.field(samplesPerPixel);
        params.field(lightSamplesPerVertex);
        params.field(maxBounces);
        params.field(maxNonSpecularBounces);

        params.field(useVBuffer);
        params.field(useAlphaTest);
        params.field(adjustShadingNormals);
        params.field(forceAlphaOne);

        params.field(clampSamples);
        params.field(clampThreshold);
        params.field(specularRoughnessThreshold);

        // Sampling
        params.field(useBRDFSampling);
        params.field(useNEE);
        params.field(useMIS);
        params.field(misHeuristic);

        params.field(misPowerExponent);
        params.field(useRussianRoulette);
        params.field(probabilityAbsorption);
        params.field(useFixedSeed);

        params.field(useLegacyBSDF);
        params.field(useNestedDielectrics);
        params.field(useLightsInDielectricVolumes);
        params.field(disableCaustics);

        // Ray footprint
        params.field(rayFootprintMode);
        params.field(rayConeMode);
        params.field(rayFootprintUseRoughness);
#undef field
    }
#endif  // SCRIPTING

}