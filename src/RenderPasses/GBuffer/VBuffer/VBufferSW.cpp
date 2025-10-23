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
#include "VBufferSW.h"

#include "Scene/HitInfo.h"

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/IndirectCommands.h"

#include "Falcor/RenderGraph/RenderPassStandardFlags.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/Scene/Material/BasicMaterial.h"
#include "Falcor/Scene/SceneDefines.slangh"
#include "Falcor/Utils/Timing/SimpleProfiler.h"

#include "VBufferSW.MicroTriangle.slangh"

#include <limits>

const RenderPass::Info VBufferSW::kInfo { "VBufferSW", "Software rasterizer V-buffer generation pass." };
const uint32_t VBufferSW::kMaxGroupThreads = 128;
const uint32_t VBufferSW::kMeshletMaxTriangles = VBufferSW::kMaxGroupThreads;
const uint32_t VBufferSW::kMeshletMaxVertices = VBufferSW::kMaxGroupThreads * 2;
static const uint32_t kInvalidIndex = 0xffffffff;
static const uint32_t kMaxLOD = 4u;
static const size_t   kSTBNOffsetsCount = 64;

#ifndef UINT32_MAX
#define UINT32_MAX (0xffffffff)
#endif 

namespace {
    const std::string kProgramComputeSubdivDataBuilderFile = "RenderPasses/GBuffer/VBuffer/VBufferSW.SubdivDataBuilder.cs.slang";
    const std::string kProgramComputeJitterGenFile = "RenderPasses/GBuffer/VBuffer/VBufferSW.jittergen.cs.slang";
    const std::string kProgramComputeRasterizerFile = "RenderPasses/GBuffer/VBuffer/VBufferSW.rasterizer.cs.slang";
    const std::string kProgramComputeTesselatorFile = "RenderPasses/GBuffer/VBuffer/VBufferSW.tesselator.cs.slang";
    const std::string kProgramComputeReconstructFile = "RenderPasses/GBuffer/VBuffer/VBufferSW.reconstruct.cs.slang";
    const std::string kProgramComputeMeshletsBuilderFile = "RenderPasses/GBuffer/VBuffer/VBufferSW.builder.cs.slang";

    // Scripting options.
    const char kUseD64[] = "highp_depth";
    const char kPerPixelJitterRaster[] = "per_pixel_jitter";
    const char kCullMode[] = "cullMode";
    const char kUseCompute[] = "useCompute";
    const char kUseDOF[] = "useDOF";
    const char kUseMotionBlur[] = "useMotionBlur";
    const char kUseSubdivisions[] = "useSubdivisions";
    const char kUseDisplacement[] = "useDisplacement";
    const char kMaxSubdivLevel[] = "maxSubdivLevel";
    const char kMinScreenEdgeLen[] = "minScreenEdgeLen";
    const char kOpacityLimit[] = "opacityLimit";

    // Ray tracing settings that affect the traversal stack size. Set as small as possible.
    const uint32_t kMaxPayloadSizeBytes = 4; // TODO: The shader doesn't need a payload, set this to zero if it's possible to pass a null payload to TraceRay()
    const uint32_t kMaxRecursionDepth = 1;

    const std::string kInputDepth = "depth";
    const std::string kVBufferName = "vbuffer";
    const std::string kVBufferDesc = "V-buffer in packed format (indices + barycentrics)";

    const std::string kVisibilityContainerParameterBlockName = "gVisibilityContainer";

    const std::string kOuputOITStartOffset = "oit_start_offset";
    const std::string kOuputTime       = "time";
    const std::string kOuputAUX        = "aux";
    const std::string kOutputDrawCount = "drawCount";
    const std::string kOutputNormal    = "normW";

    const ChannelList kExtraInputOutputChannels = {
        { kInputDepth,            "gDepth",         "Depth buffer",                         true /* optional */, ResourceFormat::Unknown },
    };

