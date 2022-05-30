#ifndef SRC_LAVA_LIB_READER_LSD_SESSION_H_
#define SRC_LAVA_LIB_READER_LSD_SESSION_H_

#include <memory>
#include <variant>
#include <future>
#include <map>
#include <unordered_map>

#include "grammar_lsd.h"
#include "../reader_bgeo/bgeo/Bgeo.h"
#include "../renderer.h"
#include "scope.h"
#include "display.h"

#include "Falcor/Utils/Math/Vector.h"
#include "Falcor/Utils/Timing/Profiler.h"
#include "Falcor/Scene/MaterialX/MaterialX.h"
#include "Falcor/Scene/MaterialX/MxTypes.h"


namespace lava {

namespace lsd {

class Session {
  public:
 	  using UniquePtr = std::unique_ptr<Session>;
    static UniquePtr create(Renderer::SharedPtr pRenderer);    
    ~Session();

    struct DisplayInfo {
      Display::DisplayType                                            displayType;    // __HYDRA__ is a special virtual type
      Display::TypeFormat                                             typeFormat;
      std::vector<std::pair<std::string, std::vector<std::string>>>   displayStringParameters;
      std::vector<std::pair<std::string, std::vector<int>>>           displayIntParameters;
      std::vector<std::pair<std::string, std::vector<float>>>         displayFloatParameters;

      std::string                                                     outputFileName;
    };

    struct CameraInfo {
      double time = 0.0;

      // camera section
      std::string cameraProjectionName = "perspective";
      double      cameraNearPlane = 0.01;
      double      cameraFarPlane  = 1000.0;
      glm::mat4   cameraTransform;
      double      cameraFocalLength = 1.0;
      double      cameraFrameHeight = 1.0;

      Renderer::SamplePattern samplePattern = Renderer::SamplePattern::Stratified;
    };

    struct TileInfo {
      Falcor::uint4   renderRegion;
      Falcor::float4  cameraCropRegion;
    };

  public:
 	  scope::Geo::SharedPtr getCurrentGeo();
    scope::ScopeBase::SharedPtr getCurrentScope();

    /** get string expanded with local env variables
     */
    std::string getExpandedString(const std::string& s);

    bool cmdStart(lsd::ast::Style object_type);
    bool cmdEnd();
    void cmdSetEnv(const std::string& key, const std::string& value);
    bool cmdRaytrace();
    void cmdIPRmode(lsd::ast::IPRMode mode, bool stash);
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

  private:
 	  Session(std::shared_ptr<Renderer> pRenderer);
 	
    void setUpCamera(Falcor::Camera::SharedPtr pCamera, Falcor::float4 cropRegion = {0.0f, 0.0f, 1.0f, 1.0f});
    //bool prepareGlobalData();
    bool prepareFrameData();
 	  bool prepareDisplayData();
 	
    void setEnvVariable(const std::string& key, const std::string& value);

    Falcor::MaterialX::UniquePtr createMaterialXFromLSD(lsd::scope::Material::SharedConstPtr pMaterialLSD);

 	  bool pushGeometryInstance(lsd::scope::Object::SharedConstPtr pObj);
    void addMxNode(Falcor::MxNode::SharedPtr pParent, scope::Node::SharedConstPtr pNodeLSD);

  private:
    bool  mIPR = false;
    ast::IPRMode mIPRmode = ast::IPRMode::GENERATE;

    bool  mFirstRun = true; // This variable used to detect subsequent cmd_raytrace calls for multy-frame and IPR modes 
    Renderer::SharedPtr 	         mpRenderer = nullptr;
    Device::SharedPtr              mpDevice   = nullptr;

    std::map<std::string, std::string>  mEnvmap;

    Renderer::FrameInfo             mCurrentFrameInfo;

    //RendererIface::GlobalData       mGlobalData;
    DisplayInfo		                  mCurrentDisplayInfo;
    CameraInfo                      mCurrentCameraInfo;
    double                          mCurrentTime = 0;
    //RendererIface::FrameData		    mFrameData;

    scope::ScopeBase::SharedPtr		  mpCurrentScope;
    scope::Material::SharedPtr      mpMaterialScope;
    scope::Global::SharedPtr		    mpGlobal;

    Display::SharedPtr              mpDisplay;

    Renderer::Config                mRendererConfig;

    std::unordered_map<std::string, std::variant<uint32_t, std::shared_future<uint32_t>>>	mMeshMap;     // maps detail(mesh) name to SceneBuilder mesh id	or it's async future
    std::unordered_map<std::string, uint32_t> mLightsMap;     // maps detail(mesh) name to SceneBuilder mesh id 
};

static inline std::string to_string(const Session::TileInfo& tileInfo) {
  return "TileInfo: region[" + std::to_string(tileInfo.renderRegion[0]) 
                    + ", " +  std::to_string(tileInfo.renderRegion[1]) 
                    + ", " +  std::to_string(tileInfo.renderRegion[2])
                    + ", " +  std::to_string(tileInfo.renderRegion[3]) + "]"
                    + "[" + std::to_string(tileInfo.cameraCropRegion[0])
                    + ", " +  std::to_string(tileInfo.cameraCropRegion[1])
                    + ", " +  std::to_string(tileInfo.cameraCropRegion[2])
                    + ", " +  std::to_string(tileInfo.cameraCropRegion[3]) + "]";
}

}  // namespace lsd

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_SESSION_H_