#include "Geometry.h"


namespace Falcor {

namespace Geometry {

MeshSpec::MeshSpec(const MeshSpec& spec) {
    name = spec.name;
    topology = spec.topology;
    materialId = materialId;
    staticVertexOffset = spec.staticVertexOffset;
    staticVertexCount = spec.staticVertexCount;
    perPrimMaterialIndicesOffset = spec.perPrimMaterialIndicesOffset;
    perPrimMaterialIndicesCount  = spec.perPrimMaterialIndicesCount;
    skinningVertexOffset = spec.skinningVertexOffset;
    skinningVertexCount = spec.skinningVertexCount;
    prevVertexOffset = spec.prevVertexOffset;
    prevVertexCount = spec.prevVertexCount;
    indexOffset = spec.indexOffset;
    indexCount = spec.indexCount;
    vertexCount = spec.vertexCount;
    skeletonNodeID = spec.skeletonNodeID;
    use16BitIndices = spec.use16BitIndices;
    hasSkinningData = spec.hasSkinningData;
    isStatic = spec.isStatic;
    isFrontFaceCW = spec.isFrontFaceCW;
    isDisplaced = spec.isDisplaced;
    isAnimated = spec.isAnimated;
    boundingBox = spec.boundingBox;
    instances = spec.instances;
    subdivDataOffset = spec.subdivDataOffset;
    adjacencyDataOffset = spec.adjacencyDataOffset;
    pointIndexData = spec.pointIndexData;
    indexData = spec.indexData;
    staticData = spec.staticData;
    skinningData = spec.skinningData;
    perPrimitiveMaterialIDsData = spec.perPrimitiveMaterialIDsData;

    meshletSpecs = spec.meshletSpecs;

    adjacencyData = spec.adjacencyData;
}

MeshSpec& MeshSpec::operator=(const MeshSpec& o) {
    if (this != &o) {
        std::lock(mMutex, o.mMutex);
        std::lock_guard<std::mutex> lhs_lk(mMutex, std::adopt_lock);
        std::lock_guard<std::mutex> rhs_lk(o.mMutex, std::adopt_lock);

        name = o.name;
        topology = o.topology;
        materialId = materialId;
        staticVertexOffset = o.staticVertexOffset;
        staticVertexCount = o.staticVertexCount;
        perPrimMaterialIndicesOffset = o.perPrimMaterialIndicesOffset;
        perPrimMaterialIndicesCount  = o.perPrimMaterialIndicesCount;
        skinningVertexOffset = o.skinningVertexOffset;
        skinningVertexCount = o.skinningVertexCount;
        prevVertexOffset = o.prevVertexOffset;
        prevVertexCount = o.prevVertexCount;
        indexOffset = o.indexOffset;
        indexCount = o.indexCount;
        vertexCount = o.vertexCount;
        skeletonNodeID = o.skeletonNodeID;
        use16BitIndices = o.use16BitIndices;
        hasSkinningData = o.hasSkinningData;
        isStatic = o.isStatic;
        isFrontFaceCW = o.isFrontFaceCW;
        isDisplaced = o.isDisplaced;
        isAnimated = o.isAnimated;
        boundingBox = o.boundingBox;
        instances = o.instances;

        subdivDataOffset = o.subdivDataOffset;
        adjacencyDataOffset = o.adjacencyDataOffset;
        pointIndexData = o.pointIndexData;

        indexData = o.indexData;
        staticData = o.staticData;
        skinningData = o.skinningData;
        perPrimitiveMaterialIDsData = o.perPrimitiveMaterialIDsData;

        meshletSpecs = o.meshletSpecs;

        adjacencyData = o.adjacencyData;
    }
    return *this;
}

}  // namespace Geometry

}  // namespace Falcor
