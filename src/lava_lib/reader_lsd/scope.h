#ifndef SRC_LAVA_LIB_READER_LSD_SCOPE_H_
#define SRC_LAVA_LIB_READER_LSD_SCOPE_H_

#include <map>
#include <memory>
#include <variant>
#include <string>
#include <vector>

#include "Externals/GLM/glm/mat4x4.hpp"

#include "../scene_reader_base.h"
#include "properties_container.h"
#include "../reader_bgeo/bgeo/Bgeo.h"

namespace lava {

namespace lsd {

namespace scope {

class Instance;
class Matreial;
class Global;
class Light;
class Plane;
class Object;
class Fog;
class Geo;
class Segment;

class ScopeBase: public PropertiesContainer, public std::enable_shared_from_this<ScopeBase> {
 public:
    using SharedPtr = std::shared_ptr<ScopeBase>;

    ScopeBase(SharedPtr pParent);
    virtual ~ScopeBase() {};

    virtual ast::Style type() = 0;

    SharedPtr parent() { return mpParent; };

    void printSummary(std::ostream& os, uint indent = 0);

 protected:
    SharedPtr mpParent;
    std::vector<SharedPtr> mChildren;
};

class Transformable: public ScopeBase {
 public:
    using SharedPtr = std::shared_ptr<Transformable>;

    Transformable(ScopeBase::SharedPtr pParent);
    virtual ~Transformable() {};

    void setTransform(const lsd::Matrix4& mat);
    const glm::mat4x4& getTransform(){ return mTransform; };

 private:
    glm::mat4x4 mTransform;
};

class Global: public Transformable {
 public:
    using SharedPtr = std::shared_ptr<Global>;
    static SharedPtr create();

    ast::Style type() override { return ast::Style::GLOBAL; };

    std::shared_ptr<Geo>        addGeo();
    std::shared_ptr<Object>     addObject();
    std::shared_ptr<Plane>      addPlane();
    std::shared_ptr<Light>      addLight();
    std::shared_ptr<Segment>    addSegment();

    const std::vector<std::shared_ptr<Plane>>&  planes() { return mPlanes; };
    const std::vector<std::shared_ptr<Geo>>&    geos() { return mGeos; };

 private:
    Global():Transformable(nullptr) {};
    std::vector<std::shared_ptr<Geo>>       mGeos;
    std::vector<std::shared_ptr<Plane>>     mPlanes;
    std::vector<std::shared_ptr<Object>>    mObjects;
    std::vector<std::shared_ptr<Light>>     mLights;
    std::vector<std::shared_ptr<Segment>>   mSegments;
};

class Geo: public ScopeBase {
 public:
    using SharedPtr = std::shared_ptr<Geo>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    ast::Style type() override { return ast::Style::GEO; };

    void setDetailFilename(const std::string& filename);
    void setDetailName(const std::string& name) { mName = name; };

    const std::string& detailFilename() { return mFileName; };
    const std::string& detailName() { return mName; };
    bool isInline() { return mIsInline; };

    ika::bgeo::Bgeo& bgeo() { return mBgeo; }
    const ika::bgeo::Bgeo& bgeo() const { return mBgeo; }

 private:
    Geo(ScopeBase::SharedPtr pParent): ScopeBase(pParent), mFileName(""), mIsInline(false) {};

 private:
    std::string     mName = "";
    std::string     mFileName = "";
    ika::bgeo::Bgeo mBgeo;
    bool            mIsInline = false;
};


class Object: public Transformable {
 public:
    using SharedPtr = std::shared_ptr<Object>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    ast::Style type() override { return ast::Style::OBJECT; };

    const std::string& geometryName() const { return mGeometryName; };
    void setGeometryName(const std::string& name) { mGeometryName = name; };

 private:
    Object(ScopeBase::SharedPtr pParent): Transformable(pParent), mGeometryName() {};

 private:
    std::string mGeometryName;
};

class Plane: public ScopeBase {
 public:
    using SharedPtr = std::shared_ptr<Plane>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    ast::Style type() override { return ast::Style::PLANE; };

 private:
    Plane(ScopeBase::SharedPtr pParent): ScopeBase(pParent) {};
};

class Light: public Transformable {
 public:
    using SharedPtr = std::shared_ptr<Light>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    ast::Style type() override { return ast::Style::LIGHT; };

 private:
    Light(ScopeBase::SharedPtr pParent): Transformable(pParent) {};
};

class Segment: public ScopeBase {
 public:
    using SharedPtr = std::shared_ptr<Segment>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    ast::Style type() override { return ast::Style::SEGMENT; };

 private:
    Segment(ScopeBase::SharedPtr pParent): ScopeBase(pParent) {};
};

}  // namespace scope

}  // namespace lsd

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_GLOBAL_H_
