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
    void cmdConfig(lsd::ast::Type type, const std::string& name, const lsd::PropValue& value);
    void cmdProperty(lsd::ast::Style style, const std::string& token, const Property::Value& value);
    void cmdPropertyV(lsd::ast::Style style, const std::vector<std::pair<std::string, Property::Value>>& values);
    void cmdDeclare(lsd::ast::Style style, lsd::ast::Type type, const std::string& token, const lsd::PropValue& value);
    void cmdImage(lsd::ast::DisplayType display_type, const std::string& filename);
    void cmdTransform(const Matrix4& transform);
    void cmdMTransform(const Matrix4& transform);
    bool cmdGeometry(const std::string& name);
    void cmdTime(double time);

    void pushLight(const scope::Light::SharedPtr pLight);
    void pushBgeo(const std::string& name, ika::bgeo::Bgeo::SharedConstPtr pBgeo, bool async = false);
    std::string getExpandedString(const std::string& str);

 private:
 	Session(std::unique_ptr<RendererIface> pRendererIface);
 	
 	bool prepareDisplayData();
 	bool prepareFrameData();

 	bool pushGeometryInstance(const scope::Object::SharedPtr pObj);

 private:
    bool                            mIPRmode = false;
 	bool 							mFirstRun = true; // This variable used to detect subsequent cmd_raytrace calls for multy-frame and IPR modes 
 	std::unique_ptr<RendererIface> 	mpRendererIface;
 	
 	RendererIface::DisplayData		mDisplayData;
 	RendererIface::FrameData		mFrameData;

 	scope::ScopeBase::SharedPtr		mpCurrentScope;
 	scope::Global::SharedPtr		mpGlobal;

 	std::unordered_map<std::string, std::variant<uint32_t, std::shared_future<uint32_t>>>	mMeshMap;     // maps detail(mesh) name to SceneBuilder mesh id	or it's async future
    std::unordered_map<std::string, uint32_t> mLightsMap;     // maps detail(mesh) name to SceneBuilder mesh id 
};

}  // namespace lsd

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_SESSION_H_