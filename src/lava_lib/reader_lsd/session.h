#ifndef SRC_LAVA_LIB_READER_LSD_SESSION_H_
#define SRC_LAVA_LIB_READER_LSD_SESSION_H_

#include <memory>
#include <variant>
#include <future>
#include <unordered_map>

#include "grammar_lsd.h"
#include "../reader_bgeo/bgeo/Bgeo.h"
#include "../renderer_iface.h"
#include "scope.h"

#include "Falcor/Scene/MaterialX/MaterialX.h"
#include "Falcor/Scene/MaterialX/MxTypes.h"

//#include "../scene_builder.h" 

namespace lava {

namespace lsd {

class Session {
  public:
 	  using UniquePtr = std::unique_ptr<Session>;
   static UniquePtr create(std::unique_ptr<RendererIface> pRendererIface);

    
    ~Session();

  public:
 	  scope::Geo::SharedPtr getCurrentGeo();
    scope::ScopeBase::SharedPtr getCurrentScope();

    bool cmdStart(lsd::ast::Style object_type);
    bool cmdEnd();
    void cmdSetEnv(const std::string& key, const std::string& value);
    bool cmdRaytrace();
    void cmdIPRmode(const std::string& mode);
    void cmdEdge(const std::string& src_node_uuid, const std::string& src_node_output_socket, const std::string& dst_node_uuid, const std::string& dst_node_input_socket);
    void cmdConfig(lsd::ast::Type type, const std::string& name, const lsd::PropValue& value);
    void cmdProperty(lsd::ast::Style style, const std::string& token, const Property::Value& value);
    void cmdPropertyV(lsd::ast::Style style, const std::vector<std::pair<std::string, Property::Value>>& values);
    void cmdDeclare(lsd::ast::Style style, lsd::ast::Type type, const std::string& token, const lsd::PropValue& value);
    void cmdImage(lsd::ast::DisplayType display_type, const std::string& filename);
    void cmdTransform(const Matrix4& transform);
    bool cmdSocket(Falcor::MxSocketDirection direction, Falcor::MxSocketDataType dataType, const std::string& name);
    void cmdMTransform(const Matrix4& transform);
    bool cmdGeometry(const std::string& name);
    void cmdTime(double time);
    void cmdQuit();

    void pushLight(const scope::Light::SharedPtr pLight);
    void pushBgeo(const std::string& name, ika::bgeo::Bgeo::SharedConstPtr pBgeo, bool async = false);
    std::string getExpandedString(const std::string& str);

  private:
 	  Session(std::unique_ptr<RendererIface> pRendererIface);
 	
    bool prepareGlobalData();
    bool prepareFrameData();
 	  bool prepareDisplayData();
 	
    Falcor::MaterialX::UniquePtr createMaterialXFromLSD(lsd::scope::Material::SharedConstPtr pMaterialLSD);

 	  bool pushGeometryInstance(lsd::scope::Object::SharedConstPtr pObj);
    void addMxNode(Falcor::MxNode::SharedPtr pParent, scope::Node::SharedConstPtr pNodeLSD);

  private:
    bool  mIPRmode = false;
    bool  mFirstRun = true; // This variable used to detect subsequent cmd_raytrace calls for multy-frame and IPR modes 
    std::unique_ptr<RendererIface> 	mpRendererIface;

    RendererIface::GlobalData       mGlobalData;
    RendererIface::DisplayData		  mDisplayData;
    RendererIface::FrameData		    mFrameData;

    scope::ScopeBase::SharedPtr		  mpCurrentScope;
    scope::Material::SharedPtr      mpMaterialScope;
    scope::Global::SharedPtr		    mpGlobal;

    std::unordered_map<std::string, std::variant<uint32_t, std::shared_future<uint32_t>>>	mMeshMap;     // maps detail(mesh) name to SceneBuilder mesh id	or it's async future
    std::unordered_map<std::string, uint32_t> mLightsMap;     // maps detail(mesh) name to SceneBuilder mesh id 
};

}  // namespace lsd

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_SESSION_H_