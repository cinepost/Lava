#ifndef SRC_LAVA_LIB_READER_LSD_SCOPE_H_
#define SRC_LAVA_LIB_READER_LSD_SCOPE_H_

#include <map>
#include <memory>
#include <variant>
#include <string>
#include <vector>

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

class Global: public ScopeBase {
 public:
    using SharedPtr = std::shared_ptr<Global>;
    static SharedPtr create();

    ast::Style type() override { return ast::Style::GLOBAL; };

    std::shared_ptr<Geo>        addGeo();
    std::shared_ptr<Object>     addObject();
    std::shared_ptr<Plane>      addPlane();
    std::shared_ptr<Light>      addLight();
    std::shared_ptr<Segment>    addSegment();

    const std::vector<std::shared_ptr<Plane>>& planes() { return mPlanes; };

 private:
    Global():ScopeBase(nullptr) {};
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

    ika::bgeo::Bgeo& bgeo() { return mBgeo; }
    const ika::bgeo::Bgeo& bgeo() const { return mBgeo; }

 private:
    Geo(ScopeBase::SharedPtr pParent): ScopeBase(pParent) {};

 private:
    ika::bgeo::Bgeo     mBgeo;
};


class Object: public ScopeBase {
 public:
    using SharedPtr = std::shared_ptr<Object>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    ast::Style type() override { return ast::Style::OBJECT; };

 private:
    Object(ScopeBase::SharedPtr pParent): ScopeBase(pParent) {};
};

class Plane: public ScopeBase {
 public:
    using SharedPtr = std::shared_ptr<Plane>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    ast::Style type() override { return ast::Style::PLANE; };

 private:
    Plane(ScopeBase::SharedPtr pParent): ScopeBase(pParent) {};
};

class Light: public ScopeBase {
 public:
    using SharedPtr = std::shared_ptr<Light>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    ast::Style type() override { return ast::Style::LIGHT; };

 private:
    Light(ScopeBase::SharedPtr pParent): ScopeBase(pParent) {};
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
