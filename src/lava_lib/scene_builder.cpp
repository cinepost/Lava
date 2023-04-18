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

#include "Falcor/Core/API/Texture.h"
#include "Falcor/Scene/Material/StandardMaterial.h"

#include "scene_builder.h"
#include "lava_utils_lib/logging.h"

#include "reader_bgeo/bgeo/Run.h"
#include "reader_bgeo/bgeo/Poly.h"
#include "reader_bgeo/bgeo/PrimType.h"
#include "reader_bgeo/bgeo/parser/types.h"
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
    LLOG_INF << "\nSceneBuilder stats:";
    LLOG_INF << "\t Unique triangles count: " << std::to_string(mUniqueTrianglesCount);
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

    uint32_t face_count = 0;

    std::vector<uint32_t> _indices(edgesCount + 1);
    std::iota(std::begin(_indices), std::end(_indices), startIdx); // Fill with startIdx, startIdx+1, ...
    _indices.back() = startIdx; // start index at he end to close poly

    while(_indices.size() > 3) {
        std::vector<uint32_t> inner_indices;
        auto indices_count = _indices.size();
        bool evenEdgeCount = indices_count % 2;
        for(uint32_t i = 0; i < (evenEdgeCount ? (indices_count - 1) : (indices_count - 2)); i+=2) {
            uint32_t i0 = _indices[i];
            uint32_t i1 = _indices[i+1];
            uint32_t i2 = _indices[i+2];
            
            // TODO: check if outer triangle edges are concave
            //float a = glm::dot(positions[i2] - positions[i1], positions[i0] - positions[i1]);   
            //if(concave) inner_indices.push_back(_indices[i]) {
            //
            //}    

            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i0);
            inner_indices.push_back(i0);
            face_count++;
        }
        if(!evenEdgeCount)inner_indices.push_back(_indices[indices_count - 2]);
        inner_indices.push_back(_indices[0]);
        _indices = inner_indices;

    }

    return face_count;
}

