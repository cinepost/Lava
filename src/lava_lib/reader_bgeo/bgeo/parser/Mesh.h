#ifndef BGEO_PARSER_MESH_H
#define BGEO_PARSER_MESH_H

#include <vector>

#include "Primitive.h"
#include "VertexArrayBuilder.h"

namespace ika {
namespace bgeo {
namespace parser {

class Mesh : public Primitive {
 public:
    enum SurfaceType {
        UNKNOWN = 0,
        TRIS,
        QUADS
    };

    Mesh(const Detail& detail);
    Mesh(const Mesh& poly) = default;

    /*virtual*/ Mesh* clone() const;

    /*virtual*/ PrimType getType() const {
        return MeshType;
    }

    /*virtual*/ RunMode getRunMode() const {
        return MeshRunMode;
    }

    /*virtual*/ void loadData(UT_JSONParser &parser);

    /*virtual*/ void loadVaryingData(UT_JSONParser& parser, const StringList& fields);
    /*virtual*/ void loadUniformData(UT_JSONParser& parser);

    /*virtual*/ std::ostream& encode(std::ostream& co) const;

    VertexArrayBuilder::VertexArray vertices;
    VertexArrayBuilder::VertexArray sides;

    SurfaceType mSurfaceType;
    bool mUwrap, mVwrap;

    void getVerticesMappedToPoints(VertexArrayBuilder::VertexArray& vertexPoints) const;
};

} // namespace parser
} // namespace ika
} // namespace bgeo

#endif // BGEO_PARSER_MESH_H
