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

#include <limits>

const RenderPass::Info VBufferSW::kInfo { "VBufferSW", "Software rasterizer V-buffer generation pass." };
const size_t VBufferSW::kMaxGroupThreads = 128;
const size_t VBufferSW::kMeshletMaxVertices = VBufferSW::kMaxGroupThreads;
const size_t VBufferSW::kMeshletMaxTriangles = VBufferSW::kMaxGroupThreads;

#ifndef UINT32_MAX
#define UINT32_MAX (0xffffffff)
#endif 

namespace {
    const std::string kProgramComputeRasterizerFile = "RenderPasses/GBuffer/VBuffer/VBufferSW.rasterizer.cs.slang";
    const std::string kProgramComputeReconstructFile = "RenderPasses/GBuffer/VBuffer/VBufferSW.reconstruct.cs.slang";
    const std::string kProgramComputeMeshletsBuilderFile = "RenderPasses/GBuffer/VBuffer/VBufferSW.builder.cs.slang";

    // Scripting options.
    const char kUseCompute[] = "useCompute";
    const char kUseDOF[] = "useDOF";
    const char kUseSubdivisions[] = "useSubdivisions";

    // Ray tracing settings that affect the traversal stack size. Set as small as possible.
    const uint32_t kMaxPayloadSizeBytes = 4; // TODO: The shader doesn't need a payload, set this to zero if it's possible to pass a null payload to TraceRay()
    const uint32_t kMaxRecursionDepth = 1;

    const std::string kVBufferName = "vbuffer";
    const std::string kVBufferDesc = "V-buffer in packed format (indices + barycentrics)";

    // Additional output channels.
    const ChannelList kVBufferExtraChannels = {
        { "normals",        "gNormals",         "Normals buffer",                  true /* optional */, ResourceFormat::RG32Float   },
        { "depth",          "gDepth",           "Depth buffer (NDC)",              true /* optional */, ResourceFormat::R32Float    },
        { "mvec",           "gMotionVector",    "Motion vector",                   true /* optional */, ResourceFormat::RG32Float   },
        { "viewW",          "gViewW",           "View direction in world space",   true /* optional */, ResourceFormat::RGBA32Float }, // TODO: Switch to packed 2x16-bit snorm format.
        
        // Debug channels
        { "time",           "gTime",            "Per-pixel execution time",        true /* optional */, ResourceFormat::R32Uint     },
        { "meshlet_id",     "gMeshletID",       "Meshlet id",                      true /* optional */, ResourceFormat::R32Uint     },
        { "micropoly_id",   "gMicroPolyID",     "MicroPolygon id",                 true /* optional */, ResourceFormat::R32Uint     },

    };
};

VBufferSW::SharedPtr VBufferSW::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    return SharedPtr(new VBufferSW(pRenderContext->device(), dict));
}

VBufferSW::VBufferSW(Device::SharedPtr pDevice, const Dictionary& dict): GBufferBase(pDevice, kInfo) {
    parseDictionary(dict);

    // Create sample generator
    mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_DEFAULT);

    mDirty = true;
}

