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

#include <limits>

const RenderPass::Info VBufferSW::kInfo { "VBufferSW", "Software rasterizer V-buffer generation pass." };
const size_t VBufferSW::kMaxGroupThreads = 128;
const size_t VBufferSW::kMeshletMaxTriangles = VBufferSW::kMaxGroupThreads;

#ifndef UINT32_MAX
#define UINT32_MAX (0xffffffff)
#endif 

namespace {
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
        { "normals",        "gNormals",         "Normals buffer",                  true /* optional */, ResourceFormat::RG32Float   },
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

static inline const std::string& to_define_string(RasterizerState::CullMode mode) {
    static const std::string kCullNone = "CULL_NONE";
    static const std::string kCullFront = "CULL_FRONT";
    static const std::string kCullBack = "CULL_BACK";

    switch(mode) {
        case RasterizerState::CullMode::Front:
            return kCullFront;
        case RasterizerState::CullMode::Back:
            return kCullBack;
        default:
            return kCullNone;
    }
}

VBufferSW::SharedPtr VBufferSW::create(RenderContext* pRenderContext, const Dictionary& dict) {
    return SharedPtr(new VBufferSW(pRenderContext->device(), dict));
}

VBufferSW::VBufferSW(Device::SharedPtr pDevice, const Dictionary& dict): GBufferBase(pDevice, kInfo), mpCamera(nullptr) {
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

    pRenderContext->clearUAV(mpLocalDepthBuffer->getUAV().get(), uint4(UINT32_MAX));
    //pRenderContext->clearUAV(renderData[kVBufferName]->asTexture()->getUAV().get(), uint4(0));

    if(!mpMeshletDrawListBuffer) return;

    // Random numbers [0, 1]
    float2 rnd = mpRandomSampleGenerator ? (mpRandomSampleGenerator->next() + float2(.5f)) : float2(.0f);

    const bool doTesselate = mUseDisplacement || mUseSubdivisions;

    if( !mpComputeJitterPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kProgramComputeJitterGenFile).csEntry("build").setShaderModel("6_5");

        Program::DefineList defines;
        
        mpComputeJitterPass = ComputePass::create(mpDevice, desc, defines, true);
    }

    // Create tesselation pass.
    if (doTesselate && (!mpComputeTesselatorPass || mDirty)) {

        bool computeDOF = mUseDOF && mpScene->getCamera()->getApertureRadius() > 0.f;

        Program::Desc desc;
        desc.addShaderLibrary(kProgramComputeTesselatorFile).csEntry("tesselate").setShaderModel("6_5");
        desc.addTypeConformances(mpScene->getTypeConformances());

        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());    
        defines.add("COMPUTE_DEPTH_OF_FIELD", computeDOF ? "1" : "0");
        defines.add("USE_ALPHA_TEST", mUseAlphaTest ? "1" : "0");
        defines.add("THREADS_COUNT", std::to_string(kMaxGroupThreads));
        
        defines.add(getValidResourceDefines(kVBufferExtraChannels, renderData));

        mpComputeTesselatorPass = ComputePass::create(mpDevice, desc, defines, true);

        // Bind static resources
        ShaderVar var = mpComputeTesselatorPass->getRootVar();
        mpScene->setRaytracingShaderData(pRenderContext, var);
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

        if(computeDOF) {
            defines.add("COMPUTE_DEPTH_OF_FIELD", "1");
        } else {
            defines.remove("COMPUTE_DEPTH_OF_FIELD");
        }

        if(computeMotionBlur) {
            defines.add("COMPUTE_MOTION_BLUR", "1");
        } else {
            defines.remove("COMPUTE_MOTION_BLUR");
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

        defines.add("USE_ALPHA_TEST", mUseAlphaTest ? "1" : "0");
        defines.add("THREADS_COUNT", std::to_string(kMaxGroupThreads));
        defines.add("CULL_MODE", to_define_string(mCullMode));

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
    }
/*
    if(!mpComputeReconstructPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kProgramComputeReconstructFile).csEntry("reconstruct").setShaderModel("6_5");
        desc.addTypeConformances(mpScene->getTypeConformances());

        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        defines.add(getValidResourceDefines(kVBufferExtraChannels, renderData));

        mpComputeReconstructPass = ComputePass::create(mpDevice, desc, defines, true);
    }
*/
    const uint32_t meshletDrawsCount = mpMeshletDrawListBuffer ? mpMeshletDrawListBuffer->getElementCount() : 0;
    const uint32_t groupsX = meshletDrawsCount * kMaxGroupThreads;

    if(doTesselate && mpComputeTesselatorPass) {
        ShaderVar var = mpComputeTesselatorPass->getRootVar();
        var["gVBufferSW"]["frameDim"] = mFrameDim;
        var["gVBufferSW"]["sampleNumber"] = mSampleNumber;
        var["gVBufferSW"]["dispatchX"] = groupsX;
        var["gVBufferSW"]["meshletDrawsCount"] = meshletDrawsCount;

        var["PerFrameCB"]["gMaxEdgeLength"] = 2;
        var["PerFrameCB"]["gMaxEdgeDivs"] = 2;

        var["gMeshletDrawList"] = mpMeshletDrawListBuffer;
        
        // Bind resources.
        var["gIndicesBuffer"] = mpIndicesBuffer;
        var["gPrimIndicesBuffer"] = mpPrimIndicesBuffer;
        var["gPositionsBuffer"] = mpPositionsBuffer;
        var["gCocsBuffer"] = mpCocsBuffer;
    }

