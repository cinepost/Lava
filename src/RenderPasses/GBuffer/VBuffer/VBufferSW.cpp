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
    const std::string kProgramComputeMeshletsBuilderFile = "RenderPasses/GBuffer/VBuffer/VBufferSW.builder.cs.slang";

    // Scripting options.
    const char kUseCompute[] = "useCompute";
    const char kUseDOF[] = "useDOF";

    // Ray tracing settings that affect the traversal stack size. Set as small as possible.
    const uint32_t kMaxPayloadSizeBytes = 4; // TODO: The shader doesn't need a payload, set this to zero if it's possible to pass a null payload to TraceRay()
    const uint32_t kMaxRecursionDepth = 1;

    const std::string kVBufferName = "vbuffer";
    const std::string kVBufferDesc = "V-buffer in packed format (indices + barycentrics)";

    // Additional output channels.
    const ChannelList kVBufferExtraChannels = {
        { "depth",          "gDepth",           "Depth buffer (NDC)",               true /* optional */, ResourceFormat::R32Float    },
        { "mvec",           "gMotionVector",    "Motion vector",                    true /* optional */, ResourceFormat::RG32Float   },
        { "viewW",          "gViewW",           "View direction in world space",    true /* optional */, ResourceFormat::RGBA32Float }, // TODO: Switch to packed 2x16-bit snorm format.
        { "time",           "gTime",            "Per-pixel execution time",         true /* optional */, ResourceFormat::R32Uint     },
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
}

