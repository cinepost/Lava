/*
 *  Copyright 2018 Laika, LLC. Authored by Peter Stuart
 *
 *  Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
 *  http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
 *  http://opensource.org/licenses/MIT>, at your option. This file may not be
 *  copied, modified, or distributed except according to those terms.
 */

#ifndef BGEO_BGEO_H
#define BGEO_BGEO_H

#include <memory>
#include <cinttypes>
#include <vector>

#include "Primitive.h"
#include "Attribute.h"

#include "Falcor/Utils/Math/Vector.h"

namespace ika {
namespace bgeo {

namespace parser {
class Detail;
}

class Bgeo: public std::enable_shared_from_this<Bgeo> {
 public:
    using SharedPtr = std::shared_ptr<Bgeo>;
    using SharedConstPtr = std::shared_ptr<const Bgeo>;
    using UniquePtr = std::unique_ptr<Bgeo>;
    using UniqueConstPtr = std::unique_ptr<const Bgeo>;

    //static UniquePtr createUnique(std::istream& in, bool checkVersion = false);
    static UniquePtr createUnique(const std::string& bgeoString, bool checkVersion = false);
    static UniquePtr createUnique(const char* bgeoPath, bool checkVersion = true);
    static UniquePtr createUnique(std::shared_ptr<parser::Detail> detail);

    //static SharedPtr create(std::istream& in, bool checkVersion = false);
    static SharedPtr create();
    static SharedPtr create(const std::string& bgeoString, bool checkVersion = false);
    static SharedPtr create(const char* bgeoPath, bool checkVersion = true);
    static SharedPtr create(std::shared_ptr<parser::Detail> detail);

    ~Bgeo(); // dtor required for unique_ptr

    void readInlineGeo(const std::string& bgeoString, bool checkVersion = false);
    void readGeoFromFile(const char* bgeoPath, bool checkVersion = false);

    int64_t getPointCount() const;
    int64_t getTotalVertexCount() const;
    int64_t getPrimitiveCount() const;

    void getBoundingBox(double bound[6]) const;

    // print //////

    void printSummary(std::ostream& co) const;

    // detail //////
    std::shared_ptr<parser::Detail> getDetail() const;

    // attributes ///////

    void getP(std::vector<float>& P) const;
    void getP(std::vector<Falcor::float3>& P) const;
    
    void getPointN(std::vector<float>& N) const;
    void getPointN(std::vector<Falcor::float3>& N) const;
    
    void getPointUV(std::vector<float>& uv) const;
    void getPointUV(std::vector<Falcor::float2>& uv) const;

    void getVertexN(std::vector<float>& N) const;
    void getVertexN(std::vector<Falcor::float3>& N) const;

    void getVertexUV(std::vector<float>& uv) const;
    void getVertexUV(std::vector<Falcor::float2>& uv) const;

    int64_t getPointAttributeCount() const;
    typedef std::shared_ptr<Attribute> AttributePtr;
    AttributePtr getPointAttribute(int64_t index) const;
    AttributePtr getPointAttributeByName(const char* name) const;

    int64_t getVertexAttributeCount() const;
    AttributePtr getVertexAttribute(int64_t index) const;
    AttributePtr getVertexAttributeByName(const char* name) const;

    int64_t getPrimitiveAttributeCount() const;
    AttributePtr getPrimitiveAttribute(int64_t index) const;
    AttributePtr getPrimitiveAttributeByName(const char* name) const;

    int64_t getDetailAttributeCount() const;
    AttributePtr getDetailAttribute(int64_t index) const;
    AttributePtr getDetailAttributeByName(const char* name) const;

    // primitives /////

    typedef std::shared_ptr<Primitive> PrimitivePtr;
    PrimitivePtr getPrimitive(int64_t index) const;
    void preCachePrimitives();

    // primitive groups //////

    int64_t getPrimitiveGroupCount() const;

    std::string getPrimitiveGroupName(int64_t index) const;
    void getPrimitiveGroup(int64_t index, std::vector<int32_t>& indices) const;

 private:
    Bgeo();
    explicit Bgeo(std::istream& in, bool checkVersion = false);
    explicit Bgeo(const std::string& bgeoString, bool checkVersion = false);
    explicit Bgeo(const char* bgeoPath, bool checkVersion = true);
    explicit Bgeo(std::shared_ptr<parser::Detail> detail);
    explicit Bgeo(Bgeo&& b) noexcept;

    // use pimpl here to avoid include UT classes
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
    
    mutable std::vector<PrimitivePtr> m_primitiveCache;
};

} // namespace ika
} // namespace bgeo

#endif // BGEO_BGEO_H