    // Additional output channels.
    const ChannelList kVBufferExtraOutputChannels = {
        { "vbuffer",            "gVBuffer",         kVBufferDesc,                      true /* optional */, ResourceFormat::RGBA32Uint  },
        { "mvec",               "gMotionVector",    "Motion vector",                   true /* optional */, ResourceFormat::RG32Float   },
        { "viewW",              "gViewW",           "View direction in world space",   true /* optional */, ResourceFormat::RGBA32Float }, // TODO: Switch to packed 2x16-bit snorm format.
        { "texGrads",           "gTextureGrads",    "Texture coordinate gradients",    true /* optional */, ResourceFormat::RGBA16Float },

        { "meshlet_id",         "gMeshletID",       "Meshlet id",                      true /* optional */, ResourceFormat::R32Uint     },
        { "micropoly_id",       "gMicroPolyID",     "MicroPolygon id",                 true /* optional */, ResourceFormat::R32Uint     },

        // OIT channels
        { kOuputOITStartOffset, "gOITStartOffset",  "OIT start offset buffer",         true /* optional */, ResourceFormat::R32Uint     },

        // Debug channels
        { kOuputAUX,            "gAUX",             "Auxiliary debug buffer",          true /* optional */, ResourceFormat::RGBA32Float },
        { kOuputTime,           "gTime",            "Per-pixel execution time",        true /* optional */, ResourceFormat::R32Uint     },
        { kOutputDrawCount,     "gDrawCount",       "Draw count debug buffer",         true /* optional */, ResourceFormat::R32Uint     },
    };

    // Additional output channels.
    const ChannelList kVBufferExtraSubdChannels = {
        { kOutputNormal,         "gNormW",          "Surface normal in world space",   true /* optional */, ResourceFormat::RGBA32Uint  },
    };
};

VBufferSW::SharedPtr VBufferSW::create(RenderContext* pRenderContext, const Dictionary& dict) {
    return SharedPtr(new VBufferSW(pRenderContext->device(), dict));
}

VBufferSW::VBufferSW(Device::SharedPtr pDevice, const Dictionary& dict): GBufferBase(pDevice, kInfo), mpCamera(nullptr) {
    if (!mpDevice->isShaderModelSupported(ShaderModel::SM6_5)) {
        FALCOR_THROW("VBufferSW: requires Shader Model 6.5 support.");
    }

    LLOG_DBG << "subgroupSize " << mpDevice->subgroupSize();
    mSubgroupSize = mpDevice->subgroupSize();
    setMaxSubdivLevel(3u);

    parseDictionary(dict);

    // Create sample generator
    mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_DEFAULT);

    mpSTBNGenerator = STBNGenerator::create(pDevice, uint3(32, 32, 16), STBNGenerator::Type::Scalar, ResourceFormat::R32Float, true /* async */);

    mpSTBNOffsetGenerator = StratifiedSamplePattern::create(kSTBNOffsetsCount);
    mSTBNOffsets.resize(kSTBNOffsetsCount);
    uint3 stbn_dims = mpSTBNGenerator->getDims();
    for(uint32_t i = 0; i < kSTBNOffsetsCount; ++i) {
        float2 rnd = mpSTBNOffsetGenerator->next();
        mSTBNOffsets[i][0] = static_cast<uint>(rnd[0] * stbn_dims[0]);
        mSTBNOffsets[i][1] = static_cast<uint>(rnd[1] * stbn_dims[1]);
    }   

    mDirty = true;
}

void VBufferSW::parseDictionary(const Dictionary& dict) {
    GBufferBase::parseDictionary(dict);

    for (const auto& [key, value] : dict) {
        if (key == kUseCompute) mUseCompute = static_cast<bool>(value);
        else if (key == kUseMotionBlur) enableMotionBlur(static_cast<bool>(value));
        else if (key == kUseSubdivisions) enableSubdivisions(static_cast<bool>(value));
        else if (key == kUseDisplacement) enableDisplacement(static_cast<bool>(value));
        else if (key == kUseD64) setHighpDepth(static_cast<bool>(value));
        else if (key == kPerPixelJitterRaster) setPerPixelJitter(static_cast<bool>(value));
        else if (key == kUseDOF) enableDepthOfField(static_cast<bool>(value));
        else if (key == kCullMode) setCullMode(value.operator std::string());
        else if (key == kMaxSubdivLevel) setMaxSubdivLevel(static_cast<uint>(value));
        else if (key == kMinScreenEdgeLen) setMinScreenEdgeLen(static_cast<float>(value));
        else if (key == kOpacityLimit) setOpacityLimit(static_cast<float>(value));
        // TODO: Check for unparsed fields, including those parsed in base classes.
    }
}

