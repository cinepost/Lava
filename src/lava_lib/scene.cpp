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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#include "scene.h"
#include "lava_utils_lib/logging.h"

namespace lava {    

Scene::Scene() {}

Scene::SharedPtr Scene::create(Falcor::Device::SharedPtr pDevice, Falcor::SceneBuilder::Flags buildFlags) {
    auto pScene = Scene::SharedPtr(new Scene());
    
    pScene->mpSceneBuilder = Falcor::SceneBuilder::create(pDevice, buildFlags);
    if(!pScene->mpSceneBuilder) {
        LLOG_FTL << "Error creating falcor scene builder !!!";
        return nullptr;
    }

    return pScene;
}

uint32_t Scene::addMesh(const ika::bgeo::Bgeo& bgeo) {
    const auto& meshes = mpSceneBuilder->meshes();
    const auto& prevMesh = mMeshes.size() ? mMeshes.back() : MeshSpec();

    // Create the new mesh spec
    mMeshes.push_back({});
    MeshSpec& spec = mMeshes.back();
    assert(mBuffersData.staticData.size() <= UINT32_MAX && mBuffersData.dynamicData.size() <= UINT32_MAX && mBuffersData.indices.size() <= UINT32_MAX);
    spec.staticVertexOffset = (uint32_t)mBuffersData.staticData.size();
    spec.dynamicVertexOffset = (uint32_t)mBuffersData.dynamicData.size();
    spec.indexOffset = (uint32_t)mBuffersData.indices.size();
    spec.indexCount = mesh.indexCount;
    spec.vertexCount = mesh.vertexCount;
    spec.topology = mesh.topology;
    spec.materialId = addMaterial(mesh.pMaterial, is_set(mFlags, Flags::RemoveDuplicateMaterials));

    // Error checking
    auto throw_on_missing_element = [&](const std::string& element) {
        throw std::runtime_error("Error when adding the mesh " + mesh.name + " to the scene.\nThe mesh is missing " + element);
    };

    auto missing_element_warning = [&](const std::string& element) {
        logWarning("The mesh " + mesh.name + " is missing the element " + element + ". This is not an error, the element will be filled with zeros which may result in incorrect rendering");
    };

    // Initialize the static data
    if (mesh.indexCount == 0 || !mesh.pIndices) throw_on_missing_element("indices");
    mBuffersData.indices.insert(mBuffersData.indices.end(), mesh.pIndices, mesh.pIndices + mesh.indexCount);

    if (mesh.vertexCount == 0) throw_on_missing_element("vertices");
    if (mesh.pPositions == nullptr) throw_on_missing_element("positions");
    if (mesh.pNormals == nullptr) missing_element_warning("normals");
    if (mesh.pTexCrd == nullptr) missing_element_warning("texture coordinates");

    // Initialize the dynamic data
    if (mesh.pBoneWeights || mesh.pBoneIDs) {
        if (mesh.pBoneIDs == nullptr) throw_on_missing_element("bone IDs");
        if (mesh.pBoneWeights == nullptr) throw_on_missing_element("bone weights");
        spec.hasDynamicData = true;
    }

    // Generate tangent space if that's required
    std::vector<float3> bitangents;
    if (!is_set(mFlags, Flags::UseOriginalTangentSpace) || !mesh.pBitangents) {
        bitangents = MikkTSpaceWrapper::generateBitangents(mesh.pPositions, mesh.pNormals, mesh.pTexCrd, mesh.pIndices, mesh.vertexCount, mesh.indexCount);
    } else {
        validateTangentSpace(mesh.pBitangents, mesh.vertexCount);
    }

    for (uint32_t v = 0; v < mesh.vertexCount; v++) {
        StaticVertexData s;
        s.position = mesh.pPositions[v];
        s.normal = mesh.pNormals ? mesh.pNormals[v] : float3(0, 0, 0);
        s.texCrd = mesh.pTexCrd ? mesh.pTexCrd[v] : float2(0, 0);
        s.bitangent = bitangents.size() ? bitangents[v] : mesh.pBitangents[v];
        mBuffersData.staticData.push_back(PackedStaticVertexData(s));

        if (mesh.pBoneWeights) {
            DynamicVertexData d;
            d.boneWeight = mesh.pBoneWeights[v];
            d.boneID = mesh.pBoneIDs[v];
            d.staticIndex = (uint32_t)mBuffersData.staticData.size() - 1;
            mBuffersData.dynamicData.push_back(d);
        }
    }

    mDirty = true;

    assert(mMeshes.size() <= UINT32_MAX);
    return (uint32_t)mMeshes.size() - 1;
}

}  // namespace lava
