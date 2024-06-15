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
#include "Falcor/RenderGraph/RenderPassStandardFlags.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/Scene/SceneDefines.slangh"

#include "VBufferSW.MicroTriangle.slangh"

#include <limits>

const RenderPass::Info VBufferSW::kInfo { "VBufferSW", "Software rasterizer V-buffer generation pass." };
const uint32_t VBufferSW::kMaxGroupThreads = 128;
const uint32_t VBufferSW::kMeshletMaxTriangles = VBufferSW::kMaxGroupThreads;
const uint32_t VBufferSW::kMeshletMaxVertices = VBufferSW::kMaxGroupThreads * 2;
static const uint32_t kInvalidIndex = 0xffffffff;
static const uint32_t kMaxLOD = 4u;

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

    // Ray tracing settings that affect the traversal stack size. Set as small as possible.
    const uint32_t kMaxPayloadSizeBytes = 4; // TODO: The shader doesn't need a payload, set this to zero if it's possible to pass a null payload to TraceRay()
    const uint32_t kMaxRecursionDepth = 1;

    const std::string kVBufferName = "vbuffer";
    const std::string kVBufferDesc = "V-buffer in packed format (indices + barycentrics)";


    const std::string kOuputTime       = "time";
    const std::string kOuputAUX        = "aux";
    const std::string kOutputDrawCount = "drawCount";

    // Additional output channels.
    const ChannelList kVBufferExtraChannels = {
        { "normW",          "gNormW",           "Surface normal in world space",   true /* optional */, ResourceFormat::RGBA32Uint },
        { "depth",          "gDepth",           "Depth buffer (NDC)",              true /* optional */, ResourceFormat::R32Float    },
        { "mvec",           "gMotionVector",    "Motion vector",                   true /* optional */, ResourceFormat::RG32Float   },
        { "viewW",          "gViewW",           "View direction in world space",   true /* optional */, ResourceFormat::RGBA32Float }, // TODO: Switch to packed 2x16-bit snorm format.
        { "texGrads",       "gTextureGrads",    "Texture coordinate gradients",    true /* optional */, ResourceFormat::RGBA16Float },

        { "meshlet_id",     "gMeshletID",       "Meshlet id",                      true /* optional */, ResourceFormat::R32Uint     },
        { "micropoly_id",   "gMicroPolyID",     "MicroPolygon id",                 true /* optional */, ResourceFormat::R32Uint     },

        // Debug channels
        { kOuputAUX,        "gAUX",             "Auxiliary debug buffer",          true /* optional */, ResourceFormat::RGBA32Float },
        { kOuputTime,       "gTime",            "Per-pixel execution time",        true /* optional */, ResourceFormat::R32Uint     },
        { kOutputDrawCount, "gDrawCount",       "Draw count debug buffer",         true /* optional */, ResourceFormat::R32Uint     },
    };
};

VBufferSW::SharedPtr VBufferSW::create(RenderContext* pRenderContext, const Dictionary& dict) {
    return SharedPtr(new VBufferSW(pRenderContext->device(), dict));
}

VBufferSW::VBufferSW(Device::SharedPtr pDevice, const Dictionary& dict): GBufferBase(pDevice, kInfo), mpCamera(nullptr) {
    LLOG_DBG << "subgroupSize " << mpDevice->subgroupSize();
    mSubgroupSize = std::max(32u, std::min(mpDevice->subgroupSize(), 64u));
    setMaxSubdivLevel(3u);

    parseDictionary(dict);

    // Create sample generator
    mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_DEFAULT);

    // Jitter texture sampler
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point)
        .setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap)
        .setUnnormalizedCoordinates(true);
    mpJitterSampler = Sampler::create(pDevice, samplerDesc);

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
        else if (key == kCullMode) setCullMode(static_cast<std::string>(value));
        else if (key == kMaxSubdivLevel) setMaxSubdivLevel(static_cast<uint>(value));
        else if (key == kMinScreenEdgeLen) setMinScreenEdgeLen(static_cast<float>(value));
        // TODO: Check for unparsed fields, including those parsed in base classes.
    }
}