RenderPassReflection VBufferSW::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    // Add the required output. This always exists.
    reflector.addOutput(kVBufferName, kVBufferDesc).bindFlags(Resource::BindFlags::UnorderedAccess).format(mVBufferFormat);

    // Add all the other input-outputs.
    addRenderPassInputOutputs(reflector, kExtraInputOutputChannels, Resource::BindFlags::UnorderedAccess);

    // Add all the other outputs.
    addRenderPassOutputs(reflector, kVBufferExtraOutputChannels, ResourceBindFlags::UnorderedAccess);

    // Add subd outputs.
    if(mUseSubdivisions) {
        addRenderPassOutputs(reflector, kVBufferExtraSubdChannels, ResourceBindFlags::UnorderedAccess);
    }

    return reflector;
}

void VBufferSW::compile(RenderContext* pRenderContext, const CompileData& compileData) {
    GBufferBase::compile(pRenderContext, compileData);
    mpRandomSampleGenerator = StratifiedSamplePattern::create(1024);
    if(mpVisibilitySamplesContainer) mpVisibilitySamplesContainer->resize(compileData.defaultTexDims.x, compileData.defaultTexDims.y);
}

bool VBufferSW::beginFrame(RenderContext *pContext, const RenderData& renderData) {
    mSampleNumber = 0;
    return true;
}

void VBufferSW::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    if (!mpDevice->isFeatureSupported(Device::SupportedFeatures::AtomicInt64))  {
        LLOG_FTL << "VBufferSW: Atomic Int64 is not supported by the current device !!!";
        return;
    }

    SimpleProfiler profile("VBufferSW::execute");

    GBufferBase::execute(pRenderContext, renderData);

    // Update frame dimension based on render pass output.
    auto pOutput = renderData[kVBufferName]->asTexture();
    if(pOutput) {
        updateFrameDim(uint2(pOutput->getWidth(), pOutput->getHeight()));
        pRenderContext->clearUAV(pOutput->getUAV().get(), uint4(0));
    } else if (mpVisibilitySamplesContainer) {
        updateFrameDim(mpVisibilitySamplesContainer->getResolution());
    } else {
        return;
    }

    clearRenderPassChannels(pRenderContext, kVBufferExtraOutputChannels, renderData);
    
    if(mUseSubdivisions) {
        clearRenderPassChannels(pRenderContext, kVBufferExtraSubdChannels, renderData);
    }
        
    // If there is no scene, clear the output and return.
    if (!mpScene) return;
    

    // Check for scene changes.
    if (is_set(mpScene->getUpdates(), Scene::UpdateFlags::GeometryChanged) || is_set(mpScene->getUpdates(), Scene::UpdateFlags::SDFGridConfigChanged)) {
        createPrograms();
    }

    // Configure depth-of-field.
    // When DOF is enabled, two PRNG dimensions are used. Pass this info to subsequent passes via the dictionary.
    if (mUseDOF) {
        renderData.getDictionary()[Falcor::kRenderPassPRNGDimension] = (mpScene->getCamera()->getApertureRadius() > 0.f) ? 2u : 0u;
    }

    createMeshletDrawList();
    if(!mpMeshletDrawListBuffer) return;

    createBuffers();

    // Configure visibility samples container
    if(mpVisibilitySamplesContainer) {
        // Use already allocated resources to save some memory
        mpVisibilitySamplesContainer->setExternalOpaqueDepthBuffer(mpLocalDepthBuffer);
        //mpVisibilitySamplesContainer->setExternalOpaqueSamplesTexture(renderData[kVBufferName]->asTexture());
        //mpVisibilitySamplesContainer->setExternalOpaqueCombinedNormalsTexture(renderData[kOutputNormal]->asTexture());

        bool storeCombinedNormals = (mUseSubdivisions && (mSubdivMeshletsCount > 0)) || mUseDisplacement;
        mpVisibilitySamplesContainer->storeCombinedNormals(storeCombinedNormals);
        
        bool storeTextureGradients = false;
        if(mpScene && mpScene->getMaterialSystem()) {
            storeTextureGradients = mpScene->getMaterialSystem()->hasTextures();
        }
        mpVisibilitySamplesContainer->storeTextureGradients(storeTextureGradients);

        mpVisibilitySamplesContainer->beginFrame();
    }

    executeCompute(pRenderContext, renderData);

    if(mpVisibilitySamplesContainer) {
        mpVisibilitySamplesContainer->endFrame();
    }

    mDirty = false;
}

