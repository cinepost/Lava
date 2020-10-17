#include <utility>
#include "session.h"

#include "lava_utils_lib/ut_fsys.h"
#include "lava_utils_lib/logging.h"

namespace lava {

namespace lsd {

std::string resolveDisplayDriverByFileName(const std::string& file_name) {
	std::string ext = ut::fsys::getFileExtension(file_name);

    if( ext == ".exr" ) return std::move("openexr");
    if( ext == ".jpg" ) return std::move("jpeg");
    if( ext == ".jpeg" ) return std::move("jpeg");
    if( ext == ".png" ) return std::move("png");
    if( ext == ".tif" ) return std::move("tiff");
    if( ext == ".tiff" ) return std::move("tiff");
    return std::move("openexr");
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

Session::Session(std::unique_ptr<RendererIface> pRendererIface) { 
	mpRendererIface = std::move(pRendererIface);
}

Session::~Session() { }


bool Session::setDisplayByType(const lsd::ast::DisplayType& display_type) {
	std::string display_name;

	switch(display_type) {
		case lsd::ast::DisplayType::IP:
		case lsd::ast::DisplayType::MD:
			display_name = "houdini";
			break;
		case lsd::ast::DisplayType::SDL:
			display_name = "sdl";
			break;
		case lsd::ast::DisplayType::OPENEXR:
			display_name = "openexr";
			break;
		case lsd::ast::DisplayType::JPEG:
			display_name = "jpeg";
			break;
		case lsd::ast::DisplayType::TIFF:
			display_name = "tiff";
			break;
		case lsd::ast::DisplayType::PNG:
		default:
			display_name = "png";
			break;
	}

	return mpRendererIface->loadDisplay(display_name);
}

bool Session::setDisplayByFileName(const std::string& file_name) {
	return mpRendererIface->loadDisplay(lsd::resolveDisplayDriverByFileName(file_name));
}

void Session::cmdSetEnv(const std::string& key, const std::string& value) {
	mpRendererIface->setEnvVariable(key, value);
}

void Session::cmdConfig(const std::string& file_name) {
	// actual render graph configs loading postponed unitl renderer is initialized
	mGraphConfigsFileNames.push_back(file_name);
}

std::string Session::getExpandedString(const std::string& str) {
	return mpRendererIface->getExpandedString(str);
}

// initialize renderer and push render data
bool Session::initRenderData() {
	LLOG_DBG << "initRenderData";
	if(!mpRendererIface->initRenderer()) return false;

	for(auto const& graph_conf_file: mGraphConfigsFileNames) {
		if(!mpRendererIface->loadScriptFile(graph_conf_file)) return false;
	}

	return true;
}

bool Session::cmdImage(lsd::ast::DisplayType display_type, const std::string& filename) {
	LLOG_DBG << "cmdImage";
	mFrameData.imageFileName = filename;
	std::string display_name;
    if (display_type != ast::DisplayType::NONE) {
		switch(display_type) {
			case lsd::ast::DisplayType::IP:
			case lsd::ast::DisplayType::MD:
				display_name = "houdini";
				break;
			case lsd::ast::DisplayType::SDL:
				display_name = "sdl";
				break;
			case lsd::ast::DisplayType::OPENEXR:
				display_name = "openexr";
				break;
			case lsd::ast::DisplayType::JPEG:
				display_name = "jpeg";
				break;
			case lsd::ast::DisplayType::TIFF:
				display_name = "tiff";
				break;
			case lsd::ast::DisplayType::PNG:
			default:
				display_name = "png";
				break;
		}
    } else {
    	display_name = lsd::resolveDisplayDriverByFileName(filename);
    }

	if(!mpRendererIface->loadDisplay(display_name)) {
		LLOG_FTL << "Failed to load \"" << display_name << "\" display driver !!!";
		return false;
	}

    return true;
}

bool Session::pushScriptFiles() {
	// this section checks it's our first run, if so load render graph configs
	bool load_configs = false;
	if(!mpRendererIface->isRendererInitialized()) {
		load_configs = true;
	
		if(!mpRendererIface->initRenderer()){
			LLOG_FTL << "Error initializing renderer !!!";
			return false;
		}
	}

	if(load_configs) {
		for(auto const& script_file_name: mGraphConfigsFileNames) {
			if(!mpRendererIface->loadScriptFile(script_file_name)) {
				LLOG_ERR << "Error pushing script: " << script_file_name << " !!!";
				return false;
			}
		}
	}

	return true;
}

bool Session::cmdRaytrace() {
	mpGlobal->printSummary(std::cout);
	LLOG_DBG << "cmdRaytrace";
	
	Int2 resolution = mpGlobal->getPropertyValue(ast::Style::IMAGE, "resolution", Int2{640, 480});
	mFrameData.imageWidth = resolution[0];
	mFrameData.imageHeight = resolution[1];
	
	// push render graph configuration scripts
	if(!pushScriptFiles())
		return false;

	// prepare display driver parameters
	auto& props_container = mpGlobal->filterProperties(ast::Style::PLANE, std::regex("^IPlay\\.[a-zA-Z]*"));
	for( auto const& item: props_container.properties()) {
		LLOG_DBG << "IPlay property: " << to_string(item.first);

		const std::string& parm_name = item.first.second.substr(6); // remove leading "IPlay."
		const Property& prop = item.second;
		switch(item.second.type()) {
			case ast::Type::FLOAT:
				//LLOG_DBG << "type: " << to_string(prop.type()) << " value: " << to_string(prop.value());
				mFrameData.displayFloatParameters.push_back(std::pair<std::string, std::vector<float>>( parm_name, {prop.get<float>()} ));
				break;
			case ast::Type::INT:
				mFrameData.displayIntParameters.push_back(std::pair<std::string, std::vector<int>>( parm_name, {prop.get<int>()} ));
				break;
			case ast::Type::STRING:
				mFrameData.displayStringParameters.push_back(std::pair<std::string, std::vector<std::string>>( parm_name, {prop.get<std::string>()} ));
				break;
			default:
				break;
		}
	}

	// push geometries
	/*
	auto pSceneBuilder = mpRendererIface->getSceneBuilder();
    if(pSceneBuilder) {

    	for (const auto& pGeo: mpGlobal->geos()) {
    		if(pGeo->isInline()) {
    			// push inline geometry
    			uint32_t mesh_id = pSceneBuilder->addMesh(std::move(pGeo->bgeo()));
    			mMeshMap[pGeo->detailName()] = mesh_id;
    		}
    	}
    } else {
    	LLOG_WRN << "SceneBuilder is not ready !!!";
    }
	*/

	mpRendererIface->renderFrame(mFrameData);
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

void Session::cmdProperty(lsd::ast::Style style, const std::string& token, const lsd::PropValue& value) {
	LLOG_DBG << "cmdProperty";
	if(mpCurrentScope) {
		mpCurrentScope->setProperty(style, token, value.get());
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
			break;
		case ast::Style::PLANE:
			pPlane = std::dynamic_pointer_cast<scope::Plane>(mpCurrentScope);
			break;
		case ast::Style::LIGHT:
			pLight = std::dynamic_pointer_cast<scope::Light>(mpCurrentScope);
			break;
		case ast::Style::GLOBAL:
		default:
			LLOG_ERR << "cmd_end makes no sence. Current scope type is " << to_string(mpCurrentScope->type()) << " !!!";
			break;
	}

	mpCurrentScope = pParent;
	return true;
}

}  // namespace lsd

}  // namespace lava