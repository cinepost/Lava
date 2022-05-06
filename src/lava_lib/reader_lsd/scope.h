#ifndef SRC_LAVA_LIB_READER_LSD_SCOPE_H_
#define SRC_LAVA_LIB_READER_LSD_SCOPE_H_

#include <map>
#include <memory>
#include <variant>
#include <string>
#include <vector>

#include "glm/glm/mat4x4.hpp"

#include "Falcor/Scene/MaterialX/MxNode.h"
#include "Falcor/Scene/MaterialX/MxTypes.h"

#include "../scene_reader_base.h"
#include "properties_container.h"
#include "../reader_bgeo/bgeo/Bgeo.h"

namespace lava {

namespace lsd {

namespace scope {

class Instance;
class Material;
class Node;
class Global;
class Light;
class Plane;
class Object;
class Fog;
class Geo;
class Segment;

class Session;

using EmbeddedData = std::vector<unsigned char>;

class ScopeBase: public PropertiesContainer {
  public:
    using SharedConstPtr = std::shared_ptr<const ScopeBase>;
    using SharedPtr = std::shared_ptr<ScopeBase>;

    ScopeBase(SharedPtr pParent);

    SharedPtr parent() { return std::dynamic_pointer_cast<ScopeBase>(mpParent); };
    virtual ~ScopeBase() {};
    virtual ast::Style type() const = 0;
    virtual const void printSummary(std::ostream& os, uint indent = 0) const override;

    EmbeddedData& getEmbeddedData(const std::string& name);

  protected:
    //SharedPtr mpParent;
    std::vector<SharedPtr> mChildren;
    std::unordered_map<std::string, EmbeddedData> mEmbeddedDataMap;
};

class Transformable: public ScopeBase {
  public:
    using SharedConstPtr = std::shared_ptr<const Transformable>;
    using SharedPtr = std::shared_ptr<Transformable>;
    using TransformList = std::vector<glm::mat4x4>;

    Transformable(ScopeBase::SharedPtr pParent);
    virtual ~Transformable() {};

    void setTransform(const lsd::Matrix4& mat);
    void addTransform(const lsd::Matrix4& mat);
    inline TransformList& getTransformList() { return mTransformList; };
    inline const TransformList& getTransformList() const { return mTransformList; };
    inline uint transformSamples() { return mTransformList.size(); };

 private:
    TransformList mTransformList;
};

class Global: public Transformable {
  public:
    using SharedConstPtr = std::shared_ptr<const Global>;
    using SharedPtr = std::shared_ptr<Global>;
    static SharedPtr create();

    inline ast::Style type() const override { return ast::Style::GLOBAL; };

    std::shared_ptr<Geo>        addGeo();
    std::shared_ptr<Object>     addObject();
    std::shared_ptr<Plane>      addPlane();
    std::shared_ptr<Light>      addLight();
    std::shared_ptr<Segment>    addSegment();
    std::shared_ptr<Material>   addMaterial();

    const std::vector<std::shared_ptr<Object>>&     objects() { return mObjects; };
    const std::vector<std::shared_ptr<Plane>>&      planes() { return mPlanes; };
    const std::vector<std::shared_ptr<Light>>&      lights() { return mLights; };
    const std::vector<std::shared_ptr<Geo>>&        geos() { return mGeos; };
    const std::vector<std::shared_ptr<Segment>>&    segments() { return mSegments; };
    const std::vector<std::shared_ptr<Material>>&   materials() { return mMaterials; };

  private:
    Global():Transformable(nullptr) {};
    std::vector<std::shared_ptr<Geo>>       mGeos;
    std::vector<std::shared_ptr<Plane>>     mPlanes;
    std::vector<std::shared_ptr<Object>>    mObjects;
    std::vector<std::shared_ptr<Light>>     mLights;
    std::vector<std::shared_ptr<Segment>>   mSegments;
    std::vector<std::shared_ptr<Material>>  mMaterials;
};

class Geo: public ScopeBase {
 public:
    using SharedConstPtr = std::shared_ptr<const Geo>;
    using SharedPtr = std::shared_ptr<Geo>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    inline ast::Style type() const override { return ast::Style::GEO; };

    void setDetailFilename(const std::string& filename);
    inline void setDetailName(const std::string& name) { mName = name; };

    inline const std::string& detailFilename() { return mFileName; };
    inline const std::string& detailName() { return mName; };
    inline bool isInline() { return mIsInline; };