Dictionary VBufferSW::getScriptingDictionary() {
    Dictionary dict = GBufferBase::getScriptingDictionary();
    dict[kUseCompute] = mUseCompute;
    dict[kUseSubdivisions] = mUseSubdivisions;
    dict[kUseDisplacement] = mUseDisplacement;
    dict[kUseDOF] = mUseDOF;

    return dict;
}

void VBufferSW::executeCompute(RenderContext* pRenderContext, const RenderData& renderData) {
    createJitterTexture();

    if(!mpThreadLockBuffer || mpThreadLockBuffer->getElementCount() != mFrameDim.y) {
        mpThreadLockBuffer = Buffer::create(pRenderContext->device(), mFrameDim.y * sizeof(uint32_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    }

    if(mpThreadLockBuffer) pRenderContext->clearUAV(mpThreadLockBuffer->getUAV().get(), uint4(0));
    if(mpLocalDepthBuffer) pRenderContext->clearUAV(mpLocalDepthBuffer->getUAV().get(), uint4(UINT32_MAX));
    if(mpOpacityShiftsBuffer) pRenderContext->clearUAV(mpOpacityShiftsBuffer->getUAV().get(), uint4(0));

    auto pStartOffsetBuffer = renderData[kOuputOITStartOffset]->asTexture();
    if(pStartOffsetBuffer) pRenderContext->clearUAV(pStartOffsetBuffer->getUAV().get(), uint4(kInvalidIndex));

    uint2 jitterTexDim = mpJitterTexture ? uint2(mpJitterTexture->getWidth(0), mpJitterTexture->getHeight(0)) : uint2(0, 0);

    // Random numbers [0, 1]
    float2 rnd = mpRandomSampleGenerator ? (mpRandomSampleGenerator->next() + float2(.5f)) : float2(.0f);

    if( !mpComputeJitterPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kProgramComputeJitterGenFile).csEntry("build");

        Program::DefineList defines;
        
        mpComputeJitterPass = ComputePass::create(mpDevice, desc, defines, true);
    }

    // Create rasterization pass.
    if (!mpComputeRasterizerPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kProgramComputeRasterizerFile).csEntry("rasterize");
        desc.addTypeConformances(mpScene->getTypeConformances());

        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        if (mpSampleGenerator) defines.add(mpSampleGenerator->getDefines());
        
        bool computeDOF = mUseDOF && mpScene->getCamera()->getApertureRadius() > 0.f;
        bool computeMotionBlur = mUseMotionBlur;

        defines.remove("COMPUTE_DEPTH_OF_FIELD");
        if(computeDOF) {
            defines.add("COMPUTE_DEPTH_OF_FIELD", "1");
        } else {
            defines.add("COMPUTE_DEPTH_OF_FIELD", "0");
        }

        defines.remove("COMPUTE_MOTION_BLUR");
        if(computeMotionBlur) {
            defines.add("COMPUTE_MOTION_BLUR", "1");
        } else {
            defines.add("COMPUTE_MOTION_BLUR", "0");
        }

        if(mUseD64) {
            defines.add("USE_HIGHP_DEPTH", "1");
        } else {
            defines.remove("USE_HIGHP_DEPTH");
        }

        if(mUsePerPixelJitter) {
            defines.add("USE_PP_JITTER", "1");
        } else {
            defines.remove("USE_PP_JITTER");
        }

        const uint max_lod = ( mUseDisplacement || mUseSubdivisions) ? mMaxLOD : 0;
        if( mUseDisplacement || mUseSubdivisions) {
            //createMicroTrianglesBuffer();
            defines.add("USE_SUBDIVISIONS", "1");
        } else {
            defines.remove("USE_SUBDIVISIONS");
        }

        if(mpVisibilitySamplesContainer) {
            defines.add(mpVisibilitySamplesContainer->getDefines());
            defines.add("USE_VISIBILITY_CONTAINER", "1");
        } else {
            defines.remove("USE_VISIBILITY_CONTAINER");
        }

        defines.add("USE_ALPHA_TEST", mUseAlphaTest ? "1" : "0");
        //defines.add("THREADS_COUNT", std::to_string(kMaxGroupThreads));
        defines.add("CULL_MODE", GBufferBase::to_define_string(mCullMode));
        defines.add("MAX_LOD", std::to_string(max_lod));
        defines.add("MAX_MT_PER_THREAD", std::to_string(mMaxMicroTrianglesPerThread));

        // For optional I/O resources, set 'is_valid_<name>' defines to inform the program of which ones it can access.
        // TODO: This should be moved to a more general mechanism using Slang.
        defines.add(getValidResourceDefines(kVBufferExtraOutputChannels, renderData));
        defines.add(getValidResourceDefines(kVBufferExtraSubdChannels, renderData));
        defines.add(getValidResourceDefines(kExtraInputOutputChannels, renderData));

        defines.add("is_valid_gIndicesBuffer", mpIndicesBuffer != nullptr ? "1" : "0");
        defines.add("is_valid_gPrimIndicesBuffer", mpPrimIndicesBuffer != nullptr ? "1" : "0");
        defines.add("is_valid_gPositionsBuffer", mpPositionsBuffer != nullptr ? "1" : "0");
        defines.add("is_valid_gCocsBuffer", mpCocsBuffer != nullptr ? "1" : "0");

        mpComputeRasterizerPass = ComputePass::create(mpDevice, desc, defines, true);

        // Bind static resources
        if(mpVisibilitySamplesContainer) {
            ShaderVar var = mpComputeRasterizerPass->getRootVar();
            var[kVisibilityContainerParameterBlockName].setParameterBlock(mpVisibilitySamplesContainer->getParameterBlock());
        }
    
        if(mpSTBNGenerator) {
            mpSTBNGenerator->bindShaderData(mpComputeRasterizerPass->getRootVar()["gNoiseGenerator"]);
        }
    }

    const uint32_t meshletDrawsCount = mpMeshletDrawListBuffer ? mpMeshletDrawListBuffer->getElementCount() : 0;
    const uint32_t dispatchX = kMaxGroupThreads;

    {
        auto var = mpComputeRasterizerPass->getRootVar();
        auto cb_var = var["gVBufferSW"];

        cb_var["frameDim"] = mFrameDim;
        cb_var["frameDimInv"] = mInvFrameDim;
        cb_var["frameDimInv2"] = mInvFrameDim * 2.0f;
        cb_var["sampleNumber"] = mSampleNumber;
        cb_var["randomSeed"] = mRandomSeed;
        cb_var["dispatchX"] = dispatchX;
        cb_var["meshletDrawsCount"] = meshletDrawsCount;
        cb_var["minScreenEdgeLen"] = mMinScreenEdgeLen;
        cb_var["minScreenEdgeLenSquared"] = mMinScreenEdgeLen * mMinScreenEdgeLen;
        cb_var["rnd"] = rnd;
        cb_var["jitterTextureDim"] = jitterTexDim;
        cb_var["transparencySamplesCount"] = mTransparencySamplesCount;
        cb_var["drawableIndex"] = kInvalidIndex;
        cb_var["opacityLimit"] = mOpacityLimit;
        
        // Stbn XY offset to get more values along Z axis
        if(!mpSTBNGenerator) {
            cb_var["stbnOffset"] = uint2(0, 0);
        } else {
            uint3 stbn_dims = mpSTBNGenerator->getDims();
            uint wrap_iter = mSampleNumber / stbn_dims[2];
            cb_var["stbnOffset"] = mSTBNOffsets[wrap_iter % mSTBNOffsets.size()];
        }

        var["gLocalDepthBuffer"] = mpLocalDepthBuffer;
        var["gMeshletDrawList"] = mpMeshletDrawListBuffer;
        var["gThreadLockBuffer"] = mpThreadLockBuffer;
        var["gOpacityShiftsBuffer"] = mpOpacityShiftsBuffer;
        
        //var["gMicroTrianglesBuffer"] = mpMicroTrianglesBuffer;
        //for(size_t i = 0; i < mMicroTriangleBuffers.size(); ++i) {
        //   var["gMicroTriangleBuffers"][i] = mMicroTriangleBuffers[i];
        //}

        var["gJitterTexture"] = mpJitterTexture;

        // Bind output channels as UAV buffers.
        auto bind = [&](const ChannelDesc& channel) {
            Texture::SharedPtr pTex = getOutput(renderData, channel.name);
            var[channel.texname] = pTex;
        };

        // Bind extra input-output channels
        for (const auto& channel : kExtraInputOutputChannels) {
            bind(channel);
        }

        // Bind extra output channels
        for (const auto& channel : kVBufferExtraOutputChannels) {
            bind(channel);
        }

        // Bind extra subd output channels
        for (const auto& channel : kVBufferExtraSubdChannels) {
            bind(channel);
        }

        if(mSampleNumber == 0) {
            SimpleProfiler profile("VBufferSW::createBuffers() setRaytracingShaderData");
            // TODO: update raytracing data once per-frame
            mpScene->setRaytracingShaderData(pRenderContext, var);
        }
    }

    // Jitter generation pass
    if(mpComputeJitterPass && mpJitterTexture) {
        ShaderVar var = mpComputeJitterPass->getRootVar();
        auto cb_var = var["PerFrameCB"];

        cb_var["gJitterTextureDim"] = jitterTexDim;
        cb_var["gSampleNumber"] = mSampleNumber;
        cb_var["gRandomSeed"] = mRandomSeed;

        // Bind resources.
        var["gJitterTexture"] = mpJitterTexture;

        mpComputeJitterPass->execute(pRenderContext, jitterTexDim.x, jitterTexDim.y);
    }

    // Frustum culling pass

    // Meshlets rasterization pass
    ShaderVar cb_var = mpComputeRasterizerPass->getRootVar()["gVBufferSW"];
    if(mTransparentMeshletsCount == 0) {
        
        cb_var["drawableOffset"] = 0;
        cb_var["meshletDrawsCount"] = mOpaqueMeshletsCount;
        mpComputeRasterizerPass->execute(pRenderContext, uint3(mOpaqueMeshletsCount, 1, 1));
    } else {
        // Rasterize opaque meshlets first
        cb_var["drawableOffset"] = 0;
        cb_var["meshletDrawsCount"] = mOpaqueMeshletsCount;
        mpComputeRasterizerPass->execute(pRenderContext, uint3(mOpaqueMeshletsCount, 1, 1));

        // Rasterize potentially transparent meshlets second
        cb_var["drawableOffset"] = mOpaqueMeshletsCount;
        cb_var["meshletDrawsCount"] = mTransparentMeshletsCount;
        mpComputeRasterizerPass->execute(pRenderContext, uint3(mTransparentMeshletsCount, 1, 1));
    }

    mSampleNumber++;
}

