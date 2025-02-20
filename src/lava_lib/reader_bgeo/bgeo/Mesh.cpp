#include "Mesh.h"

#include "parser/Mesh.h"

namespace ika {
namespace bgeo {

RTTI_DEFINE(Mesh, Primitive, PrimType::MeshPrimType)

Mesh::Mesh(const Bgeo& bgeo, const parser::Mesh& mesh): m_bgeo(bgeo), m_mesh(mesh) { }

void Mesh::getRawVertexList(std::vector<int32_t>& vertices) const {
    vertices = m_mesh.vertices;
}

void Mesh::getVertexList(std::vector<int32_t>& vertices) const {
    m_mesh.getVerticesMappedToPoints(vertices);
}

void Mesh::getStartIndices(std::vector<uint32_t>& startIndices) const {
    startIndices.resize(m_mesh.sides.size() + 1);
    startIndices[0] = 0;

    int64 current = 0;
    for (size_t i = 0; i < m_mesh.sides.size(); ++i) {
        current += m_mesh.sides[i];
        startIndices[i + 1] = current;
    }
}

uint32_t Mesh::getFaceCount() const {
    return static_cast<uint32_t>(m_mesh.sides.size());
}

uint32_t Mesh::getVertexCount() const {
    return static_cast<uint32_t>(m_mesh.vertices.size());
}

} // namespace ika
} // namespace bgeo
