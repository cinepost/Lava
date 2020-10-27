#include <utility>

#include "Falcor/Scene/Lights/Light.h"

#include "session.h"

#include "../display.h"
#include "../scene_builder.h" 

#include "lava_utils_lib/ut_fsys.h"
#include "lava_utils_lib/ut_string.h"
#include "lava_utils_lib/logging.h"


namespace lava {

namespace lsd {

using DisplayType = Display::DisplayType;

DisplayType resolveDisplayTypeByFileName(const std::string& file_name) {
	std::string ext = ut::fsys::getFileExtension(file_name);

    if( ext == ".exr" ) return DisplayType::OPENEXR;
    if( ext == ".jpg" ) return DisplayType::JPEG;
    if( ext == ".jpeg" ) return DisplayType::JPEG;
    if( ext == ".png" ) return DisplayType::PNG;
    if( ext == ".tif" ) return DisplayType::TIFF;
    if( ext == ".tiff" ) return DisplayType::TIFF;
    return DisplayType::OPENEXR;
}

Session::UniquePtr Session::create(std::unique_ptr<RendererIface> pRendererIface) {
	auto pSession = Session::UniquePtr(new Session(std::move(pRendererIface)));
	auto pGlobal = scope::Global::create();
	if (!pGlobal) {
		return nullptr;
	}

	pSession->mpGlobal = pGlobal;
	pSession->mpCurrentScope = pGlobal;

	return std::move(pSession);
}

Session::Session(std::unique_ptr<RendererIface> pRendererIface):mFirstRun(true) { 
	mpRendererIface = std::move(pRendererIface);
}

Session::~Session() { }

void Session::cmdSetEnv(const std::string& key, const std::string& value) {
	mpRendererIface->setEnvVariable(key, value);
}

void Session::cmdConfig(const std::string& file_name) {
	// actual render graph configs loading postponed unitl renderer is initialized
	mpRendererIface->loadDeferredScriptFile(file_name);
}

std::string Session::getExpandedString(const std::string& str) {
	return mpRendererIface->getExpandedString(str);
}

void Session::cmdImage(lsd::ast::DisplayType display_type, const std::string& filename) {
	LLOG_DBG << "cmdImage";
	mFrameData.imageFileName = filename;
    mDisplayData.displayType = display_type;
}

// initialize frame independet render data
bool Session::prepareDisplayData() {
	LLOG_DBG << "prepareDisplayData";

	// prepare display driver parameters
	auto& props_container = mpGlobal->filterProperties(ast::Style::PLANE, std::regex("^IPlay\\.[a-zA-Z]*"));
	for( auto const& item: props_container.properties()) {
		LLOG_DBG << "Display property: " << to_string(item.first);

		const std::string& parm_name = item.first.second.substr(6); // remove leading "IPlay."
		const Property& prop = item.second;
		switch(item.second.type()) {
			case ast::Type::FLOAT:
				//LLOG_DBG << "type: " << to_string(prop.type()) << " value: " << to_string(prop.value());
				mDisplayData.displayFloatParameters.push_back(std::pair<std::string, std::vector<float>>( parm_name, {prop.get<float>()} ));
				break;
			case ast::Type::INT:
				mDisplayData.displayIntParameters.push_back(std::pair<std::string, std::vector<int>>( parm_name, {prop.get<int>()} ));
				break;
			case ast::Type::STRING:
				mDisplayData.displayStringParameters.push_back(std::pair<std::string, std::vector<std::string>>( parm_name, {prop.get<std::string>()} ));
				break;
			default:
				break;
		}
	}

	return true;
}

// initialize frame dependet render data
bool Session::prepareFrameData() {
	LLOG_DBG << "prepareFrameData";
	if(!mpRendererIface->initRenderer()) return false;

	// set up frame resolution (as they don't have to be the same size)
	Int2 resolution = mpGlobal->getPropertyValue(ast::Style::IMAGE, "resolution", Int2{640, 480});
	mFrameData.imageWidth = resolution[0];
	mFrameData.imageHeight = resolution[1];

	// set up camera data
	Vector2 camera_clip = mpGlobal->getPropertyValue(ast::Style::CAMERA, "clip", Vector2{0.01, 1000.0});
	
	mFrameData.cameraNearPlane = camera_clip[0];
	mFrameData.cameraFarPlane  = camera_clip[1];
	mFrameData.cameraProjectionName = mpGlobal->getPropertyValue(ast::Style::CAMERA, "projection", std::string("perspective"));
	mFrameData.cameraTransform = mpGlobal->getTransformList()[0];

	const auto& segments = mpGlobal->segments();
	if(segments.size()) {
		const auto& pSegment = segments[0];
		mFrameData.cameraFocalLength = 50.0 * pSegment->getPropertyValue(ast::Style::CAMERA, "zoom", (double)1.0);
		
		double height_k = static_cast<double>(mFrameData.imageHeight) / static_cast<double>(mFrameData.imageWidth);
		mFrameData.cameraFrameHeight = height_k * 50.0;
	}

	mFrameData.imageSamples = mpGlobal->getPropertyValue(ast::Style::IMAGE, "samples", 1);

	return true;
}


bool Session::cmdRaytrace() {
	mpGlobal->printSummary(std::cout);
	LLOG_DBG << "cmdRaytrace";
	
	// push frame independent data to the rendering interface
	if(mFirstRun) {

		// prepare display driver parameters
		if(!prepareDisplayData()) {
			LLOG_ERR << "Unable to prepare display data !!!";
			return false;
		}

		if(!mpRendererIface->setDisplay(mDisplayData)) {
			LLOG_ERR << "Error setting display data !!!";
			return false;
		}
	}

	if(!prepareFrameData()) {
		LLOG_ERR << "Unable to prepare frame data !";
		return false;
	}

	mpRendererIface->renderFrame(mFrameData);

	mFirstRun = false;
	return true;
}

void Session::pushBgeo(const std::string& name, ika::bgeo::Bgeo& bgeo) {
	LLOG_DBG << "pushBgeo";
    bgeo.printSummary(std::cout);

    auto pSceneBuilder = mpRendererIface->getSceneBuilder();
    if(pSceneBuilder) {
    	uint32_t mesh_id = pSceneBuilder->addMesh(std::move(bgeo));
    	mMeshMap[name] = mesh_id;
    } else {
    	LLOG_ERR << "Can't push geometry (bgeo). SceneBuilder not ready !!!";
    }
}

void Session::pushLight(const scope::Light::SharedPtr pLight) {

	static std::string unnamed = "unnamed"; // safety. in case light scope has no name specified

    auto pSceneBuilder = mpRendererIface->getSceneBuilder();
    if(pSceneBuilder) {
    	std::string light_name = pLight->getPropertyValue(ast::Style::OBJECT, "name", unnamed);

    	const auto& transform = pLight->getTransformList()[0];

    	auto pFalcorLight = Falcor::PointLight::create();
    	pFalcorLight->setWorldPosition({transform[3][0], transform[3][1], transform[3][2]});
    	uint32_t light_id = pSceneBuilder->addLight(pFalcorLight);
    	mMeshMap[light_name] = light_id;

    	unnamed += "_";
    } else {
    	LLOG_ERR << "Can't push light. SceneBuilder not ready !!!";
    }
}

void Session::cmdProperty(lsd::ast::Style style, const std::string& token, const Property::Value& value) {
	LLOG_DBG << "cmdProperty" << to_string(value);
	if(mpCurrentScope) {
		mpCurrentScope->setProperty(style, token, value);
	}
}

void Session::cmdDeclare(lsd::ast::Style style, lsd::ast::Type type, const std::string& token, const lsd::PropValue& value) {
	LLOG_DBG << "cmdDeclare";
	if(mpCurrentScope) {
		mpCurrentScope->declareProperty(style, type, token, value.get(), Property::Owner::USER);
	}
}

void Session::cmdTransform(const Matrix4& transform) {
	LLOG_DBG << "cmdTransform";
	auto pScope = std::dynamic_pointer_cast<scope::Transformable>(mpCurrentScope);
	if(!pScope) {
		LLOG_DBG << "Trying to set transform on non-transformable scope !!!";
		return;
	}
	pScope->setTransform(transform);
}

void Session::cmdMTransform(const Matrix4& transform) {
	LLOG_DBG << "cmdMTransform";
	auto pScope = std::dynamic_pointer_cast<scope::Transformable>(mpCurrentScope);
	if(!pScope) {
		LLOG_DBG << "Trying to add transform to non-transformable scope !!!";
		return;
	}
	pScope->addTransform(transform);
}

scope::Geo::SharedPtr Session::getCurrentGeo() {
	auto pGeo = std::dynamic_pointer_cast<scope::Geo>(mpCurrentScope);
	if(!pGeo) {
		LLOG_ERR << "Unable to get scope::Geo. Current scope type is " << to_string(mpCurrentScope->type()) << " !!!";
		return nullptr;
	}
	return pGeo;
}

bool Session::cmdStart(lsd::ast::Style object_type) {
	LLOG_DBG << "cmdStart";
	auto pGlobal = std::dynamic_pointer_cast<scope::Global>(mpCurrentScope);
	if(!pGlobal) {
		LLOG_FTL << "Objects creation allowed only inside global scope !!!";
		return false;
	}

	switch (object_type) {
		case lsd::ast::Style::GEO: 
			mpCurrentScope = pGlobal->addGeo();
			break;
		case lsd::ast::Style::OBJECT: 
			mpCurrentScope = pGlobal->addObject();
			break;
		case lsd::ast::Style::LIGHT:
			mpCurrentScope = pGlobal->addLight();
			break;
		case lsd::ast::Style::PLANE:
			mpCurrentScope = pGlobal->addPlane();
			break;
		case lsd::ast::Style::SEGMENT:
			mpCurrentScope = pGlobal->addSegment();
			break;
		default:
			LLOG_FTL << "Objects creation allowed only inside global scope !!!";
			return false;
	}

	return true;
}

bool Session::cmdEnd() {
	LLOG_DBG << "cmdEnd";
	auto pGlobal = std::dynamic_pointer_cast<scope::Global>(mpCurrentScope);
	if(pGlobal) {
		LLOG_FTL << "Can't end global scope !!!";
		return false;
	}

	auto pParent = mpCurrentScope->parent();
	if(!pParent) {
		LLOG_FTL << "Unable to end scope with no parent !!!";
		return false;
	}

	bool result = true;

	scope::Geo::SharedPtr pGeo;
	scope::Object::SharedPtr pObj;
	scope::Plane::SharedPtr pPlane;
	scope::Light::SharedPtr pLight;

	switch(mpCurrentScope->type()) {
		case ast::Style::GEO:
			pGeo = std::dynamic_pointer_cast<scope::Geo>(mpCurrentScope);
			if( pGeo->isInline()) {
				pGeo->bgeo().printSummary(std::cout);
				pushBgeo(pGeo->detailName(), pGeo->bgeo());
			}
			break;
		case ast::Style::OBJECT:
			pObj = std::dynamic_pointer_cast<scope::Object>(mpCurrentScope);
			result = pushGeometryInstance(pObj);
			break;
		case ast::Style::PLANE:
			pPlane = std::dynamic_pointer_cast<scope::Plane>(mpCurrentScope);
			break;
		case ast::Style::LIGHT:
			pLight = std::dynamic_pointer_cast<scope::Light>(mpCurrentScope);
			pushLight(pLight);
			break;
		case ast::Style::SEGMENT:
		case ast::Style::GLOBAL:
			break;
		default:
			LLOG_ERR << "cmd_end makes no sence. Current scope type is " << to_string(mpCurrentScope->type()) << " !!!";
			break;
	}

	mpCurrentScope = pParent;
	return result;
}

bool Session::pushGeometryInstance(scope::Object::SharedPtr pObj) {
	auto it = mMeshMap.find(pObj->geometryName());
	if(it == mMeshMap.end()) {
		LLOG_ERR << "No geometry found for name " << pObj->geometryName();
		return false;
	}

	auto pSceneBuilder = mpRendererIface->getSceneBuilder();
	if (!pSceneBuilder) {
		LLOG_ERR << "Unable to push geometry instance. SceneBuilder not ready !!!";
		return false;
	}

	Falcor::SceneBuilder::Node node = {
		it->first,
		pObj->getTransformList()[0],
		glm::mat4(1),
		Falcor::SceneBuilder::kInvalidNode // just a node with no parent
	};

	uint32_t mesh_id = it->second;
	uint32_t node_id  = pSceneBuilder->addNode(node);

    // add a mesh instance to a node
    pSceneBuilder->addMeshInstance(node_id, mesh_id);

	return true;
}


bool Session::cmdGeometry(const std::string& name) {
 	LLOG_DBG << "cmdGeometry";
 	if( mpCurrentScope->type() != ast::Style::OBJECT) {
 		LLOG_ERR << "cmd_geometry outside object scope !!!";
 		return false;
 	}

 	auto pObj = std::dynamic_pointer_cast<scope::Object>(mpCurrentScope);
 	pObj->setGeometryName(name);

 	return true;
}

void Session::cmdTime(double time) {
	mFrameData.time = time;
}


}  // namespace lsd

}  // namespace lava