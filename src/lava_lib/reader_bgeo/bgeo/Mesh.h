#ifndef BGEO_MESH_H
#define BGEO_MESH_H

#include <vector>
#include <cinttypes>

#include "rtti.h"
#include "Bgeo.h"

namespace ika {
namespace bgeo {

namespace parser {
class Mesh;
}

class Mesh : public Primitive {
    RTTI_DECLARE(Mesh, Primitive)

public:
    // FIXME probably ought to be shared ptr
    Mesh(const Bgeo& bgeo, const parser::Mesh& poly);

    void getRawVertexList(std::vector<int32_t>& vertices) const;
    void getVertexList(std::vector<int32_t>& vertices) const;
    void getStartIndices(std::vector<int32_t>& startIndices) const;
    int32_t getFaceCount() const;

    /*virtual*/ int32_t getVertexCount() const override;

private:
    const Bgeo& m_bgeo;
    const parser::Mesh& m_mesh;

};

} // namespace ika
} // namespace bgeo

#endif // BGEO_MESH_H