void VBufferSW::setRandomSeed(int seed) {
    if(mRandomSeed == seed) return;

    mRandomSeed = seed;
}

void VBufferSW::createBuffers() {
    if(!mDirty) return;

    createMicroTrianglesBuffer();
    
    if(mUseD64) {
        mpLocalDepthBuffer = Buffer::create(mpDevice, mFrameDim.x * mFrameDim.y * sizeof(uint64_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    } else {
        mpLocalDepthBuffer = Buffer::create(mpDevice, mFrameDim.x * mFrameDim.y * sizeof(uint32_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    }

    // Opacity shifts buffer
    if(mpScene && mpScene->materialSystem()->hasTransparentMaterials()) {
        size_t opacityShiftsBufferSize = ((mFrameDim.x * mFrameDim.y * sizeof(uint8_t)) << 2) >> 2;
        mpOpacityShiftsBuffer = Buffer::create(mpDevice, opacityShiftsBufferSize, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    }

    // Opacity transparent visibility samples buffer
}

void VBufferSW::createMicroTrianglesBuffer() {
    return;
    if(!mDirty) return;

    const uint32_t maxMicroTrianglesCount = mMaxMicroTrianglesPerThread * kMaxGroupThreads;

    if(mpMicroTrianglesBuffer && (mpMicroTrianglesBuffer->getElementCount() == maxMicroTrianglesCount)) return;

    LLOG_DBG << "Size of MicroTriangle struct is " << sizeof(MicroTriangle) << " bytes";
    LLOG_DBG << "Max MicroTriangles count per thread " << mMaxMicroTrianglesPerThread;
    LLOG_DBG << "Max MicroTriangles buffer size is " << maxMicroTrianglesCount;
    
    size_t total_buffers_size_bytes = 0;

    static bool createCounter = true;

    static const Resource::BindFlags flags = Resource::BindFlags::None;

    mpMicroTrianglesBuffer = Buffer::createStructured(mpDevice, sizeof(MicroTriangle), maxMicroTrianglesCount, flags, Buffer::CpuAccess::None, nullptr, createCounter);

    mMicroTriangleBuffers.resize(1024);
    for(size_t i = 0; i < mMicroTriangleBuffers.size(); ++i) {
        mMicroTriangleBuffers[i] = Buffer::createStructured(mpDevice, sizeof(MicroTriangle), mMaxMicroTrianglesPerThread, flags, Buffer::CpuAccess::None, nullptr, false);
        total_buffers_size_bytes += mMaxMicroTrianglesPerThread * sizeof(MicroTriangle);
    }

    LLOG_DBG << "Total MicroTriangle buffers size bytes is " << total_buffers_size_bytes;
    LLOG_DBG << "Total MicroTriangle buffers size megabytes is " << (total_buffers_size_bytes / 1024) / 1024;

    mDirty = true;
}

bool VBufferSW::isOpaqueMaterial(const Material::SharedPtr& pMaterial) {
    if(!pMaterial) return false;
    auto pBasicMaterial = pMaterial->toBasicMaterial();
    return pBasicMaterial ? pBasicMaterial->isOpaque() : pMaterial->isOpaque();
}

void VBufferSW::createMeshletDrawList() {
    if(!mDirty || !mpScene || mpScene->meshletsData().empty()) {
        return;
    }

    mOpaqueMeshletsCount = 0;
    mTransparentMeshletsCount = 0;
    mTransparentMeshletsStartOffset = kInvalidIndex;
    mSubdivMeshletsCount = 0;

    std::vector<MeshletDraw> meshletsDrawList;
    std::vector<MeshletDraw> nonOpaqueMeshletsDrawList;

    for(uint32_t instanceID = 0; instanceID < mpScene->getGeometryInstanceCount(); ++instanceID) {
        const GeometryInstanceData& instanceData = mpScene->getGeometryInstance(instanceID);
        if(instanceData.getType() != GeometryType::TriangleMesh) continue; // Only triangles now

        const bool isSubdivInstance = instanceData.isSubdividable();
        const uint32_t meshID = instanceData.geometryID;
        if(mpScene->hasMeshlets(meshID)) {
            const MeshletGroup& meshletGroup = mpScene->meshletGroup(meshID);
            if(meshletGroup.meshlets_count == 0) continue;
            if(isSubdivInstance) mSubdivMeshletsCount++;

            //const MeshDesc& mesh = mpScene->getMesh(meshID);
            
            bool isOpaqueInstanceMaterial = isOpaqueMaterial(mpScene->getMaterial(instanceData.materialID));
            LLOG_TRC << "Mesh " << meshID << " has " << meshletGroup.meshlets_count << " meshlets";
                
            for(uint32_t i = 0; i < meshletGroup.meshlets_count; ++i) {
                MeshletDraw draw = {};
                draw.instanceID = instanceID;
                draw.meshletID = meshletGroup.meshlet_offset + i;
                draw.drawCount = 1;

                bool isOpaqueMehslet = isOpaqueInstanceMaterial;

                if(isOpaqueMehslet) {
                    meshletsDrawList.push_back(draw);
                    mOpaqueMeshletsCount++;
                } else {
                    nonOpaqueMeshletsDrawList.push_back(draw);
                    mTransparentMeshletsCount++;
                }
            }
           
        }
    }

    // Create buffers
    LLOG_DBG << "Meshlets count is " << mpScene->meshletsData().size();
    
    if(mTransparentMeshletsCount == 0) {
        LLOG_DBG << "Meshlets draw list size is " << meshletsDrawList.size();
        mTransparentMeshletsCount = 0;
    } else {
        LLOG_DBG << "Meshlets opaque draw list size is " << meshletsDrawList.size();
        LLOG_DBG << "Meshlets non-opaque draw list size is " << nonOpaqueMeshletsDrawList.size();
        mTransparentMeshletsStartOffset = meshletsDrawList.size();
        meshletsDrawList.insert( meshletsDrawList.end(), nonOpaqueMeshletsDrawList.begin(), nonOpaqueMeshletsDrawList.end() );
    }

    if(!meshletsDrawList.empty()) {
        mpMeshletDrawListBuffer = Buffer::createStructured(
            mpDevice, sizeof(MeshletDraw), meshletsDrawList.size(), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, 
            meshletsDrawList.data()
        );
    } else {
        mpMeshletDrawListBuffer = nullptr;
    }
}

void VBufferSW::preProcessMeshlets() {
    
}

void VBufferSW::createJitterTexture() {
    if(mpJitterTexture && !mDirty) return;

    ResourceFormat format = ResourceFormat::RG32Float;
    if(mUseDOF || mUseMotionBlur) {
        // 4 component jitter needed to decorrelate mblur/dof from anti-aliasing.
        format = ResourceFormat::RGBA32Float;
    }

    mpJitterTexture = Texture::create2D(mpDevice, 256, 256, format, 1, 1, nullptr, Texture::BindFlags::ShaderResource | Texture::BindFlags::UnorderedAccess);
}

void VBufferSW::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    GBufferBase::setScene(pRenderContext, pScene);
    
    if(mpScene) {
        mpCamera = mpScene->getCamera();
    } else {
        mpCamera = nullptr;
    }
    
    mSubdivMeshletsCount = 0;
    createPrograms();
}

void VBufferSW::createPrograms() {
    mpComputeJitterPass = nullptr;
    mpComputeTesselatorPass = nullptr;
    mpComputeRasterizerPass = nullptr;
    mpComputeFrustumCullingPass = nullptr;
}

void VBufferSW::enableSubdivisions(bool value) {
    if (mUseSubdivisions == value) return;
    mUseSubdivisions = value;
    requestRecompile();
    mDirty = true;
}

void VBufferSW::enableDisplacement(bool value) {
    if (mUseDisplacement == value) return;
    mUseDisplacement = value;
    mDirty = true;
}

void VBufferSW::setPerPixelJitter(bool value) {
    if (mUsePerPixelJitter == value) return;
    mUsePerPixelJitter = value;
    mDirty = true;
}

void VBufferSW::enableDepthOfField(bool value) {
    if(mUseDOF == value) return;
    mUseDOF = value;
    mDirty = true;
}

void VBufferSW::enableMotionBlur(bool value) {
    if(mUseMotionBlur == value) return;
    mUseMotionBlur = value;
    mDirty = true;
}

void VBufferSW::setCullMode(RasterizerState::CullMode mode) {
    if(mCullMode == mode) return;
    GBufferBase::setCullMode(mode);
    mDirty = true;
}

void VBufferSW::setHighpDepth(bool state) {
    if(mUseD64 == state) return;
    mUseD64 = state;
    mDirty = true;
}

void VBufferSW::setMaxSubdivLevel(uint level) {
    if(mMaxLOD == level) return;
    static const uint kLowerSubdLevel = 0u;
    static const uint kUpperSubdLevel = kMaxLOD;
    mMaxLOD = std::max(kLowerSubdLevel, std::min(kUpperSubdLevel, level));
    mMaxMicroTrianglesPerThread = std::max(1u, static_cast<uint32_t>(pow(2u,  std::min(mMaxLOD, kMaxLOD) * 2u)));
    mDirty = true;
}

void VBufferSW::setMinScreenEdgeLen(float len) {
    if(mMinScreenEdgeLen == len) return;
    static const float kLowerScreenLen = 2.0f;
    static const float kUpperScreenLen = 100.0f;
    mMinScreenEdgeLen = std::max(kLowerScreenLen, std::min(len, kUpperScreenLen));
    mDirty = true;
}

void VBufferSW::setCullMode(const std::string& mode_str) {
    RasterizerState::CullMode mode = RasterizerState::CullMode::Back;
    if(mode_str == "back") {
        mode = RasterizerState::CullMode::Back;
    } else if(mode_str == "front") {  
        mode = RasterizerState::CullMode::Front;
    } else {
        mode = RasterizerState::CullMode::None;
    }
    setCullMode(mode);
}

void VBufferSW::setOpacityLimit(float limit) {
    limit = std::clamp(limit, 0.f, 1.f);
    if(limit == mOpacityLimit) return;
    mOpacityLimit = limit;
    mDirty = true;
}