void VBufferSW::parseDictionary(const Dictionary& dict) {
    GBufferBase::parseDictionary(dict);

    for (const auto& [key, value] : dict) {
        if (key == kUseCompute) mUseCompute = value;
        else if (key == kUseDOF) mUseDOF = value;
        else if (key == kUseSubdivisions) mUseSubdivisions = value;
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

    // If there is no scene, clear the output and return.
    if (mpScene == nullptr) {
        pRenderContext->clearUAV(pOutput->getUAV().get(), uint4(0));
        clearRenderPassChannels(pRenderContext, kVBufferExtraChannels, renderData);
        return;
    }

    // Check for scene changes.
    if (is_set(mpScene->getUpdates(), Scene::UpdateFlags::GeometryChanged) || is_set(mpScene->getUpdates(), Scene::UpdateFlags::SDFGridConfigChanged)) {
        recreatePrograms();
    }

    // Configure depth-of-field.
    // When DOF is enabled, two PRNG dimensions are used. Pass this info to subsequent passes via the dictionary.
    mComputeDOF = mUseDOF && mpScene->getCamera()->getApertureRadius() > 0.f;
    if (mUseDOF) {
        renderData.getDictionary()[Falcor::kRenderPassPRNGDimension] = mComputeDOF ? 2u : 0u;
    }

    executeCompute(pRenderContext, renderData);
    mDirty = false;
}

Dictionary VBufferSW::getScriptingDictionary() {
    Dictionary dict = GBufferBase::getScriptingDictionary();
    dict[kUseCompute] = mUseCompute;
    dict[kUseDOF] = mUseDOF;

    return dict;
}

void VBufferSW::executeCompute(RenderContext* pRenderContext, const RenderData& renderData) {
    recreateBuffers();
    recreateMeshletDrawList();

    pRenderContext->clearUAV(mpLocalDepthPrimBuffer->getUAV().get(), uint4(UINT32_MAX));
    pRenderContext->clearUAV(mpLocalDepthParmBuffer->getUAV().get(), uint4(UINT32_MAX));
    pRenderContext->clearUAV(mpLocalDepthInstBuffer->getUAV().get(), uint4(UINT32_MAX));
    pRenderContext->clearUAV(renderData[kVBufferName]->asTexture()->getUAV().get(), uint4(0));

    if(!mpMeshletDrawListBuffer) return;

    // Create rasterization pass.
    if (!mpComputeRasterizerPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kProgramComputeRasterizerFile).csEntry("rasterize").setShaderModel("6_5");
        desc.addTypeConformances(mpScene->getTypeConformances());

        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        defines.add(mpSampleGenerator->getDefines());
        defines.add("COMPUTE_DEPTH_OF_FIELD", mComputeDOF ? "1" : "0");
        defines.add("USE_ALPHA_TEST", mUseAlphaTest ? "1" : "0");
        defines.add("THREADS_COUNT", std::to_string(kMaxGroupThreads));

        // For optional I/O resources, set 'is_valid_<name>' defines to inform the program of which ones it can access.
        // TODO: This should be moved to a more general mechanism using Slang.
        defines.add(getValidResourceDefines(kVBufferExtraChannels, renderData));

        mpComputeRasterizerPass = ComputePass::create(mpDevice, desc, defines, true);

        // Bind static resources
        ShaderVar var = mpComputeRasterizerPass->getRootVar();
        mpScene->setRaytracingShaderData(pRenderContext, var);
        mpSampleGenerator->setShaderData(var);
    }

    if(!mpComputeReconstructPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kProgramComputeReconstructFile).csEntry("reconstruct").setShaderModel("6_5");
        desc.addTypeConformances(mpScene->getTypeConformances());

        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        defines.add(getValidResourceDefines(kVBufferExtraChannels, renderData));

        mpComputeReconstructPass = ComputePass::create(mpDevice, desc, defines, true);
    }

    const uint32_t meshletDrawsCount = mpMeshletDrawListBuffer ? mpMeshletDrawListBuffer->getElementCount() : 0;
    const uint32_t groupsX = meshletDrawsCount * kMaxGroupThreads;

    {
        ShaderVar var = mpComputeRasterizerPass->getRootVar();
        var["gVBufferSW"]["frameDim"] = mFrameDim;
        var["gVBufferSW"]["frameDimF"] = float2(mFrameDim);
        var["gVBufferSW"]["dispatchX"] = groupsX;
        var["gVBufferSW"]["meshletDrawsCount"] = meshletDrawsCount;
        
        var["gHiZBuffer"] = mpHiZBuffer;
        var["gLocalDepthPrimBuffer"] = mpLocalDepthPrimBuffer;
        var["gLocalDepthParmBuffer"] = mpLocalDepthParmBuffer;
        var["gLocalDepthInstBuffer"] = mpLocalDepthInstBuffer;

        var["gMeshletDrawList"] = mpMeshletDrawListBuffer;

        // Bind output channels as UAV buffers.
        auto bind = [&](const ChannelDesc& channel) {
            Texture::SharedPtr pTex = getOutput(renderData, channel.name);
            var[channel.texname] = pTex;
        };

        for (const auto& channel : kVBufferExtraChannels) bind(channel);
    }

    {
        ShaderVar var = mpComputeReconstructPass->getRootVar();
        var["gVBufferSW"]["frameDim"] = mFrameDim;
        
        // Bind resources.
        var["gVBuffer"] = getOutput(renderData, kVBufferName);
        
        var["gHiZBuffer"] = mpHiZBuffer;
        var["gLocalDepthPrimBuffer"] = mpLocalDepthPrimBuffer;
        var["gLocalDepthParmBuffer"] = mpLocalDepthParmBuffer;
        var["gLocalDepthInstBuffer"] = mpLocalDepthInstBuffer;
        
        // Bind output channels as UAV buffers.
        auto bind = [&](const ChannelDesc& channel) {
            Texture::SharedPtr pTex = getOutput(renderData, channel.name);
            var[channel.texname] = pTex;
        };

        for (const auto& channel : kVBufferExtraChannels) bind(channel);
    }

    // Frustum culling pass

    // Meshlets rsterization pass
    LLOG_DBG << "Software rasterizer executing compute pass with " << std::to_string(groupsX) << " threads.";
    mpComputeRasterizerPass->execute(pRenderContext, uint3(groupsX, 1, 1));

    // Image reconstruction pass
    mpComputeReconstructPass->execute(pRenderContext, mFrameDim.x, mFrameDim.y);
}