uint32_t SceneBuilder::addGeometry(ika::bgeo::Bgeo::SharedConstPtr pBgeo, const std::string& name) {
    assert(pBgeo);

    const int64_t bgeo_point_count = pBgeo->getPointCount();
    const int64_t bgeo_vertex_count = pBgeo->getTotalVertexCount();

    LLOG_TRC << "bgeo point count: " << bgeo_point_count;
    LLOG_TRC << "bgeo total vertex count: " << bgeo_vertex_count;
    LLOG_TRC << "bgeo prim count: " << pBgeo->getPrimitiveCount();
    LLOG_TRC << "------------------------------------------------";

    // get basic bgeo data P, N, UV

    std::vector<float> P;
    pBgeo->getP(P);
    assert(P.size() / 3 == bgeo_point_count && "P positions count not equal to the bgeo points count !!!");
    LLOG_TRC << "P<float> size: " << P.size();

    std::vector<float> N;
    pBgeo->getPointN(N);
    LLOG_TRC << "N<float> size: " << N.size();
    
    std::vector<float> UV;
    pBgeo->getPointUV(UV);
    LLOG_TRC << "UV<float> size: " << UV.size();

    std::vector<float> vN;
    pBgeo->getVertexN(vN);
    LLOG_TRC << "vN<float> size: " << vN.size();
    
    std::vector<float> vUV;
    pBgeo->getVertexUV(vUV);
    LLOG_TRC << "vUV<float> size: " << vUV.size();

    bool unique_points = false; // separate points only if we have any vertex data present
    if(vN.size() || vUV.size()) unique_points = true; 

    // separated verices data arrays
    std::vector<float3> positions;
    std::vector<float3> normals;
    std::vector<float2> uv_coords;
    
    // build separated verices data
    if(unique_points) {
        auto pDetail = pBgeo->getDetail();
        auto const& vt_map = pDetail->getVertexMap();

        assert(vt_map.vertexCount == bgeo_vertex_count && "Bgeo detail vertices count not equal to the number of bgeo vertices count !!!");

        ika::bgeo::parser::int32 point_idx;
        const ika::bgeo::parser::int32* vt_idx_ptr = vt_map.getVertices();
        
        if( unique_points) {
            positions.resize(bgeo_vertex_count);
            // fill in vertex positions
            for( ika::bgeo::parser::int64 i = 0; i < vt_map.getVertexCount(); i++){
                point_idx = vt_idx_ptr[i] * 3;
                positions[i] = {P[point_idx], P[point_idx+1], P[point_idx+2]};
            }
        }

        // fill in normals
        if (vN.size() || N.size()) normals.resize(bgeo_vertex_count);
        if (vN.size()) {
            size_t ii = 0;
            assert(vN.size() / 3 == bgeo_vertex_count && "Vertex normals count not equal to the number of bgeo verices count !!!");
            // use vertex normals
            for( size_t i = 0; i < bgeo_vertex_count; i++){
                normals[i] = {vN[ii], vN[ii+1], vN[ii+2]};
                ii += 3;
            }
        } else if (N.size() && unique_points) {
            // use point normals
            assert(N.size() == P.size() && "Point normals count not equal to the number of bgeo points !!!");

            for( ika::bgeo::parser::int64 i = 0; i < vt_map.getVertexCount(); i++){
                point_idx = vt_idx_ptr[i] * 3;
                normals[i] = {N[point_idx], N[point_idx+1], N[point_idx+2]};
            }
        }

        // fill in texture coordinates
        if (vUV.size() || UV.size()) uv_coords.resize(bgeo_vertex_count);
        if (vUV.size()) {
            size_t ii = 0;
            assert(vUV.size()/2 == bgeo_vertex_count && "Vertex texture coordinates count not equal to the number of bgeo verices count !!!");
            // use vertex normals
            for( size_t i = 0; i < bgeo_vertex_count; i++){
                uv_coords[i] = {
                    vUV[ii], 
                    vUV[ii+1]
                };
                ii += 2;
            }
        } else if (UV.size() && unique_points) {
            // use point normals
            assert(UV.size()/2 == P.size()/3 && "Point texture coordinates count not equal to the number of bgeo points !!!");

            for( ika::bgeo::parser::int64 i = 0; i < vt_map.getVertexCount(); i++){
                point_idx = vt_idx_ptr[i]*2;
                uv_coords[i] = {
                    UV[point_idx], 
                    UV[point_idx+1]
                };
            }
        } else {
            // no coords provided from bgeo. fill with zeroes as this field is required
            for(auto& uv: uv_coords) {
                uv = {0.0, 0.0};
            }
        }
    }

    // process primitives and build indices

    uint32_t face_count = 0;
    std::vector<uint32_t> indices;
    std::vector<int32_t> prim_start_indices;

    for(uint32_t p_i=0; p_i < pBgeo->getPrimitiveCount(); p_i++) {
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

                if (unique_points) {
                    
                    pPoly->getStartIndices(prim_start_indices);

                    // process faces
                    int32_t csi; // face current start index
                    int32_t nsi; // next face start index
                    for( uint32_t i = 0; i < (prim_start_indices.size() - 1); i++){
                        csi = prim_start_indices[i];
                        nsi = prim_start_indices[i+1];
                        uint edgesCount = nsi-csi;
                        switch(edgesCount) { // number of face sides literally
                            case 0:
                            case 1:
                            case 2:
                                LLOG_ERR << "Polygon sides count should be 3 or more !!!";
                                break;
                            case 3:
                                indices.push_back(csi+2);
                                indices.push_back(csi+1);
                                indices.push_back(csi);
                                face_count += 1;
                                break;
                            /*    
                            case 4:
                                indices.push_back(csi+2);
                                indices.push_back(csi+1);
                                indices.push_back(csi);

                                indices.push_back(csi+3);
                                indices.push_back(csi+2);
                                indices.push_back(csi);
                                face_count += 2;
                                break;
                            case 5:
                                indices.push_back(csi+2);
                                indices.push_back(csi+1);
                                indices.push_back(csi);

                                indices.push_back(csi+3);
                                indices.push_back(csi+2);
                                indices.push_back(csi);

                                indices.push_back(csi+4);
                                indices.push_back(csi+3);
                                indices.push_back(csi);
                                face_count += 3;
                                break;
                            case 6:
                                indices.push_back(csi+2);
                                indices.push_back(csi+1);
                                indices.push_back(csi);

                                indices.push_back(csi+3);
                                indices.push_back(csi+2);
                                indices.push_back(csi);

                                indices.push_back(csi+4);
                                indices.push_back(csi+3);
                                indices.push_back(csi);

                                indices.push_back(csi+4);
                                indices.push_back(csi+3);
                                indices.push_back(csi);

                                indices.push_back(csi+5);
                                indices.push_back(csi+4);
                                indices.push_back(csi);

                                face_count += 4;
                                break;
                            */
                            default:
                                face_count += tesselatePolySimple(positions, indices, csi, edgesCount);
                                break;
                        }
                    }
                    LLOG_TRC << "prim vertex count: " << pPoly->getVertexCount();
                    LLOG_TRC << "prim faces count: " << pPoly->getFaceCount();
                } else {

                }

                break;
            default:
                LLOG_WRN << "Unsupported prim type \"" + std::string(pPrim->getStrType()) + "\" !!!";
                break;
        }
    }

    Mesh mesh;
    mesh.faceCount = face_count;

    if(unique_points) {
        mesh.positions.frequency = Mesh::AttributeFrequency::Vertex;
        mesh.vertexCount = positions.size();
        mesh.positions.pData = (float3*)positions.data();
    } else {
        mesh.positions.frequency = Mesh::AttributeFrequency::FaceVarying;
        mesh.vertexCount = P.size() / 3;
        mesh.positions.pData = (float3*)P.data();
    }

    mesh.indexCount = indices.size();
    mesh.pIndices = (uint32_t*)indices.data();
    
    if(unique_points) {
        mesh.normals.frequency = Mesh::AttributeFrequency::Vertex;
        mesh.normals.pData = (float3*)normals.data();
    } else {
        mesh.normals.frequency = Mesh::AttributeFrequency::FaceVarying;
        mesh.normals.pData = (float3*)N.data();
    }

    if(unique_points) {
        mesh.texCrds.frequency = Mesh::AttributeFrequency::Vertex;
        mesh.texCrds.pData = (float2*)uv_coords.data();
    } else {
        mesh.texCrds.frequency = Mesh::AttributeFrequency::FaceVarying;
        mesh.texCrds.pData = (float2*)UV.data();
    }

    //mesh.pBoneIDs = nullptr;
    //mesh.pBoneWeights = nullptr;
    mesh.topology = Falcor::Vao::Topology::TriangleList;
    mesh.name = name;
    mesh.pMaterial = mpDefaultMaterial;

    mUniqueTrianglesCount += face_count;

    return Falcor::SceneBuilder::addMesh(mesh);
}

std::shared_future<uint32_t> SceneBuilder::addGeometryAsync(lsd::scope::Geo::SharedConstPtr pGeo, const std::string& name) {
    assert(pGeo);

    // Pass the task to thread pool to run asynchronously
    ThreadPool& pool = ThreadPool::instance();
    mAddGeoTasks.push_back(pool.submit([this, pGeo, &name]
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

        result = this->addGeometry(pBgeo, name);
        
        if(pGeo->isTemporary()) {
            // TODO: delete temporary geometry as a deffered job (when no references left or when rendering is done).
            //fs::remove(fullpath);
        }
        
        return result;
    }));

    return mAddGeoTasks.back();
}

void SceneBuilder::finalize() {
    getScene();
}


}  // namespace lava
