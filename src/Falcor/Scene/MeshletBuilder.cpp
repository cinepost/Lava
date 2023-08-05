#include "Falcor/Scene/MeshletBuilder.h"

namespace Falcor {


MeshletBuilder::MeshletBuilder() {

};

MeshletBuilder::UniquePtr MeshletBuilder::create() {

}

void MeshletBuilder::generateMeshlets(SceneBuilder::ProcessedMesh& mesh) {
  if(mesh.indexData.empty()) {
    LLOG_WRN << "Meshlets generation for non-indexed mesh \"" << mesh.name << "\" not supported yet !!!";
  	return;
  }

  auto& meshletSpecs = mesh.meshletSpecs;
	meshletSpecs.clear();

  static constexpr uint32_t maximumMeshletTriangleIndices = MESHLET_MAX_POLYGONS_COUNT * 3u;
  static constexpr uint32_t maximumMeshletQuadIndices = MESHLET_MAX_POLYGONS_COUNT * 4u;
  
  uint32_t mesh_start_index = 0;

  while(mesh_start_index < mesh.indexData.size()) {
    std::vector<uint32_t> meshletVertices;
    std::vector<uint32_t> meshletPrimIndices;
    std::vector<uint8_t>  meshletIndices;

    std::set<uint32_t> pointIndices; // this set is used to avoid having duplicate vertices within meshlet

    SceneBuilder::MeshletSpec meshletSpec = {};
    meshletSpec.type = MeshletType::Triangles;

    // Run through mesh indices until we reach max number of elements (points or tris)
    for(uint32_t i = mesh_start_index; i < mesh.indexData.size(); i+=3) {
      size_t unique_vertices_count = pointIndices.size();

      std::set<uint32_t>::iterator it0 = pointIndices.find(mesh.indexData[i]);
      std::set<uint32_t>::iterator it1 = pointIndices.find(mesh.indexData[i+1]);
      std::set<uint32_t>::iterator it2 = pointIndices.find(mesh.indexData[i+2]);

      size_t new_vertices_count = (it0 == pointIndices.end() ? 1 : 0) + (it1 == pointIndices.end() ? 1 : 0) + (it2 == pointIndices.end() ? 1 : 0);

      bool maxVerticesPerMeshletReached = (unique_vertices_count + new_vertices_count) > MESHLET_MAX_VERTICES_COUNT || (pointIndices.size() > MESHLET_MAX_VERTICES_COUNT);
      bool maxIndicesPerMeshletReached =  ((meshletSpec.type == MeshletType::Triangles) && (meshletIndices.size() > maximumMeshletTriangleIndices)) ||
                                          ((meshletSpec.type == MeshletType::Quads) && (meshletIndices.size() > maximumMeshletQuadIndices));

      if (maxVerticesPerMeshletReached || maxIndicesPerMeshletReached) {
        LLOG_TRC << (maxIndicesPerMeshletReached ? std::string("Maximum meshlet polys reached for mesh ") : std::string("Maximum meshlet vertices reached for mesh ")) << mesh.name;
        mesh_start_index = i;
        break;
      }

      // These are per meshlet local indices. Guaranteed not to exceed value of 255 (by default we use max 128 elements)
      meshletIndices.push_back(std::distance(pointIndices.begin(), it0 == pointIndices.end() ? pointIndices.insert(mesh.indexData[i]).first : it0));
      meshletIndices.push_back(std::distance(pointIndices.begin(), it1 == pointIndices.end() ? pointIndices.insert(mesh.indexData[i+1]).first : it1));
      meshletIndices.push_back(std::distance(pointIndices.begin(), it2 == pointIndices.end() ? pointIndices.insert(mesh.indexData[i+2]).first : it2));
      meshletPrimIndices.push_back(i / 3u);
      mesh_start_index += 3;
    }

    for(auto pi : pointIndices) {
    	meshletVertices.push_back(pi);
    }

    meshletSpec.vertices = std::move(meshletVertices);
    meshletSpec.indices = std::move(meshletIndices);
    meshletSpec.primitiveIndices = std::move(meshletPrimIndices);
    LLOG_TRC << "Generated meshlet spec " << meshletSpecs.size() << " for mesh \"" << mesh.name << "\". " << meshletSpec.vertices.size() << 
      " vertices. " << meshletSpec.indices.size() << " indices.";

    meshletSpecs.push_back(std::move(meshletSpec));
  }
  LLOG_DBG << "Generated " << meshletSpecs.size() << " meshlet specs for mesh \"" << mesh.name;
}

}  // namespace Falcor
