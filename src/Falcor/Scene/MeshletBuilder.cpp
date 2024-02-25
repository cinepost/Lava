#include <unordered_set>

#include "Falcor/Scene/MeshletBuilder.h"


namespace Falcor {

static constexpr uint32_t maximumMeshletTriangleIndices = MESHLET_MAX_POLYGONS_COUNT * 3u;
static constexpr uint32_t maximumMeshletQuadIndices = MESHLET_MAX_POLYGONS_COUNT * 4u;

MeshletBuilder::MeshletBuilder() {

};

MeshletBuilder::UniquePtr MeshletBuilder::create() {
  return std::move(UniquePtr(new MeshletBuilder()));
}

void MeshletBuilder::generateMeshlets(SceneBuilder::MeshSpec& mesh, BuildMode mode) {
  switch(mode) {
    case BuildMode::MESHOPT: 
      generateMeshletsMeshopt(mesh);
      break;
    default:
      generateMeshletsScan(mesh);
      break;
  }
}

void MeshletBuilder::generateMeshletsScan(SceneBuilder::MeshSpec& mesh) {
  if(mesh.indexCount == 0 || mesh.indexData.empty()) {
    LLOG_WRN << "Meshlets generation for non-indexed mesh \"" << mesh.name << "\" not supported yet !!!";
    return;
  }

  auto& meshletSpecs = mesh.meshletSpecs;
  meshletSpecs.clear();

  if (mesh.use16BitIndices) { assert(mesh.indexCount <= mesh.indexData.size() * 2); }

  uint16_t const* pMesh16BitIndicesData = reinterpret_cast<uint16_t const*>(mesh.indexData.data());

  uint32_t mesh_start_index = 0;

  while(mesh_start_index < mesh.indexCount) {
    std::vector<uint32_t> meshletVertices;
    std::vector<uint32_t> meshletPrimIndices;
    std::vector<uint8_t>  meshletIndices;

    std::set<uint32_t> pointIndices; // this set is used to avoid having duplicate vertices within meshlet
    std::vector<uint3> triangles;
    std::vector<uint4> quads;

    SceneBuilder::MeshletSpec meshletSpec = {};
    meshletSpec.type = MeshletType::Triangles;

    LLOG_TRC << "Meshlet start index: " << mesh_start_index;

    // Run through mesh indices until we reach max number of elements (points or tris)
    for(uint32_t i = mesh_start_index; i < mesh.indexCount; i+=3) {
      const bool maxIndicesPerMeshletReached  = ((meshletSpec.type == MeshletType::Triangles) && (triangles.size() > MESHLET_MAX_POLYGONS_COUNT)) ||
                                                ((meshletSpec.type == MeshletType::Quads) && (quads.size() > MESHLET_MAX_POLYGONS_COUNT));

      if (maxIndicesPerMeshletReached) {
        LLOG_TRC << "Maximum meshlet polys reached for mesh " << mesh.name;
        break;
      }

      uint32_t mIdx0, mIdx1, mIdx2;

      if (mesh.use16BitIndices) {
        mIdx0 = static_cast<uint32_t>(pMesh16BitIndicesData[i]);
        mIdx1 = static_cast<uint32_t>(pMesh16BitIndicesData[i+1]);
        mIdx2 = static_cast<uint32_t>(pMesh16BitIndicesData[i+2]);
      } else {
        mIdx0 = mesh.indexData[i];
        mIdx1 = mesh.indexData[i+1];
        mIdx2 = mesh.indexData[i+2];
      }

      const bool newV0 = pointIndices.find(mIdx0) == pointIndices.end();
      const bool newV1 = pointIndices.find(mIdx1) == pointIndices.end();
      const bool newV2 = pointIndices.find(mIdx2) == pointIndices.end();

      const size_t new_vertices_count = (newV0 ? 1 : 0) + (newV1 ? 1 : 0) + (newV2 ? 1 : 0);

      const bool maxVerticesPerMeshletReached = ((pointIndices.size() + new_vertices_count) > MESHLET_MAX_VERTICES_COUNT);

      if (maxVerticesPerMeshletReached) {
        LLOG_TRC << "Maximum meshlet vertices reached for mesh " << mesh.name;
        break;
      }

      triangles.push_back({mIdx0, mIdx1, mIdx2});

      if(newV0) pointIndices.insert(mIdx0);
      if(newV1) pointIndices.insert(mIdx1);
      if(newV2) pointIndices.insert(mIdx2);

      meshletPrimIndices.push_back(i / 3u);
      mesh_start_index += 3;
    }

    // These are per meshlet local indices. Guaranteed not to exceed value of 255 (by default we use max 128 elements)
    for(const auto& triangle: triangles) {
      meshletIndices.push_back(std::distance(pointIndices.begin(), pointIndices.find(triangle[0])));
      meshletIndices.push_back(std::distance(pointIndices.begin(), pointIndices.find(triangle[1])));
      meshletIndices.push_back(std::distance(pointIndices.begin(), pointIndices.find(triangle[2])));
    }

    for(auto pi : pointIndices) meshletVertices.push_back(pi);

    meshletSpec.vertices = std::move(meshletVertices);
    meshletSpec.indices = std::move(meshletIndices);
    meshletSpec.primitiveIndices = std::move(meshletPrimIndices);
    LLOG_TRC << "Generated meshlet spec " << meshletSpecs.size() << " for mesh \"" << mesh.name << "\". " << meshletSpec.vertices.size() << 
      " vertices. " << meshletSpec.indices.size() << " indices.";

    meshletSpecs.push_back(std::move(meshletSpec));
  }


  LLOG_DBG << "Generated " << meshletSpecs.size() << " meshlet specs for mesh \"" << mesh.name;

}

void MeshletBuilder::buildPrimitiveAdjacency(SceneBuilder::MeshSpec& mesh) {
  size_t prim_count = mesh.getPrimitiveCount();
  
  auto& adjacency = mesh.adjacency;

  auto& adj_counts = adjacency.mCounts;
  auto& adj_offsets = adjacency.mOffsets;
  auto& adj_data = adjacency.mData; 

  adj_counts.resize(mesh.vertexCount);
  adj_offsets.resize(mesh.vertexCount);
  adj_data.resize(mesh.vertexCount);

  // fill prim counts
  memset(adj_counts.data(), 0, mesh.vertexCount * sizeof(uint32_t));

  for (size_t i = 0; i < static_cast<size_t>(mesh.indexCount); ++i) {
    assert(mesh.indexData[i] < vertex_count);
    adj_counts[mesh.indexData[i]]++;
  }

  // fill offset table
  uint32_t offset = 0;

  for (size_t i = 0; i < static_cast<size_t>(mesh.vertexCount); ++i) {
    adj_offsets[i] = offset;
    offset += adj_counts[i];
  }

  assert(offset == index_count);

  // fill triangle data
  for (size_t i = 0; i < prim_count; ++i) {
    uint32_t a = mesh.indexData[i * 3 + 0], b = mesh.indexData[i * 3 + 1], c = mesh.indexData[i * 3 + 2];

    adj_data[adj_offsets[a]++] = uint32_t(i);
    adj_data[adj_offsets[b]++] = uint32_t(i);
    adj_data[adj_offsets[c]++] = uint32_t(i);
  }

  // fix offsets that have been disturbed by the previous pass
  for (size_t i = 0; i < static_cast<size_t>(mesh.vertexCount); ++i) {
    assert(adj_offsets[i] >= adj_counts[i]);
    adj_offsets[i] -= adj_counts[i];
  }

  adjacency.mValid = true;

}

void MeshletBuilder::generateMeshletsMeshopt(SceneBuilder::MeshSpec& mesh) {
  buildPrimitiveAdjacency(mesh);
}

}  // namespace Falcor