void VBufferSW::recreateBuffers() {
    if(!mDirty) return;
    //mpLocalDepth = Texture::create2D(mpDevice, mFrameDim.x, mFrameDim.y, Falcor::ResourceFormat::R32Float, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
    mpHiZBuffer = Buffer::create(mpDevice, mFrameDim.x * mFrameDim.y * sizeof(uint32_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    
    mpLocalDepthPrimBuffer = Buffer::create(mpDevice, mFrameDim.x * mFrameDim.y * sizeof(uint64_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    mpLocalDepthParmBuffer = Buffer::create(mpDevice, mFrameDim.x * mFrameDim.y * sizeof(uint64_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
    mpLocalDepthInstBuffer = Buffer::create(mpDevice, mFrameDim.x * mFrameDim.y * sizeof(uint64_t), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr);
}

void VBufferSW::recreateMeshletDrawList() {
    if(!mDirty || (mpScene->meshlets().size() == 0)) return;

    std::vector<MeshletDraw> meshletsDrawList;
    
    for(uint32_t instanceID = 0; instanceID < mpScene->getGeometryInstanceCount(); ++instanceID) {
        const GeometryInstanceData& instanceData = mpScene->getGeometryInstance(instanceID);
        if(instanceData.getType() != GeometryType::TriangleMesh) continue; // Only triangles now

        const uint32_t meshID = instanceData.geometryID;
        if(mpScene->hasMeshlets(meshID)) {
            const MeshletGroup& meshletGroup = mpScene->meshletGroup(meshID);

            for(uint32_t i = 0; i < meshletGroup.meshlets_count; ++i) {
                MeshletDraw draw = {};
                draw.instanceID = instanceID;
                draw.meshletID = meshletGroup.meshlet_offset + i;
                draw.drawCount = 1;
                meshletsDrawList.push_back(draw);
            }
        }
    }

    LLOG_DBG << "Meshlets count is " << mpScene->meshlets().size();
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

void VBufferSW::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    GBufferBase::setScene(pRenderContext, pScene);
    recreatePrograms();
}

void VBufferSW::recreatePrograms() {
    mpComputeRasterizerPass = nullptr;
    mpComputeFrustumCullingPass = nullptr;
}