    ika::bgeo::Bgeo::SharedPtr bgeo();
    ika::bgeo::Bgeo::SharedConstPtr bgeo() const { return mpBgeo; }

 private:
    Geo(ScopeBase::SharedPtr pParent): ScopeBase(pParent), mFileName(""), mIsInline(false) {};

 private:
    std::string     mName = "";
    std::string     mFileName = "";
    ika::bgeo::Bgeo::SharedPtr mpBgeo = nullptr; // lazy initialized bgeo
    bool            mIsInline = false;
};


class Node: public ScopeBase {
  public:
    using SharedConstPtr = std::shared_ptr<const Node>;
    using SharedPtr = std::shared_ptr<Node>;
    
    struct DataSocketTemplate {
      std::string name;
      Falcor::MxSocketDataType  dataType;
      Falcor::MxSocketDirection direction;
    };

    struct EdgeInfo {
      std::string src_node_uuid;
      std::string src_node_output_socket;
      std::string dst_node_uuid;
      std::string dst_node_input_socket;
    };

    static SharedPtr create(ScopeBase::SharedPtr pParent);

    inline ast::Style type() const override { return ast::Style::NODE; };

    std::shared_ptr<Node>       addChildNode();
    void                        addChildEdge(const std::string& src_node_uuid, const std::string& src_node_output_socket, const std::string& dst_node_uuid, const std::string& dst_node_input_socket);
    inline const std::vector<std::shared_ptr<Node>>& childNodes() const { return mChildNodes; };
    inline const std::vector<EdgeInfo>&            childEdges() const { return mChildEdges; };

    inline const std::vector<DataSocketTemplate>& socketTemplates() const { return mSocketTemplates; };

    void addDataSocketTemplate(const std::string& name, Falcor::MxSocketDataType dataType, Falcor::MxSocketDirection direction);

    virtual const void printSummary(std::ostream& os, uint indent = 0) const override;

  protected:
    Node(ScopeBase::SharedPtr pParent): ScopeBase(pParent) {};

  private:
    std::vector<std::shared_ptr<Node>>      mChildNodes;
    std::vector<EdgeInfo>                   mChildEdges;

    std::vector<DataSocketTemplate>         mSocketTemplates;
};

class Material: public Node {
  public:

    using NodeUUID = std::string;

    using SharedConstPtr = std::shared_ptr<const Material>;
    using SharedPtr = std::shared_ptr<Material>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    inline ast::Style type() const override { return ast::Style::MATERIAL; };

    bool insertNode(const NodeUUID& uuid, Falcor::MxNode::SharedPtr pNode);
    Falcor::MxNode::SharedPtr node(const NodeUUID& uuid);

  private:
    Material(ScopeBase::SharedPtr pParent): Node(pParent) {};

    std::map<NodeUUID, Falcor::MxNode::SharedPtr> mNodesMap; // uuid to shading node map
};

class Object: public Transformable {
 public:
    using SharedConstPtr = std::shared_ptr<const Object>;
    using SharedPtr = std::shared_ptr<Object>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    inline ast::Style type() const override { return ast::Style::OBJECT; };

    inline const std::string& geometryName() const { return mGeometryName; };
    inline void setGeometryName(const std::string& name) { mGeometryName = name; };

 private:
    Object(ScopeBase::SharedPtr pParent): Transformable(pParent), mGeometryName() {};

 private:
    std::string mGeometryName;
};

class Plane: public ScopeBase {
 public:
    using SharedConstPtr = std::shared_ptr<const Plane>;
    using SharedPtr = std::shared_ptr<Plane>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    inline ast::Style type() const override { return ast::Style::PLANE; };

 private:
    Plane(ScopeBase::SharedPtr pParent): ScopeBase(pParent) {};
};

class Light: public Transformable {
 public:
    using SharedConstPtr = std::shared_ptr<const Light>;
    using SharedPtr = std::shared_ptr<Light>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    inline ast::Style type() const override { return ast::Style::LIGHT; };

 private:
    Light(ScopeBase::SharedPtr pParent): Transformable(pParent) {};
};

class Segment: public ScopeBase {
 public:
    using SharedConstPtr = std::shared_ptr<const Segment>;
    using SharedPtr = std::shared_ptr<Segment>;
    static SharedPtr create(ScopeBase::SharedPtr pParent);

    inline ast::Style type() const override { return ast::Style::SEGMENT; };

 private:
    Segment(ScopeBase::SharedPtr pParent): ScopeBase(pParent) {};
};


}  // namespace scope

}  // namespace lsd

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_GLOBAL_H_