RenderPassReflection VBufferSW::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    // Add the required output. This always exists.
    reflector.addOutput(kVBufferName, kVBufferDesc).bindFlags(Resource::BindFlags::UnorderedAccess).format(mVBufferFormat);

    // Add all the other outputs.
    addRenderPassOutputs(reflector, kVBufferExtraChannels, ResourceBindFlags::UnorderedAccess);

    return reflector;
}

void VBufferSW::compile(RenderContext* pRenderContext, const CompileData& compileData) {
    GBufferBase::compile(pRenderContext, compileData);
    mpRandomSampleGenerator = StratifiedSamplePattern::create(1024);
}

void VBufferSW::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    if (!mpDevice->isFeatureSupported(Device::SupportedFeatures::AtomicInt64))  {
        LLOG_FTL << "VBufferSW: Atomic Int64 is not supported by the current device !!!";
        return;
    }

    GBufferBase::execute(pRenderContext, renderData);

    // Update frame dimension based on render pass output.
    auto pOutput = renderData[kVBufferName]->asTexture();
    if (!pOutput) return;

    updateFrameDim(uint2(pOutput->getWidth(), pOutput->getHeight()));

    pRenderContext->clearUAV(pOutput->getUAV().get(), uint4(0));
    clearRenderPassChannels(pRenderContext, kVBufferExtraChannels, renderData);
        
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

    executeCompute(pRenderContext, renderData);
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
    createBuffers();
    createJitterTexture();
    createMeshletDrawList();

    if(!mpMeshletDrawListBuffer) return;

    if(!mpThreadLockBuffer || mpThreadLockBuffer->getElementCount() != mFrameDim.y) {
        mpThreadLockBuffer = Buffer::create(pRenderContext->device(), mFrameDim.y * sizeof(uint32_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    }

    if(mpThreadLockBuffer) pRenderContext->clearUAV(mpThreadLockBuffer->getUAV().get(), uint4(0));
    if(mpLocalDepthBuffer) pRenderContext->clearUAV(mpLocalDepthBuffer->getUAV().get(), uint4(UINT32_MAX));
    //pRenderContext->clearUAV(renderData[kVBufferName]->asTexture()->getUAV().get(), uint4(0));

    uint2 jitterTexDim = mpJitterTexture ? uint2(mpJitterTexture->getWidth(0), mpJitterTexture->getHeight(0)) : uint2(0, 0);

    // Random numbers [0, 1]
    float2 rnd = mpRandomSampleGenerator ? (mpRandomSampleGenerator->next() + float2(.5f)) : float2(.0f);

    if( !mpComputeJitterPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kProgramComputeJitterGenFile).csEntry("build").setShaderModel("6_5");

        Program::DefineList defines;
        
        mpComputeJitterPass = ComputePass::create(mpDevice, desc, defines, true);
    }

    // Create rasterization pass.
    if (!mpComputeRasterizerPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kProgramComputeRasterizerFile).csEntry("rasterize").setShaderModel("6_5");
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

        defines.add("USE_ALPHA_TEST", mUseAlphaTest ? "1" : "0");
        //defines.add("THREADS_COUNT", std::to_string(kMaxGroupThreads));
        defines.add("CULL_MODE", GBufferBase::to_define_string(mCullMode));
        defines.add("MAX_LOD", std::to_string(max_lod));
        defines.add("MAX_MT_PER_THREAD", std::to_string(mMaxMicroTrianglesPerThread));

        // For optional I/O resources, set 'is_valid_<name>' defines to inform the program of which ones it can access.
        // TODO: This should be moved to a more general mechanism using Slang.
        defines.add(getValidResourceDefines(kVBufferExtraChannels, renderData));
        
        defines.add("is_valid_gIndicesBuffer", mpIndicesBuffer != nullptr ? "1" : "0");
        defines.add("is_valid_gPrimIndicesBuffer", mpPrimIndicesBuffer != nullptr ? "1" : "0");
        defines.add("is_valid_gPositionsBuffer", mpPositionsBuffer != nullptr ? "1" : "0");
        defines.add("is_valid_gCocsBuffer", mpCocsBuffer != nullptr ? "1" : "0");

        mpComputeRasterizerPass = ComputePass::create(mpDevice, desc, defines, true);

        // Bind static resources
        ShaderVar var = mpComputeRasterizerPass->getRootVar();
        mpScene->setRaytracingShaderData(pRenderContext, var);
        //mpScene->setNullRaytracingShaderData(pRenderContext, var);
    }

    const uint32_t meshletDrawsCount = mpMeshletDrawListBuffer ? mpMeshletDrawListBuffer->getElementCount() : 0;
    const uint32_t threadsX = meshletDrawsCount * kMaxGroupThreads;
    const uint32_t dispatchX = kMaxGroupThreads;

    {
        ShaderVar var = mpComputeRasterizerPass->getRootVar();
        var["gVBufferSW"]["frameDim"] = mFrameDim;
        var["gVBufferSW"]["frameDimInv"] = mInvFrameDim;
        var["gVBufferSW"]["frameDimInv2"] = mInvFrameDim * 2.0f;
        var["gVBufferSW"]["sampleNumber"] = mSampleNumber;
        var["gVBufferSW"]["dispatchX"] = dispatchX;
        var["gVBufferSW"]["meshletDrawsCount"] = meshletDrawsCount;
        var["gVBufferSW"]["minScreenEdgeLen"] = mMinScreenEdgeLen;
        var["gVBufferSW"]["minScreenEdgeLenSquared"] = mMinScreenEdgeLen * mMinScreenEdgeLen;
        var["gVBufferSW"]["rnd"] = rnd;
        var["gVBufferSW"]["jitterTextureDim"] = jitterTexDim;
        
        var["gLocalDepthBuffer"] = mpLocalDepthBuffer;
        var["gMeshletDrawList"] = mpMeshletDrawListBuffer;
        var["gThreadLockBuffer"] = mpThreadLockBuffer;
        
        var["gMicroTrianglesBuffer"] = mpMicroTrianglesBuffer;
        for(size_t i = 0; i < mMicroTriangleBuffers.size(); ++i) {
            var["gMicroTriangleBuffers"][i] = mMicroTriangleBuffers[i];
        }

        var["gJitterTexture"] = mpJitterTexture;
        var["gJitterSampler"] = mpJitterSampler;

        var["gVBuffer"] = getOutput(renderData, kVBufferName);

        // Bind output channels as UAV buffers.
        auto bind = [&](const ChannelDesc& channel) {
            Texture::SharedPtr pTex = getOutput(renderData, channel.name);
            var[channel.texname] = pTex;
        };

        // Bind extra output channels
        for (const auto& channel : kVBufferExtraChannels) {
            bind(channel);
        }
    }

    // Jitter generation pass
    if(mpComputeJitterPass && mpJitterTexture) {
        ShaderVar var = mpComputeJitterPass->getRootVar();

        var["PerFrameCB"]["gJitterTextureDim"] = jitterTexDim;
        var["PerFrameCB"]["gSampleNumber"] = mSampleNumber;

        // Bind resources.
        var["gJitterTexture"] = mpJitterTexture;

        mpComputeJitterPass->execute(pRenderContext, jitterTexDim.x, jitterTexDim.y);
    }

    // Frustum culling pass

    // Meshlets rasterization pass
    LLOG_TRC << "Software rasterizer dispatchX size " << std::to_string(dispatchX);
    LLOG_TRC << "Software rasterizer threads count " << std::to_string(threadsX);
    
    mpComputeRasterizerPass->execute(pRenderContext, uint3(threadsX, 1, 1));
    mSampleNumber++;
}

