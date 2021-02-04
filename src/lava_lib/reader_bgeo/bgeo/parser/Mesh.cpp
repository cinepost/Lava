/*
 *  Copyright 2018 Laika, LLC. Authored by Peter Stuart
 *
 *  Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
 *  http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
 *  http://opensource.org/licenses/MIT>, at your option. This file may not be
 *  copied, modified, or distributed except according to those terms.
 */

#include "Mesh.h"

#include <cassert>
#include <iostream>

#include <UT/UT_JSONHandle.h>

#include "Detail.h"
#include "ReadError.h"
#include "util.h"

namespace ika {
namespace bgeo {
namespace parser {

namespace {

class UniformDataHandle : public UT_JSONHandleError {
 public:
    UniformDataHandle(Mesh& mesh): mesh(mesh) { }

    /*virtual*/ bool jsonKey(UT_JSONParser& parser, const char *v, int64 len) {
        UT_String key(v);
        if (key == "surface") {
            //UT_String surfaceType;

            UT_WorkBuffer buffer;
            //BGEO_CHECK(parser.parseString(buffer));
            //value.harden(buffer.buffer());

            BGEO_CHECK(parser.parseString(buffer));
            if(buffer == "quads") {
                mesh.mSurfaceType = Mesh::SurfaceType::QUADS;
            }
            return true;
        }

        if (key == "uwrap") {
            BGEO_CHECK(parser.parseBool(mesh.mUwrap));
            return true;
        }

        if (key == "vwrap") {
            BGEO_CHECK(parser.parseBool(mesh.mVwrap));
            return true;
        }


        return false;
    }

    /*virtual*/ bool jsonBeginMap(UT_JSONParser& parser) {
        return true;
    }

    /*virtual*/ bool jsonEndMap(UT_JSONParser& parser) {
        return true;
    }

private:
    Mesh& mesh;
};

} // anonymous namespace

Mesh::Mesh(const Detail& detail): Primitive(detail), mSurfaceType(Mesh::SurfaceType::UNKNOWN) {
}

Mesh* Mesh::clone() const {
    return new Mesh(*this);
}

/*virtual*/ void Mesh::loadData(UT_JSONParser &parser) {
    std::cout << "Mesh::loadData\n";
    parseBeginArray(parser);
    {
        /*
        parseArrayKey(parser, "vertex");
        VertexArrayBuilder vertexBuilder(vertices, sides);
        BGEO_CHECK(parser.parseObject(vertexBuilder));

        closed.resize(1);
        bool closedValue = true;
        parseArrayValueForKey(parser, "closed", closedValue);
        closed[0] = closedValue;
        */
    }
    parseEndArray(parser);
    std::cout << "Mesh::loadData done\n";
}

/*virtual*/ void Mesh::loadVaryingData(UT_JSONParser& parser, const StringList& fields) {
    std::cout << "Mesh::loadVaryingData\n";
    // NOTE: for now just support only the vertex field
    if (fields.size() != 1 || fields[0] != "vertex") {
        std::cout << "ReadError!!! Mesh primitive supports only varying vertex\n";
        throw ReadError("Mesh primitive supports only varying vertex");
    }

    VertexArrayBuilder builder(vertices, sides);
    for (auto it = parser.beginArray(); !it.atEnd(); ++it) {
        BGEO_CHECK(parser.parseObject(builder));
    }
    std::cout << "Mesh::loadVaryingData done\n";
}

/*virtual*/ void Mesh::loadUniformData(UT_JSONParser& parser) {
    std::cout << "Mesh::loadUniformData\n";
    UniformDataHandle uniformHandle(*this);
    BGEO_CHECK(parser.parseObject(uniformHandle));
    std::cout << "Mesh::loadUniformData done\n";
}

/*virtual*/ std::ostream& Mesh::encode(std::ostream& co) const {

    return co;
}

void Mesh::getVerticesMappedToPoints(VertexArrayBuilder::VertexArray& vertexPoints) const
{
    detail.mapVerticesToPoints(vertices, vertexPoints);
}

} // namespace parser
} // namespace ika
} // namespace bgeo