void VBufferSW::parseDictionary(const Dictionary& dict) {
    GBufferBase::parseDictionary(dict);

    for (const auto& [key, value] : dict) {
        if (key == kUseCompute) mUseCompute = value;
        else if (key == kUseDOF) mUseDOF = value;
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
}

Dictionary VBufferSW::getScriptingDictionary() {
    Dictionary dict = GBufferBase::getScriptingDictionary();
    dict[kUseCompute] = mUseCompute;
    dict[kUseDOF] = mUseDOF;

    return dict;
}

void VBufferSW::executeCompute(RenderContext* pRenderContext, const RenderData& renderData) {
    recreateBuffers();
    recreateMeshlets();

    pRenderContext->clearUAV(mpLocalDepth->getUAV().get(), uint4(UINT32_MAX));
    pRenderContext->clearUAV(renderData[kVBufferName]->asTexture()->getUAV().get(), uint4(0));

    if(!mpMeshletDrawListBuffer || !mpMeshletsBuffer) return;

    // Create rasterization pass.
    if (!mpComputeRasterizerPass) {
        Program::Desc desc;
        desc.addShaderLibrary(kProgramComputeRasterizerFile).csEntry("rasterize").setShaderModel("6_5");
        desc.addTypeConformances(mpScene->getTypeConformances());

        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        defines.add(mpSampleGenerator->getDefines());
        defines.add("COMPUTE_DEPTH_OF_FIELD", mComputeDOF ? "1" : "0");
        defines.add("USE_ALPHA_TEST", mUseAlphaTest ? "1" : "0");
        defines.add("MESHLET_MAX_ELEMENTS_SIZE", std::to_string(kMaxGroupThreads));

        // For optional I/O resources, set 'is_valid_<name>' defines to inform the program of which ones it can access.
        // TODO: This should be moved to a more general mechanism using Slang.
        defines.add(getValidResourceDefines(kVBufferExtraChannels, renderData));

        mpComputeRasterizerPass = ComputePass::create(mpDevice, desc, defines, true);

        // Bind static resources
        ShaderVar var = mpComputeRasterizerPass->getRootVar();
        mpScene->setRaytracingShaderData(pRenderContext, var);
        mpSampleGenerator->setShaderData(var);
    }

    const uint meshletDrawsCount = mpMeshletDrawListBuffer ? mpMeshletDrawListBuffer->getElementCount() : 0;
    const uint meshletsCount = mpMeshletsBuffer ? mpMeshletsBuffer->getElementCount() : 0;
    const uint groupsX = meshletsCount * kMaxGroupThreads;

    ShaderVar var = mpComputeRasterizerPass->getRootVar();
    var["gVBufferSW"]["frameDim"] = mFrameDim;
    var["gVBufferSW"]["frameDimF"] = float2(mFrameDim);
    var["gVBufferSW"]["dispatchX"] = groupsX;
    var["gVBufferSW"]["meshletsCount"] = meshletsCount;
    var["gVBufferSW"]["meshletDrawsCount"] = meshletDrawsCount;

    // Bind resources.
    var["gVBuffer"] = getOutput(renderData, kVBufferName);
    var["gLocalDepth"] = mpLocalDepth;
    var["gMeshlets"] = mpMeshletsBuffer;
    var["gMeshletDrawList"] = mpMeshletDrawListBuffer;
    var["gMeshletsVertices"] = mpMeshletsVerticesBuffer;
    var["gMeshletsTriangles"] = mpMeshletsTrianglesBuffer;

    // Bind output channels as UAV buffers.
    auto bind = [&](const ChannelDesc& channel) {
        Texture::SharedPtr pTex = getOutput(renderData, channel.name);
        var[channel.texname] = pTex;
    };

    for (const auto& channel : kVBufferExtraChannels) bind(channel);

    LLOG_DBG << "Meshlets draw list size " << std::to_string(meshletDrawsCount);

    // Frustum culling pass

    // Meshlets rsterization pass
    mpComputeRasterizerPass->execute(pRenderContext, uint3(groupsX, 1, 1));
}

void VBufferSW::recreateBuffers() {
    if(!mDirty) return;
    mpLocalDepth = Texture::create2D(mpDevice, mFrameDim.x, mFrameDim.y, Falcor::ResourceFormat::R32Uint, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
}

void VBufferSW::recreateMeshlets() {
    if(!mDirty) return;

    const size_t max_meshlets = 10;
    static const size_t max_vertices = 10;
    static const size_t max_triangles = 10;

/*
struct GeometryInstanceData
{
    static const uint kTypeBits = 3;
    static const uint kTypeOffset = 32 - kTypeBits;

    uint32_t flags;         ///< Upper kTypeBits bits are reserved for storing the type.
    uint globalMatrixID;
    uint materialID;
    uint geometryID;

    uint vbOffset;          ///< Offset into vertex buffer.
    uint ibOffset;          ///< Offset into index buffer, or zero if non-indexed.
    uint instanceIndex;     ///< InstanceIndex in TLAS.
    uint geometryIndex;     ///< GeometryIndex in BLAS.

    uint internalID;        ///< Internal scene id.
    uint externalID;        ///< External dcc id.
    uint _pad0;
    uint _pad1;

#ifdef HOST_CODE
    GeometryInstanceData() = default;

    GeometryInstanceData(GeometryType type): flags((uint32_t)type << kTypeOffset)
    {}
#endif

    GeometryType getType() CONST_FUNCTION {
        return GeometryType(flags >> kTypeOffset);
    }

*/

/*

struct MeshDesc
{
    uint vbOffset;          ///< Offset into global vertex buffer.
    uint ibOffset;          ///< Offset into global index buffer, or zero if non-indexed.
    uint vertexCount;       ///< Vertex count.
    uint indexCount;        ///< Index count, or zero if non-indexed.
    uint skinningVbOffset;  ///< Offset into skinning data buffer, or zero if no skinning data.
    uint prevVbOffset;      ///< Offset into previous vertex data buffer, or zero if neither skinned or animated.
    uint materialID;        ///< Material ID.
    uint flags;             ///< See MeshFlags.

*/


    // Create test meshlets
    std::vector<MeshletDraw> meshletsDrawList;
    MeshletsList  meshlets;
    VerticesList  meshletsVerticesList;
    TrianglesList meshletsTrianglesList;

    // Process meshes
    for(uint32_t meshID = 0; meshID < mpScene->getMeshCount(); ++meshID) {
        const MeshDesc& meshDesc = mpScene->getMesh(meshID);
    }

    // Process instances
    struct MeshletGroup {
        uint meshlet_offset;    // Meshlets buffer offset
        uint meshlets_count;    // Meshlets count
    };

    std::unordered_map<uint32_t, MeshletGroup> meshToMeshletGroupMap; // maps meshes to meshlet groups
    
    for(uint32_t instanceID = 0; instanceID < mpScene->getGeometryInstanceCount(); ++instanceID) {
        const GeometryInstanceData& instanceData = mpScene->getGeometryInstance(instanceID);
        if(instanceData.getType() != GeometryType::TriangleMesh) continue; // Only triangles now

        const uint32_t meshID = instanceData.geometryID;

        // Generate meshlets for the new mesh
        if (meshToMeshletGroupMap.find(meshID) == meshToMeshletGroupMap.end()) {
            const MeshDesc& meshDesc = mpScene->getMesh(meshID);
            const uint meshlets_prev_count = meshlets.size();
            buildMeshlets(meshDesc, meshlets, meshletsVerticesList, meshletsTrianglesList);
            meshToMeshletGroupMap[meshID] = {meshlets_prev_count, static_cast<Falcor::uint>(meshlets.size()) - meshlets_prev_count};
        }

        const auto& meshletGroup = meshToMeshletGroupMap[meshID];

        for(uint32_t i = 0; i < meshletGroup.meshlets_count; ++i) {
            MeshletDraw draw = {};
            draw.instanceID = instanceID;
            draw.meshletID = meshletGroup.meshlet_offset;
            draw.drawCount = 1;
            meshletsDrawList.push_back(draw);
        }
    }

    LLOG_DBG << "Meshlets count is " << meshlets.size();
    LLOG_DBG << "Meshlets draw list size is " << meshletsDrawList.size();

    // test meshlets and instances
    //meshlets.resize(5);
    //meshlets[0].vertex_count = 100;
    //meshlets[1].vertex_count = 20;
    //meshlets[2].vertex_count = 5;
    //meshlets[3].vertex_count = 73;
    //meshlets[4].vertex_count = 128;

    // Create buffers
    if((meshletsDrawList.size() > 0) && (meshlets.size() > 0)) {
        mpMeshletsBuffer = Buffer::createStructured(
            mpDevice, sizeof(Meshlet), meshlets.size(), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, 
            meshlets.data()
        );

        mpMeshletDrawListBuffer = Buffer::createStructured(
            mpDevice, sizeof(MeshletDraw), meshletsDrawList.size(), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, 
            meshletsDrawList.data()
        );

        if (meshletsVerticesList.size() > 0) {
            mpMeshletsVerticesBuffer = Buffer::createTyped<uint32_t>(
                mpDevice, meshletsVerticesList.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, meshletsVerticesList.data());
        }

        if ((meshletsTrianglesList.size() > 0)) {
            mpMeshletsTrianglesBuffer = Buffer::createTyped<uint32_t>(
                mpDevice, meshletsTrianglesList.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, meshletsTrianglesList.data());
        }
    } else {
        mpMeshletsBuffer = nullptr;
        mpMeshletDrawListBuffer = nullptr;
        mpMeshletsVerticesBuffer = nullptr;
        mpMeshletsTrianglesBuffer = nullptr;
    }
}

void VBufferSW::buildMeshlets(const MeshDesc& meshDesc, MeshletsList& meshlets, VerticesList& vertices, TrianglesList& triangles) {
    const uint baseIndex = meshDesc.ibOffset * 4;
    const bool indexedMesh = meshDesc.ibOffset == 0;
    const bool use16BitIndices = indexedMesh && meshDesc.use16BitIndices();
    const uint32_t trianglesCount = meshDesc.indexCount / 3u;

    LLOG_DBG << "Generating meshlets for " << (indexedMesh ? ( use16BitIndices ? "32bit indexed": "16bit indexed"): "non-indexed") 
            << " mesh. " << meshDesc.indexCount << " indices. " << meshDesc.vertexCount << " vertices." << " triangles " << trianglesCount;

    uint32_t generatedMeshletsCount = 0;
    uint32_t meshletTrianglesCount = 0;
    std::set<uint32_t> meshletVertexIndices;

    for(uint32_t triangleIdx = 0; triangleIdx < trianglesCount; triangleIdx+=kMaxGroupThreads) {
        meshlets.push_back({});
        auto& meshlet = meshlets.back();

        //meshlet.triangle_offset = triangles.size();
        meshlet.triangle_offset = meshDesc.ibOffset + triangleIdx * kMaxGroupThreads * 3;
        
        meshlet.triangle_count = ((trianglesCount - triangleIdx) < kMaxGroupThreads) ? (trianglesCount - triangleIdx) : kMaxGroupThreads;
        generatedMeshletsCount++;
    }
    LLOG_DBG << "Generated " << generatedMeshletsCount << " meshlets.";
}

void VBufferSW::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    GBufferBase::setScene(pRenderContext, pScene);
    recreatePrograms();
}

void VBufferSW::recreatePrograms() {
    mpComputeRasterizerPass = nullptr;
    mpComputeFrustumCullingPass = nullptr;
}
