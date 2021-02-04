/*
 *  Copyright 2018 Laika, LLC. Authored by Peter Stuart
 *
 *  Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
 *  http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
 *  http://opensource.org/licenses/MIT>, at your option. This file may not be
 *  copied, modified, or distributed except according to those terms.
 */

#ifndef BGEO_PARSER_ALEMBIC_REF_H
#define BGEO_PARSER_ALEMBIC_REF_H

#include <vector>

#include "Primitive.h"
#include "VertexArrayBuilder.h"

namespace ika
{
namespace bgeo
{
namespace parser
{

class AlembicRef : public Primitive
{
public:
    AlembicRef(const Detail& detail);
    AlembicRef(const AlembicRef& poly) = default;

    /*virtual*/ AlembicRef* clone() const;

    /*virtual*/ PrimType getType() const
    {
        return AlembicRefType;
    }

    /*virtual*/ void loadData(UT_JSONParser &parser);

    /*virtual*/ std::ostream& encode(std::ostream& co) const;

    VertexArrayBuilder::VertexArray vertices;
    VertexArrayBuilder::VertexArray sides;

    typedef std::vector<bool> ClosedArray;
    ClosedArray closed;
};

} // namespace parser
} // namespace ika
} // namespace bgeo

#endif // BGEO_PARSER_ALEMBIC_REF_H
