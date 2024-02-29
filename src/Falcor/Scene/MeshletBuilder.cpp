#include <unordered_set>

#include "Falcor/Scene/MeshletBuilder.h"


namespace Falcor {

static constexpr uint32_t maximumMeshletTriangleIndices = MESHLET_MAX_POLYGONS_COUNT * 3u;
static constexpr uint32_t maximumMeshletQuadIndices = MESHLET_MAX_POLYGONS_COUNT * 4u;

struct Cone {
  float px, py, pz;
  float nx, ny, nz;
};

static float computePrimititveCones(std::vector<Cone>& triangles, const SceneBuilder::MeshSpec& mesh) {

  size_t prim_count = mesh.getPrimitivesCount();
  size_t vertex_count = mesh.vertexCount;

  float mesh_area = 0;

  for (size_t i = 0; i < prim_count; ++i) {
    const uint32_t a = mesh.getIndex(i * 3 + 0), b = mesh.getIndex(i * 3 + 1), c = mesh.getIndex(i * 3 + 2);
    assert(a < vertex_count && b < vertex_count && c < vertex_count);

    const float3 p0 = mesh.staticData[a].position;
    const float3 p1 = mesh.staticData[b].position;
    const float3 p2 = mesh.staticData[c].position;

    float p10[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
    float p20[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};

    float normalx = p10[1] * p20[2] - p10[2] * p20[1];
    float normaly = p10[2] * p20[0] - p10[0] * p20[2];
    float normalz = p10[0] * p20[1] - p10[1] * p20[0];

    float area = sqrtf(normalx * normalx + normaly * normaly + normalz * normalz);
    float invarea = (area == 0.f) ? 0.f : 1.f / area;

    triangles[i].px = (p0[0] + p1[0] + p2[0]) / 3.f;
    triangles[i].py = (p0[1] + p1[1] + p2[1]) / 3.f;
    triangles[i].pz = (p0[2] + p1[2] + p2[2]) / 3.f;

    triangles[i].nx = normalx * invarea;
    triangles[i].ny = normaly * invarea;
    triangles[i].nz = normalz * invarea;

    mesh_area += area;
  }

  return mesh_area;
}

struct KDNode {
  union {
    float     split;
    uint32_t  index;
  };

  // leaves: axis = 3, children = number of extra points after this one (0 if 'index' is the only point)
  // branches: axis != 3, left subtree = skip 1, right subtree = skip 1+children
  uint32_t axis : 2;
  uint32_t children : 30;
};

static size_t kdtreePartition(const SceneBuilder::MeshSpec& mesh, uint32_t* kdindices, size_t count, size_t stride, unsigned int axis, float pivot) {
  size_t m = 0;

  // invariant: elements in range [0, m) are < pivot, elements in range [m, i) are >= pivot
  for (size_t i = 0; i < count; ++i) {
    float3 v& = mesh.staticData[indices[i] * stride + axis];

    // swap(m, i) unconditionally
    uint32_t t = indices[m];
    indices[m] = indices[i];
    indices[i] = t;

    // when v >= pivot, we swap i with m without advancing it, preserving invariants
    m += v < pivot;
  }

  return m;
}

static size_t kdtreeBuildLeaf(size_t offset, std::vector<KDNode>& nodes, size_t node_count, uint32_t* kdindices, size_t count) {
  assert(offset + count <= node_count);
  (void)node_count;

  KDNode& result = nodes[offset];

  result.index = kdindices[0];
  result.axis = 3;
  result.children = static_cast<uint32_t>(count - 1);

  // all remaining points are stored in nodes immediately following the leaf
  for (size_t i = 1; i < count; ++i) {
    KDNode& tail = nodes[offset + i];

    tail.index = kdindices[i];
    tail.axis = 3;
    tail.children = ~0u >> 2; // bogus value to prevent misuse
  }

  return offset + count;
}


static size_t kdtreeBuild(size_t offset, std::vector<KDNode>& nodes, size_t node_count, const SceneBuilder::MeshSpec& mesh, uint32_t* kdindices, size_t count, size_t leaf_size) {
  assert(count > 0);
  assert(offset < node_count);

  if (count <= leaf_size) {
    return kdtreeBuildLeaf(offset, nodes, node_count, kdindices, count);
  }

  float mean[3] = {};
  float vars[3] = {};
  float runc = 1, runs = 1;

  // gather statistics on the points in the subtree using Welford's algorithm
  for (size_t i = 0; i < count; ++i, runc += 1.f, runs = 1.f / runc) {
    const float3& point = mesh.staticData[kdindices[i]];

    for (int k = 0; k < 3; ++k) {
      float delta = point[k] - mean[k];
      mean[k] += delta * runs;
      vars[k] += delta * (point[k] - mean[k]);
    }
  }

  // split axis is one where the variance is largest
  uint32_t axis = vars[0] >= vars[1] && vars[0] >= vars[2] ? 0 : vars[1] >= vars[2] ? 1 : 2;

  float split = mean[axis];
  size_t middle = kdtreePartition(mesh, kdindices, count, axis, split);

  // when the partition is degenerate simply consolidate the points into a single node
  if (middle <= leaf_size / 2 || middle >= count - leaf_size / 2) {
    return kdtreeBuildLeaf(offset, nodes, node_count, kdindices, count);
  }

  KDNode& result = nodes[offset];

  result.split = split;
  result.axis = axis;

  // left subtree is right after our node
  size_t next_offset = kdtreeBuild(offset + 1, nodes, node_count, mesh, kdindices, middle, leaf_size);

  // distance to the right subtree is represented explicitly
  result.children = static_cast<uint32_t>(next_offset - offset - 1);

  return kdtreeBuild(next_offset, nodes, node_count, mesh, kdindices + middle, count - middle, leaf_size);
}

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
  size_t prim_count = mesh.getPrimitivesCount();
  
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
    assert(mesh.getIndex(i) < vertex_count);
    adj_counts[mesh.getIndex(i)]++;
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
    uint32_t a = mesh.getIndex(i * 3 + 0), b = mesh.getIndex(i * 3 + 1), c = mesh.getIndex(i * 3 + 2);

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

  std::vector<uint32_t> live_primitives = mesh.adjacency.counts();

  size_t prim_count = mesh.getPrimitivesCount();

  std::vector<uint8_t> emitted_flags(prim_count);
  memset(emitted_flags.data(), 0, prim_count);


  // For each triangle, precompute centroid & normal to use for scoring. TODO: move to GPU compute task
  std::vector<Cone> primitives(prim_count);
  float mesh_area = computePrimititveCones(primitives, mesh);

  // build a kd-tree for nearest neighbor lookup
  std::vector<uint32_t> kdindices(prim_count);
  for (size_t i = 0; i < kdindices.size(); ++i) {
    kdindices[i] = static_cast<uint32_t>(i);
  }

  std::vector<KDNode> nodes(prim_count * 2);
  kdtreeBuild(0, nodes, prim_count * 2, mesh, sizeof(Cone) / sizeof(float), kdindices, face_count, /* leaf_size= */ 8);
}

}  // namespace Falcor
