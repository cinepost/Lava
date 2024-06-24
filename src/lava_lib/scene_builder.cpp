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
#include <array>
#include <thread>
#include <cmath>
#include <limits>
#include <numeric>

#include "boost/system/error_code.hpp"

#include "Falcor/Core/API/Texture.h"
#include "Falcor/Scene/Material/StandardMaterial.h"

#include "scene_builder.h"
#include "lava_utils_lib/logging.h"

#include "reader_bgeo/bgeo/Run.h"
#include "reader_bgeo/bgeo/Poly.h"
#include "reader_bgeo/bgeo/PrimType.h"
#include "reader_bgeo/bgeo/parser/types.h"
#include "reader_bgeo/bgeo/parser/Attribute.h"
#include "reader_bgeo/bgeo/parser/Detail.h"
#include "reader_bgeo/bgeo/parser/ReadError.h"

namespace lava {    

#define FIX_BGEO_UV

SceneBuilder::SceneBuilder(Falcor::Device::SharedPtr pDevice, Flags buildFlags): Falcor::SceneBuilder(pDevice, buildFlags), mUniqueTrianglesCount(0) {
    mpDefaultMaterial = StandardMaterial::create(pDevice, "default");
    mpDefaultMaterial->setBaseColor({0.4, 0.4, 0.4});
    mpDefaultMaterial->setRoughness(0.33);
    mpDefaultMaterial->setIndexOfRefraction(1.5);
    mpDefaultMaterial->setEmissiveFactor(0.0);
    mpDefaultMaterial->setReflectivity(1.0);
}

SceneBuilder::~SceneBuilder() {
    mAddGeoTasks.wait();
    
    // Remove temporary geometries from filesystem
    freeTemporaryResources();
    
    LLOG_INF << "SceneBuilder stats:";
    LLOG_INF << "\tUnique triangles count: " << std::to_string(mUniqueTrianglesCount);
    LLOG_INF << std::endl;
}

SceneBuilder::SharedPtr SceneBuilder::create(Falcor::Device::SharedPtr pDevice, Flags buildFlags) {
    return SharedPtr(new SceneBuilder(pDevice, buildFlags));
}

Falcor::Scene::SharedPtr SceneBuilder::getScene() {
    return Falcor::SceneBuilder::getScene();
}

// Simple ear-clipping triangulation
static inline uint32_t tesselatePolySimple(const std::vector<float3>& positions, std::vector<uint32_t>& indices, uint32_t startIdx, uint32_t edgesCount) {
    assert(edgesCount > 2u);

    uint32_t mesh_face_count = 0;

    std::vector<uint32_t> _indices(edgesCount + 1);
    std::iota(std::begin(_indices), std::end(_indices), startIdx); // Fill with startIdx, startIdx+1, ...
    _indices.back() = startIdx; // start index at he end to close poly

    while(_indices.size() > 3) {
        std::vector<uint32_t> inner_indices;
        auto indices_count = _indices.size();
        bool evenEdgeCount = indices_count % 2;
        for(uint32_t i = 0; i < (evenEdgeCount ? (indices_count - 1) : (indices_count - 2)); i+=2) {
            // TODO: check if outer triangle edges are concave
            //float a = glm::dot(positions[i2] - positions[i1], positions[i0] - positions[i1]);   
            //if(concave) inner_indices.push_back(_indices[i]) {
            //
            //}    

            indices.push_back(_indices[i+2]);
            indices.push_back(_indices[i+1]);
            indices.push_back(_indices[i]);
            inner_indices.push_back(_indices[i]);
            mesh_face_count++;
        }
        if(!evenEdgeCount)inner_indices.push_back(_indices[indices_count - 2]);
        inner_indices.push_back(_indices[0]);
        _indices = inner_indices;

    }

    return mesh_face_count;
}

uint32_t SceneBuilder::_addGeometry(ika::bgeo::Bgeo::SharedConstPtr pBgeo, const std::string& name) {
    assert(pBgeo);

    const auto pDetail = pBgeo->getDetail();
    if(!pDetail) {
        LLOG_ERR << "Bgeo " << name << " has no gepmetry !!!";
        return SceneBuilder::kInvalidMeshID;
    }

    const int64_t bgeo_point_count = pBgeo->getPointCount();
    const int64_t bgeo_vertex_count = pBgeo->getTotalVertexCount();

    auto pPrimitiveMatrialAttribute =  pBgeo->getPrimitiveAttributeByName("shop_materialpath");
    const bool hasPerPrimitiveMaterial = pPrimitiveMatrialAttribute != nullptr;

    std::vector<std::string> bgeoPerPrimitiveMaterialNames;
    std::vector<int32_t> bgeoPerPrimitiveMaterialIDs;       // id's in bgeoPerPrimitiveMaterialNames list. where -1 means global mesh material 
    std::vector<int32_t> meshPerPrimitiveMaterialIDs;

    if (hasPerPrimitiveMaterial) {
        LLOG_TRC << "Mesh " << name << " has per-primitive materials assigned !";
        pPrimitiveMatrialAttribute->getStrings(bgeoPerPrimitiveMaterialNames);
        pPrimitiveMatrialAttribute->getData(bgeoPerPrimitiveMaterialIDs);

        LLOG_WRN << "Per primitive material indices count:" << bgeoPerPrimitiveMaterialIDs.size();
        LLOG_WRN << "Primitive count:" << pBgeo->getPrimitiveCount();
    }

    LLOG_TRC << "bgeo point count: " << bgeo_point_count;
    LLOG_TRC << "bgeo total vertex count: " << bgeo_vertex_count;
    LLOG_TRC << "bgeo prim count: " << pBgeo->getPrimitiveCount();
    LLOG_TRC << "------------------------------------------------";

    // get basic bgeo data P, N, UV

    std::vector<float3> P;
    pBgeo->getP(P);

    if(P.size() != bgeo_point_count) {
        LLOG_ERR << "P positions count not equal to the bgeo points count !!!";
        return SceneBuilder::kInvalidMeshID;
    }

    LLOG_TRC << "P<float3> size: " << P.size();

    std::vector<float3> N;
    pBgeo->getPointN(N);
    LLOG_TRC << "N<float3> size: " << N.size();
    
    std::vector<float2> UV;
    pBgeo->getPointUV(UV);
    LLOG_TRC << "UV<float2> size: " << UV.size();

    std::vector<float3> vN;
    pBgeo->getVertexN(vN);
    LLOG_TRC << "vN<float3> size: " << vN.size();
    
    std::vector<float2> vUV;
    pBgeo->getVertexUV(vUV);
    LLOG_TRC << "vUV<float2> size: " << vUV.size();

    auto const& vt_map = pDetail->getVertexMap();
    if(vt_map.vertexCount != bgeo_vertex_count) {
        LLOG_ERR << "Bgeo " << name << " detail vertices count not equal to the number of bgeo vertices count !!!";
        return SceneBuilder::kInvalidMeshID;
    }

    const bool hasVertexN = !vN.empty() && (vN.size() == bgeo_vertex_count);
    const bool hasVertexUV  = !vUV.empty() && (vUV.size() == bgeo_vertex_count);

    // vertex attribs
    std::vector<float3> vP;
    std::vector<uint32_t> pointIndices; // per-vertex indices of points
    
    const ika::bgeo::parser::int32* vt_idx_ptr = vt_map.getVertices();

    if(hasVertexN || hasVertexUV || (bgeo_vertex_count != bgeo_point_count)) {
        vP.resize(bgeo_vertex_count);
        pointIndices.resize(bgeo_vertex_count);
        for( ika::bgeo::parser::int64 i = 0; i < vt_map.getVertexCount(); ++i){
            vP[i] = P[vt_idx_ptr[i]];
            pointIndices[i] = vt_idx_ptr[i];
        }
    } else {
        vP = std::move(P);
    }
    
    // build separated verices data

    // fill in normals
    if (!hasVertexN) {
        vN.resize(bgeo_vertex_count);
        if(N.size() == bgeo_point_count) {
            for( ika::bgeo::parser::int64 i = 0; i < vt_map.getVertexCount(); ++i){
                vN[i] = N[vt_idx_ptr[i]];
            }
        } else {
            LLOG_ERR << "Bgeo " << name << " geometry missing normals !!!";
        }
    }

    // fill in texture coordinates
    if (!hasVertexUV) {
        vUV.resize(bgeo_vertex_count);
        if(UV.size() == bgeo_point_count) {
            for( ika::bgeo::parser::int64 i = 0; i < vt_map.getVertexCount(); ++i){
                vUV[i] = UV[vt_idx_ptr[i]];
            }
        } else {
            LLOG_WRN << "Mesh " << name << " has no texture coordinates !";
            for( ika::bgeo::parser::int64 i = 0; i < vt_map.getVertexCount(); ++i){
                vUV[i] = {0.f, 0.f};
            }
        }
    }

    // process primitives and build indices

    uint32_t mesh_face_count = 0;
    uint32_t polygon_id = 0;
    std::vector<uint32_t> vIndices;
    std::vector<int32_t> prim_start_indices;

    for(uint32_t p_i=0; p_i < pBgeo->getPrimitiveCount(); ++p_i) {
        const auto& pPrim = pBgeo->getPrimitive(p_i);
        if(!pPrim) {
            LLOG_WRN << "Unable to get primitive number: " << p_i;
            continue;
        }

        const ika::bgeo::Poly* pPoly;
        prim_start_indices.clear();

        switch (pPrim->getType()) {
            case ika::bgeo::PrimType::PolyPrimType:
                pPoly = std::dynamic_pointer_cast<ika::bgeo::Poly>(pPrim).get();                    
                pPoly->getStartIndices(prim_start_indices);

                // process faces
                int32_t csi; // face current start index
                for( uint32_t i = 0; i < (prim_start_indices.size() - 1); ++i){
                    csi = prim_start_indices[i];
                    uint edgesCount =  prim_start_indices[i+1] - csi;
                    uint32_t face_count = 0;
                    switch(edgesCount) { // number of face sides literally
                        case 0:
                        case 1:
                        case 2:
                            LLOG_ERR << "Polygon sides count should be 3 or more !!!";
                            break;
                        case 3:
                            vIndices.push_back(csi+2);
                            vIndices.push_back(csi+1);
                            vIndices.push_back(csi);
                            face_count = 1;
                            break;
                        default:
                            face_count = tesselatePolySimple(vP, vIndices, csi, edgesCount);
                            break;
                    }
                    if(hasPerPrimitiveMaterial && (face_count > 0)) {
                        for(uint32_t ti = 0; ti < face_count; ++ti) {
                            meshPerPrimitiveMaterialIDs.push_back(bgeoPerPrimitiveMaterialIDs[polygon_id]);
                        }
                        polygon_id++;
                    }
                    mesh_face_count += face_count;
                }
                LLOG_TRC << "prim vertex count: " << pPoly->getVertexCount();
                LLOG_TRC << "prim faces count: " << pPoly->getFaceCount();

                break;
            default:
                LLOG_WRN << "Unsupported prim type \"" + std::string(pPrim->getStrType()) + "\" !!!";
                break;
        }
    }

    LLOG_TRC << "Bgeo primitives iteration done.";

    Mesh mesh;
    mesh.faceCount = mesh_face_count;

    if(hasPerPrimitiveMaterial) {
        mesh.materialIDs.frequency = Mesh::AttributeFrequency::Uniform;
        mesh.materialIDs.pData = (int32_t*)meshPerPrimitiveMaterialIDs.data();

        static const bool createMissing = true;
        auto pMaterialStrings = mesh.materialAttributeStrings(createMissing);
        if (pMaterialStrings) {
            pMaterialStrings->clear();
            for( const auto& material_name: bgeoPerPrimitiveMaterialNames) {
                pMaterialStrings->push_back(material_name);
            }
        } else {
            LLOG_ERR << "Error creating material attribute strings for mesh " << name;
        }
    }

    mesh.positions.frequency = Mesh::AttributeFrequency::Vertex;
    mesh.vertexCount = vP.size();
    mesh.positions.pData = (float3*)vP.data();
    
    mesh.pointIndices.frequency = Mesh::AttributeFrequency::Vertex;
    mesh.pointIndices.pData = (uint32_t*)pointIndices.data();

    mesh.indexCount = vIndices.size();
    mesh.pIndices = (uint32_t*)vIndices.data();
    
    mesh.normals.frequency = Mesh::AttributeFrequency::Vertex;
    mesh.normals.pData = (float3*)vN.data();
    
    mesh.texCrds.frequency = Mesh::AttributeFrequency::Vertex;
    mesh.texCrds.pData = (float2*)vUV.data();
    
    //mesh.pBoneIDs = nullptr;
    //mesh.pBoneWeights = nullptr;
    mesh.topology = Falcor::Vao::Topology::TriangleList;
    mesh.name = name;
    mesh.pMaterial = mpDefaultMaterial;

    mUniqueTrianglesCount += mesh_face_count;

    return addProcessedMesh(processMesh(mesh));
}

uint32_t SceneBuilder::addGeometry(ika::bgeo::Bgeo::SharedConstPtr pBgeo, const std::string& name) {
    const uint32_t id = _addGeometry(pBgeo, name);
    {
        std::scoped_lock lock(mMeshesMutex);
        mMeshMap[name] = id;
    }
    return id;
}

void SceneBuilder::addGeometryAsync(lsd::scope::Geo::SharedConstPtr pGeo, const std::string& name) {
    assert(pGeo);

    // Pass the task to thread pool to run asynchronously
    ThreadPool& pool = ThreadPool::instance();
    mAddGeoTasks.push_back(std::move(pool.submit([this, pGeo, &name]
    {
        uint32_t result = std::numeric_limits<uint32_t>::max();
        ika::bgeo::Bgeo::SharedPtr pBgeo = ika::bgeo::Bgeo::create();

        std::string fullpath = pGeo->detailFilePath().string();
        try {
            pBgeo->readGeoFromFile(fullpath.c_str(), false); // FIXME: don't check version for now
            pBgeo->preCachePrimitives();
        } catch (const ika::bgeo::parser::ReadError& e) {
            LLOG_ERR << "Error parsing bgeo file " << fullpath;
            LLOG_ERR << "Parsing error: " << e.what();
            return result;
        } catch (const std::runtime_error& e) {
            LLOG_ERR << "Error loading bgeo from file " << fullpath;
            LLOG_ERR << e.what();
            return result;
        } catch (...) {
            LLOG_ERR << "Unknown error while loading bgeo from file " << fullpath;
            return result;
        }

        result = this->_addGeometry(pBgeo, name);
        
        return result;
    })));

    { // thread safety
        std::scoped_lock lock(mMeshesMutex);
        mMeshMap[name] = mAddGeoTasks.back();
    }
}

void SceneBuilder::finalize() {
    getScene();
}


}  // namespace lava