void VBufferSW::createBuffers() {
    if(!mDirty) return;

    createMicroTrianglesBuffer();
    
    if(mUseD64) {
        mpLocalDepthBuffer = Buffer::create(mpDevice, mFrameDim.x * mFrameDim.y * sizeof(uint64_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    } else {
        mpLocalDepthBuffer = Buffer::create(mpDevice, mFrameDim.x * mFrameDim.y * sizeof(uint32_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    }
}

void VBufferSW::createMicroTrianglesBuffer() {
    if(!mDirty) return;

    static constexpr uint32_t kMaxMicroTriangles = VBufferSW::kMeshletMaxTriangles * pow(2u, kMaxLOD * 2u);
    const uint32_t maxMicroTrianglesCount = mMaxMicroTrianglesPerThread * kMaxGroupThreads;

    if(mpMicroTrianglesBuffer && (mpMicroTrianglesBuffer->getElementCount() == maxMicroTrianglesCount)) return;

    LLOG_DBG << "Size of MicroTriangle struct is " << sizeof(MicroTriangle) << " bytes";
    LLOG_DBG << "Max MicroTriangles count per thread " << mMaxMicroTrianglesPerThread;
    LLOG_DBG << "Max MicroTriangles buffer size is " << maxMicroTrianglesCount;
    
    size_t total_buffers_size_bytes = 0;

    static bool createCounter = true;

    static const Resource::BindFlags flags = Resource::BindFlags::None;

    mpMicroTrianglesBuffer = Buffer::createStructured(mpDevice, sizeof(MicroTriangle), maxMicroTrianglesCount, flags, Buffer::CpuAccess::None, nullptr, createCounter);

    //total_buffers_size_bytes += maxMicroTrianglesCount * sizeof(MicroTriangle);

    mMicroTriangleBuffers.resize(1024);
    for(size_t i = 0; i < mMicroTriangleBuffers.size(); ++i) {
        mMicroTriangleBuffers[i] = Buffer::createStructured(mpDevice, sizeof(MicroTriangle), mMaxMicroTrianglesPerThread, flags, Buffer::CpuAccess::None, nullptr, false);
        total_buffers_size_bytes += mMaxMicroTrianglesPerThread * sizeof(MicroTriangle);
    }

    LLOG_DBG << "Total MicroTriangle buffers size bytes is " << total_buffers_size_bytes;
    LLOG_DBG << "Total MicroTriangle buffers size megabytes is " << (total_buffers_size_bytes / 1024) / 1024;

    mDirty = true;
}

void VBufferSW::createMeshletDrawList() {
    if(!mDirty || mpScene->meshletsData().empty()) return;

    std::vector<MeshletDraw> meshletsDrawList;
    
    for(uint32_t instanceID = 0; instanceID < mpScene->getGeometryInstanceCount(); ++instanceID) {
        const GeometryInstanceData& instanceData = mpScene->getGeometryInstance(instanceID);
        if(instanceData.getType() != GeometryType::TriangleMesh) continue; // Only triangles now

        const uint32_t meshID = instanceData.geometryID;
        if(mpScene->hasMeshlets(meshID)) {
            const MeshletGroup& meshletGroup = mpScene->meshletGroup(meshID);
            if(meshletGroup.meshlets_count == 0) continue;
            LLOG_TRC << "Mesh " << meshID << " has " << meshletGroup.meshlets_count << " meshlets";
            for(uint32_t i = 0; i < meshletGroup.meshlets_count; ++i) {
                MeshletDraw draw = {};
                draw.instanceID = instanceID;
                draw.meshletID = meshletGroup.meshlet_offset + i;
                draw.drawCount = 1;
                meshletsDrawList.push_back(draw);
            }
        }
    }

    LLOG_DBG << "Meshlets count is " << mpScene->meshletsData().size();
    LLOG_DBG << "Meshlets draw list size is " << meshletsDrawList.size();

    // Create buffers
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