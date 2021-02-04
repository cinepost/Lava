/*
 *  Copyright 2018 Laika, LLC. Authored by Peter Stuart
 *
 *  Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
 *  http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
 *  http://opensource.org/licenses/MIT>, at your option. This file may not be
 *  copied, modified, or distributed except according to those terms.
 */

#include "AlembicRef.h"

#include <cassert>

#include <UT/UT_JSONHandle.h>

#include "Detail.h"
#include "ReadError.h"
#include "util.h"

namespace ika
{
namespace bgeo
{
namespace parser
{

AlembicRef::AlembicRef(const Detail& detail)
    : Primitive(detail)
{
}

AlembicRef* AlembicRef::clone() const
{
    return new AlembicRef(*this);
}

/*virtual*/ void AlembicRef::loadData(UT_JSONParser &parser)
{
    parseBeginArray(parser);
    {
        parseArrayKey(parser, "parameters");
        VertexArrayBuilder vertexBuilder(vertices, sides);
        BGEO_CHECK(parser.parseObject(vertexBuilder));

        closed.resize(1);
        bool closedValue = true;
        parseArrayValueForKey(parser, "closed", closedValue);
        closed[0] = closedValue;
    }
    parseEndArray(parser);
}

/*virtual*/ std::ostream& AlembicRef::encode(std::ostream& co) const
{
    return co;
}

} // namespace parser
} // namespace ika
} // namespace bgeo