    {
        ShaderVar var = mpComputeRasterizerPass->getRootVar();
        var["gVBufferSW"]["frameDim"] = mFrameDim;
        var["gVBufferSW"]["frameDimInv"] = mInvFrameDim;
        var["gVBufferSW"]["frameDimInv2"] = mInvFrameDim * 2.0f;
        var["gVBufferSW"]["sampleNumber"] = mSampleNumber;
        var["gVBufferSW"]["dispatchX"] = groupsX;
        var["gVBufferSW"]["meshletDrawsCount"] = meshletDrawsCount;
        var["gVBufferSW"]["rnd"] = rnd;
        
        var["gHiZBuffer"] = mpHiZBuffer;
        var["gLocalDepthBuffer"] = mpLocalDepthBuffer;

        var["gMeshletDrawList"] = mpMeshletDrawListBuffer;
        
        var["gJitterTexture"] = mpJitterTexture;
        var["gJitterSampler"] = mpJitterSampler;

        // Bind resources.
        var["gIndicesBuffer"]       = mpIndicesBuffer;
        var["gPrimIndicesBuffer"]   = mpPrimIndicesBuffer;
        var["gPositionsBuffer"]     = mpPositionsBuffer;
        var["gCocsBuffer"]          = mpCocsBuffer;

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
/*
    {
        ShaderVar var = mpComputeReconstructPass->getRootVar();
        var["gVBufferSW"]["frameDim"] = mFrameDim;

        // Bind resources.
        var["gVBuffer"] = getOutput(renderData, kVBufferName);
        
        var["gHiZBuffer"] = mpHiZBuffer;
        var["gLocalDepthBuffer"] = mpLocalDepthBuffer;
        
        // Bind output channels as UAV buffers.
        auto bind = [&](const ChannelDesc& channel) {
            Texture::SharedPtr pTex = getOutput(renderData, channel.name);
            var[channel.texname] = pTex;
        };

        for (const auto& channel : kVBufferExtraChannels) bind(channel);
    }
*/
    {
        ShaderVar var = mpComputeJitterPass->getRootVar();
        var["PerFrameCB"]["gJitterTextureDim"] = mFrameDim;
        var["PerFrameCB"]["gSampleNumber"] = mSampleNumber;

        // Bind resources.
        var["gJitterTexture"] = mpJitterTexture;
    }

    // Jitter generation pass
    if(mpJitterTexture) {
        const uint2 jitterTexDim = {mpJitterTexture->getWidth(0), mpJitterTexture->getHeight(0)};
        mpComputeJitterPass->execute(pRenderContext, jitterTexDim.x, jitterTexDim.y);
    }

    // Frustum culling pass

    // Tesselator pass
    if(doTesselate && mpComputeTesselatorPass) {
        LLOG_DBG << "Software rasterizer tesselator pass with " << std::to_string(groupsX) << " threads.";
        mpComputeTesselatorPass->execute(pRenderContext, uint3(groupsX, 1, 1));
    }

    // Meshlets rasterization pass
    LLOG_DBG << "Software rasterizer executing compute pass with " << std::to_string(groupsX) << " threads.";
    mpComputeRasterizerPass->execute(pRenderContext, uint3(groupsX, 1, 1));

    // Image reconstruction pass
    //mpComputeReconstructPass->execute(pRenderContext, mFrameDim.x, mFrameDim.y);

    mSampleNumber++;
}

void VBufferSW::createBuffers() {
    if(!mDirty) return;
    
    mpHiZBuffer = Buffer::create(mpDevice, mFrameDim.x * mFrameDim.y * sizeof(uint32_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    
    if(mUseD64) {
        mpLocalDepthBuffer = Buffer::create(mpDevice, mFrameDim.x * mFrameDim.y * sizeof(uint64_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    } else {
        mpLocalDepthBuffer = Buffer::create(mpDevice, mFrameDim.x * mFrameDim.y * sizeof(uint32_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    }

    if(mUseSubdivisions || mUseDisplacement) {
        static const uint32_t kMaxEdgeSubdivisions = 3;
        static constexpr uint32_t kMaxPolygonsCount = MESHLET_MAX_PRIM_COUNT * kMaxEdgeSubdivisions * kMaxEdgeSubdivisions;
        static constexpr uint32_t kMaxIndices       = kMaxPolygonsCount * 3;
        static constexpr uint32_t kMaxPrimIndices   = kMaxPolygonsCount;
        static constexpr uint32_t kMaxPositions     = kMaxIndices;
        static constexpr uint32_t kMaxCocs          = kMaxPositions; 

        mpIndicesBuffer = Buffer::createStructured(mpDevice, sizeof(uint32_t), (uint32_t)kMaxIndices, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpPrimIndicesBuffer = Buffer::createStructured(mpDevice, sizeof(uint32_t), (uint32_t)kMaxPrimIndices, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false); 
        mpPositionsBuffer = Buffer::createStructured(mpDevice, sizeof(float3), (uint32_t)kMaxPositions, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpCocsBuffer = Buffer::createStructured(mpDevice, sizeof(float), (uint32_t)kMaxCocs, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
    }
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

    ResourceFormat format = ResourceFormat::RG16Float;
    if(mUseDOF || mUseMotionBlur) {
        // 4 component jitter needed to decorrelate mblur/dof from anti-aliasing.
        format = ResourceFormat::RGBA16Float;
    }

    mpJitterTexture = Texture::create2D(mpDevice, 128, 128, format, 1, 1, nullptr, Texture::BindFlags::ShaderResource | Texture::BindFlags::UnorderedAccess);
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