#include <unordered_set>

#include "Falcor/Scene/Geometry.h"
#include "Falcor/Scene/MeshletBuilder.h"


namespace Falcor {

using Adjacency = Geometry::PrimitiveAdjacency;

static constexpr uint32_t kMaximumMeshletTriangleIndices = MESHLET_MAX_POLYGONS_COUNT * 3u;
static constexpr uint32_t kMaximumMeshletQuadIndices = MESHLET_MAX_POLYGONS_COUNT * 4u;

struct Cone {
  float px, py, pz;
  float nx, ny, nz;
};


static float getMeshletScore(float distance2, float spread, float cone_weight, float expected_radius) {
  float cone = 1.f - spread * cone_weight;
  float cone_clamped = cone < 1e-3f ? 1e-3f : cone;

  return (1 + sqrtf(distance2) / expected_radius * (1 - cone_weight)) * cone_clamped;
}

static float computePrimitiveCones(std::vector<Cone>& prim_cones, const SceneBuilder::MeshSpec& mesh) {

  size_t prim_count = mesh.getPrimitivesCount();
  size_t vertex_count = mesh.vertexCount;

  float mesh_area = 0;

  for (size_t i = 0; i < prim_count; ++i) {
    const uint32_t a = mesh.getIndex(i * 3 + 0), b = mesh.getIndex(i * 3 + 1), c = mesh.getIndex(i * 3 + 2);
    assert(a < vertex_count && b < vertex_count && c < vertex_count);

    const float3& p0 = mesh.staticData[a].position;
    const float3& p1 = mesh.staticData[b].position;
    const float3& p2 = mesh.staticData[c].position;

    float p10[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
    float p20[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};

    float normalx = p10[1] * p20[2] - p10[2] * p20[1];
    float normaly = p10[2] * p20[0] - p10[0] * p20[2];
    float normalz = p10[0] * p20[1] - p10[1] * p20[0];

    float area = sqrtf(normalx * normalx + normaly * normaly + normalz * normalz);
    float invarea = (area == 0.f) ? 0.f : 1.f / area;

    prim_cones[i].px = (p0[0] + p1[0] + p2[0]) / 3.f;
    prim_cones[i].py = (p0[1] + p1[1] + p2[1]) / 3.f;
    prim_cones[i].pz = (p0[2] + p1[2] + p2[2]) / 3.f;

    prim_cones[i].nx = normalx * invarea;
    prim_cones[i].ny = normaly * invarea;
    prim_cones[i].nz = normalz * invarea;

    mesh_area += area;
  }

  return mesh_area;
}

static Cone getMeshletCone(const Cone& acc, uint32_t prim_count) {
  Cone result = acc;

  float center_scale = prim_count == 0 ? 0.f : 1.f / float(prim_count);

  result.px *= center_scale;
  result.py *= center_scale;
  result.pz *= center_scale;

  float axis_length = result.nx * result.nx + result.ny * result.ny + result.nz * result.nz;
  float axis_scale = axis_length == 0.f ? 0.f : 1.f / sqrtf(axis_length);

  result.nx *= axis_scale;
  result.ny *= axis_scale;
  result.nz *= axis_scale;

  return result;
}

void MeshletBuilder::buildPrimitiveAdjacency(SceneBuilder::MeshSpec& mesh) {
  size_t prim_count = mesh.getPrimitivesCount();
  
  auto& adjacency = mesh.adjacency;

  auto& adj_counts = adjacency.counts;
  auto& adj_offsets = adjacency.offsets;
  auto& adj_data = adjacency.data; 

  adj_counts.resize(mesh.vertexCount);
  adj_offsets.resize(mesh.vertexCount);
  adj_data.resize(mesh.indexCount);

  // fill prim counts
  memset(adj_counts.data(), 0, mesh.vertexCount * sizeof(uint32_t));

  for (size_t i = 0; i < static_cast<size_t>(mesh.indexCount); ++i) {
    assert(mesh.getIndex(i) < mesh.vertexCount);
    adj_counts[mesh.getIndex(i)]++;
  }

  // fill offset table
  uint32_t offset = 0;

  for (size_t i = 0; i < static_cast<size_t>(mesh.vertexCount); ++i) {
    adj_offsets[i] = offset;
    offset += adj_counts[i];
  }

  assert(offset == mesh.indexCount);

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
}

static unsigned int getNeighborTriangle(const SceneBuilder::MeshSpec& mesh, const SceneBuilder::MeshletSpec& meshletSpec, const Cone* meshlet_cone, const Cone* prim_cones, const uint32_t* live_primitives, const uint8_t* used, float meshlet_expected_radius, float cone_weight, uint32_t* out_extra) {
  uint32_t best_prim = ~0u;
  uint32_t best_extra = 5;
  float best_score = FLT_MAX;

  const auto& adjacency = mesh.adjacency;

  for (size_t i = 0; i < meshletSpec.vertexCount(); ++i) {
    uint32_t index = meshletSpec.vertices[i];

    const uint32_t* neighbors = adjacency.data.data() + adjacency.offsets[index];
    size_t neighbors_size = adjacency.counts[index];

    for (size_t j = 0; j < neighbors_size; ++j) {
      uint32_t prim = neighbors[j];
      uint32_t a = mesh.getIndex(prim * 3 + 0), b = mesh.getIndex(prim * 3 + 1), c = mesh.getIndex(prim * 3 + 2);
      uint32_t extra = (used[a] == 0xff) + (used[b] == 0xff) + (used[c] == 0xff);

      // triangles that don't add new vertices to meshlets are max. priority
      if (extra != 0) {
        // artificially increase the priority of dangling triangles as they're expensive to add to new meshlets
        if (live_primitives[a] == 1 || live_primitives[b] == 1 || live_primitives[c] == 1) {
          extra = 0;
        }

        extra++;
      }

      // since topology-based priority is always more important than the score, we can skip scoring in some cases
      if (extra > best_extra) continue;

      float score = 0;

      // caller selects one of two scoring functions: geometrical (based on meshletSpec cone) or topological (based on remaining triangles)
      if (meshlet_cone) {
        const Cone& tri_cone = prim_cones[prim];

        float distance2 = (tri_cone.px - meshlet_cone->px) * (tri_cone.px - meshlet_cone->px) +
            (tri_cone.py - meshlet_cone->py) * (tri_cone.py - meshlet_cone->py) +
            (tri_cone.pz - meshlet_cone->pz) * (tri_cone.pz - meshlet_cone->pz);

        float spread = tri_cone.nx * meshlet_cone->nx + tri_cone.ny * meshlet_cone->ny + tri_cone.nz * meshlet_cone->nz;

        score = getMeshletScore(distance2, spread, cone_weight, meshlet_expected_radius);
      } else {
        // each live_primitives entry is >= 1 since it includes the current prim we're processing
        score = float(live_primitives[a] + live_primitives[b] + live_primitives[c] - 3);
      }

      // note that topology-based priority is always more important than the score
      // this helps maintain reasonable effectiveness of meshletSpec data and reduces scoring cost
      if (extra < best_extra || score < best_score) {
        best_prim = prim;
        best_extra = extra;
        best_score = score;
      }
    }
  }

  if (out_extra) {
    *out_extra = best_extra;
  }

  return best_prim;
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

static size_t kdtreePartition(uint32_t* kdindices, size_t count, const float* points, size_t stride, uint32_t axis, uint32_t pivot) {
  size_t m = 0;

  // invariant: elements in range [0, m) are < pivot, elements in range [m, i) are >= pivot
  for (size_t i = 0; i < count; ++i) {
    float v = points[kdindices[i] * stride + axis];

    // swap(m, i) unconditionally
    uint32_t t = kdindices[m];
    kdindices[m] = kdindices[i];
    kdindices[i] = t;

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


static size_t kdtreeBuild(size_t offset, std::vector<KDNode>& nodes, size_t node_count, const float* points, size_t stride, uint32_t* kdindices, size_t count, size_t leaf_size) {
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
    const float* point = points + kdindices[i] * stride;

    for (int k = 0; k < 3; ++k) {
      float delta = point[k] - mean[k];
      mean[k] += delta * runs;
      vars[k] += delta * (point[k] - mean[k]);
    }
  }

  // split axis is one where the variance is largest
  uint32_t axis = vars[0] >= vars[1] && vars[0] >= vars[2] ? 0 : vars[1] >= vars[2] ? 1 : 2;

  float split = mean[axis];
  size_t middle = kdtreePartition(kdindices, count, points, stride, axis, split);

  // when the partition is degenerate simply consolidate the points into a single node
  if (middle <= leaf_size / 2 || middle >= count - leaf_size / 2) {
    return kdtreeBuildLeaf(offset, nodes, node_count, kdindices, count);
  }

  KDNode& result = nodes[offset];

  result.split = split;
  result.axis = axis;

  // left subtree is right after our node
  size_t next_offset = kdtreeBuild(offset + 1, nodes, node_count, points, stride, kdindices, middle, leaf_size);

  // distance to the right subtree is represented explicitly
  result.children = static_cast<uint32_t>(next_offset - offset - 1);

  return kdtreeBuild(next_offset, nodes, node_count, points, stride, kdindices + middle, count - middle, leaf_size);
}

static void kdtreeNearest(std::vector<KDNode>& nodes, uint32_t root, const float* points, size_t stride, const uint8_t* emitted_flags, const float* position, uint32_t& result, float& limit) {
  const KDNode& node = nodes[root];

  if (node.axis == 3) {
    // leaf
    for (uint32_t i = 0; i <= node.children; ++i) {
      uint32_t index = nodes[root + i].index;

      if (emitted_flags[index]) {
        continue;
      }

      const float* point = points + index * stride;

      float distance2 =
          (point[0] - position[0]) * (point[0] - position[0]) +
          (point[1] - position[1]) * (point[1] - position[1]) +
          (point[2] - position[2]) * (point[2] - position[2]);
      float distance = sqrtf(distance2);

      if (distance < limit) {
        result = index;
        limit = distance;
      }
    }
  } else {
    // branch; we order recursion to process the node that search position is in first
    float delta = position[node.axis] - node.split;
    uint32_t first = (delta <= 0) ? 0 : node.children;
    uint32_t second = first ^ node.children;

    kdtreeNearest(nodes, root + 1 + first, points, stride, emitted_flags, position, result, limit);

    // only process the other node if it can have a match based on closest distance so far
    if (fabsf(delta) <= limit)
      kdtreeNearest(nodes, root + 1 + second, points, stride, emitted_flags, position, result, limit);
  }
}

static bool appendMeshlet(SceneBuilder::MeshletSpec& meshletSpec, uint32_t a, uint32_t b, uint32_t c, uint8_t* used, std::vector<SceneBuilder::MeshletSpec>& meshletSpecs, size_t max_vertices, size_t max_prims) {
  uint8_t& av = used[a];
  uint8_t& bv = used[b];
  uint8_t& cv = used[c];

  bool result = false;

  uint32_t used_extra = (av == 0xff) + (bv == 0xff) + (cv == 0xff);

  if (((meshletSpec.vertexCount() + used_extra) >= max_vertices) || (meshletSpec.primitiveCount() >= max_prims)) {
    for (size_t j = 0; j < meshletSpec.vertexCount(); ++j) {
      used[meshletSpec.vertices[j]] = 0xff;
    }

    LLOG_WRN << "Finished meshlet vertex_count " << meshletSpec.vertices.size() << " indices " << meshletSpec.indices.size();
    meshletSpecs.push_back(meshletSpec);
    result = true;
  }

  if (av == 0xff) {
    av = (uint8_t)meshletSpec.vertexCount();
    meshletSpec.vertices.push_back(a);
  }

  if (bv == 0xff) {
    bv = (uint8_t)meshletSpec.vertexCount();
    meshletSpec.vertices.push_back(b);
  }

  if (cv == 0xff) {
    cv = (uint8_t)meshletSpec.vertexCount();
    meshletSpec.vertices.push_back(c);
  }

  meshletSpec.indices.push_back(av);
  meshletSpec.indices.push_back(bv);
  meshletSpec.indices.push_back(cv);

  return result;
}

MeshletBuilder::Stats::Stats() {
  mTotalMeshletsBuildCount = 0;
  mTotalMeshletsBuildDuration = std::chrono::milliseconds::zero();
}

void MeshletBuilder::Stats::appendTotalMeshletsBuildCount(const std::vector<SceneBuilder::MeshletSpec>& meshletSpecs) { 
  std::scoped_lock lock(mMutex);
  mTotalMeshletsBuildCount += meshletSpecs.size(); 
}

void MeshletBuilder::Stats::appendTotalMeshletsBuildDuration(const std::chrono::duration<double>& duration) { 
  std::scoped_lock lock(mMutex);
  mTotalMeshletsBuildDuration += duration; 
}

void MeshletBuilder::printStats() const {
  LLOG_INF << "MeshletBuilder stats:";
  LLOG_INF << "\tTotal meshlets count: " << std::to_string(mStats.totalMeshletsBuildCount());
  LLOG_INF << "\tTotal meshlets build time: " << std::chrono::duration_cast<std::chrono::seconds>(mStats.totalMeshletsBuildDuration()).count() << " s"
           << " ( " << std::chrono::duration_cast<std::chrono::milliseconds>(mStats.totalMeshletsBuildDuration()).count() << " ms )";
  LLOG_INF << std::endl;
}

MeshletBuilder::MeshletBuilder() {

};

MeshletBuilder::~MeshletBuilder() {
  printStats();
}

MeshletBuilder::UniquePtr MeshletBuilder::create() {
  return std::move(UniquePtr(new MeshletBuilder()));
}

void MeshletBuilder::generateMeshlets(SceneBuilder::MeshSpec& mesh, BuildMode mode) {
  const std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
  switch(mode) {
    case BuildMode::MESHOPT: 
      generateMeshletsMeshopt(mesh);
      break;
    default:
      generateMeshletsScan(mesh);
      break;
  }
  
  mStats.appendTotalMeshletsBuildCount(mesh.meshletSpecs);
  mStats.appendTotalMeshletsBuildDuration(std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - start_time ));
}

void MeshletBuilder::generateMeshletsScan(SceneBuilder::MeshSpec& mesh) {
  if(mesh.indexCount == 0 || mesh.indexData.empty()) {
    LLOG_WRN << "Meshlets generation for non-indexed mesh \"" << mesh.name << "\" not supported yet !!!";
    return;
  }

  auto& meshletSpecs = mesh.meshletSpecs;
  meshletSpecs.clear();

  if (mesh.use16BitIndices) { assert(mesh.indexCount <= mesh.indexData.size() * 2); }

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

      uint32_t mIdx0 = mesh.getIndex(i), mIdx1 = mesh.getIndex(i+1), mIdx2 = mesh.getIndex(i+2);

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

void MeshletBuilder::generateMeshletsMeshopt(SceneBuilder::MeshSpec& mesh) {
  static const uint32_t max_vertices = MESHLET_MAX_POLYGONS_COUNT * 4u;
  static const uint32_t max_triangles = MESHLET_MAX_POLYGONS_COUNT;
  static const float    cone_weight = .5f;

  assert(cone_weight >= 0.0f && cone_weight <= 1.0f);

  const uint32_t vertex_count = mesh.vertexCount;
  if(vertex_count == 0) return;
  
  buildPrimitiveAdjacency(mesh);

  assert(static_cast<size_t>(vertex_count) == mesh.adjacency.counts.size());
  
  std::vector<uint32_t> live_primitives(vertex_count);
  memcpy(live_primitives.data(), mesh.adjacency.counts.data(), vertex_count * sizeof(uint32_t));

  size_t prim_count = mesh.getPrimitivesCount();

  std::vector<uint8_t> emitted_flags(prim_count);
  memset(emitted_flags.data(), 0, prim_count);

  // For each triangle, precompute centroid & normal to use for scoring. TODO: move to GPU compute task
  std::vector<Cone> prim_cones(prim_count);

  const float mesh_area = computePrimitiveCones(prim_cones, mesh);
  if(mesh_area == 0.0f) {
    LLOG_ERR << "Mesh " << mesh.name << " has zero area. Meshlets generation aborted!!!";
    return;
  }

  // assuming each meshlet is a square patch, expected radius is sqrt(expected area)
  float triangle_area_avg = prim_count == 0 ? 0.f : mesh_area / float(prim_count) * 0.5f;
  float meshlet_expected_radius = sqrtf(triangle_area_avg * max_triangles) * 0.5f;

  // build a kd-tree for nearest neighbor lookup
  std::vector<uint32_t> kdindices(prim_count);
  for (size_t i = 0; i < kdindices.size(); ++i) {
    kdindices[i] = static_cast<uint32_t>(i);
  }

  std::vector<KDNode> nodes(prim_count * 2);
  kdtreeBuild(0, nodes, prim_count * 2, &prim_cones[0].px, sizeof(Cone) / sizeof(float), kdindices.data(), prim_count, /* leaf_size= */ 8);

  // index of the vertex in the meshlet, 0xff if the vertex isn't used
  std::vector<uint8_t> used(vertex_count);
  memset(used.data(), -1, vertex_count);

  SceneBuilder::MeshletSpec meshletSpec = {};
  meshletSpec.primitiveIndices.reserve(MESHLET_MAX_POLYGONS_COUNT);
  meshletSpec.indices.reserve(MESHLET_MAX_POLYGONS_COUNT * 4);
  meshletSpec.vertices.reserve(MESHLET_MAX_VERTICES_COUNT);

  Cone meshlet_cone_acc = {};

  uint32_t cycle_count = 0;

  for (;;) {
    cycle_count++;
    Cone meshlet_cone = getMeshletCone(meshlet_cone_acc, meshletSpec.primitiveCount());

    uint32_t best_extra = 0;
    uint32_t best_prim = getNeighborTriangle(mesh, meshletSpec, &meshlet_cone, prim_cones.data(), live_primitives.data(), used.data(), meshlet_expected_radius, cone_weight, &best_extra);

    // if the best triangle doesn't fit into current meshlet, the spatial scoring we've used is not very meaningful, so we re-select using topological scoring
    if (best_prim != ~0u && (meshletSpec.vertexCount() + best_extra > max_vertices || meshletSpec.primitiveCount() >= max_triangles)) {
      best_prim = getNeighborTriangle(mesh, meshletSpec, NULL, prim_cones.data(), live_primitives.data(), used.data(), meshlet_expected_radius, 0.f, NULL);
    }

    // when we run out of neighboring triangles we need to switch to spatial search; we currently just pick the closest triangle irrespective of connectivity
    if (best_prim == ~0u) {
      float position[3] = {meshlet_cone.px, meshlet_cone.py, meshlet_cone.pz};
      uint32_t index = ~0u;
      float limit = FLT_MAX;

      kdtreeNearest(nodes, 0, &prim_cones[0].px, sizeof(Cone) / sizeof(float), emitted_flags.data(), position, index, limit);
      LLOG_WRN << "Cycle " << cycle_count << " Best prim is -1. Found nearest in kdtree is " << index;

      best_prim = index;
    } else {
      LLOG_WRN << "Cycle " << cycle_count << " Best prim is " << best_prim << ". Found neighbor.";
    }

    if (best_prim == ~0u) {
      LLOG_WRN << "Cycle count " << cycle_count << " . Breaking...";
      break;
    }

    uint32_t a = mesh.getIndex(best_prim * 3 + 0), b = mesh.getIndex(best_prim * 3 + 1), c = mesh.getIndex(best_prim * 3 + 2);
    assert(a < vertex_count && b < vertex_count && c < vertex_count);

    // add meshletSpec to the output; when the current meshletSpec is full we reset the accumulated bounds
    if (appendMeshlet(meshletSpec, a, b, c, used.data(), mesh.meshletSpecs, max_vertices, max_triangles)) {
      LLOG_WRN << "Added meshlet";
      meshletSpec.reset();
      memset(&meshlet_cone_acc, 0, sizeof(meshlet_cone_acc));
      
      //if(cycle_count == 1) break;
    }

    live_primitives[a]--;
    live_primitives[b]--;
    live_primitives[c]--;

    // remove emitted triangle from adjacency data
    // this makes sure that we spend less time traversing these lists on subsequent iterations
    for (size_t k = 0; k < 3; ++k) {
      uint32_t index = mesh.getIndex(best_prim * 3 + k);

      uint32_t* neighbors = mesh.adjacency.data.data() + mesh.adjacency.offsets[index];
      size_t neighbors_size = mesh.adjacency.counts[index];

      for (size_t i = 0; i < neighbors_size; ++i) {
        uint32_t prim = neighbors[i];

        if (prim == best_prim) {
          neighbors[i] = neighbors[neighbors_size - 1];
          mesh.adjacency.counts[index]--;
          break;
        }
      }
    }

    // update aggregated meshletSpec cone data for scoring subsequent prim_cones
    meshlet_cone_acc.px += prim_cones[best_prim].px;
    meshlet_cone_acc.py += prim_cones[best_prim].py;
    meshlet_cone_acc.pz += prim_cones[best_prim].pz;
    meshlet_cone_acc.nx += prim_cones[best_prim].nx;
    meshlet_cone_acc.ny += prim_cones[best_prim].ny;
    meshlet_cone_acc.nz += prim_cones[best_prim].nz;

    emitted_flags[best_prim] = 1;
  }

  if (meshletSpec.primitiveCount()) {
    LLOG_WRN << "Added last meshlet";
    mesh.meshletSpecs.push_back(meshletSpec);
    meshletSpec.reset();
  }

  //assert(meshlet_offset <= meshopt_buildMeshletsBound(index_count, max_vertices, max_triangles));
}

void MeshletBuilder::generateMeshletsGreedy(SceneBuilder::MeshSpec& mesh) {

}

}  // namespace Falcor
