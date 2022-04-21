#include <utility>
#include <mutex>
#include <limits>

#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/ResourceManager.h"
#include "Falcor/Scene/Lights/Light.h"
#include "Falcor/Scene/MaterialX/MxNode.h"
#include "Falcor/Utils/ConfigStore.h"

#include "session.h"

#include "../display.h"
#include "../scene_builder.h" 

#include "lava_utils_lib/ut_fsys.h"
#include "lava_utils_lib/ut_string.h"
#include "lava_utils_lib/logging.h"

#include "glm/gtx/string_cast.hpp"



namespace lava {

namespace lsd {

using DisplayType = Display::DisplayType;

static DisplayType resolveDisplayTypeByFileName(const std::string& file_name) {
	std::string ext = ut::fsys::getFileExtension(file_name);

    if( ext == ".exr" ) return DisplayType::OPENEXR;
    if( ext == ".jpg" ) return DisplayType::JPEG;
    if( ext == ".jpeg" ) return DisplayType::JPEG;
    if( ext == ".png" ) return DisplayType::PNG;
    if( ext == ".tif" ) return DisplayType::TIFF;
    if( ext == ".tiff" ) return DisplayType::TIFF;

    return DisplayType::OPENEXR;
}

static Display::TypeFormat resolveDisplayTypeFormat(const std::string& fname) {
	if( fname == "int8") return Display::TypeFormat::UNSIGNED8;
	if( fname == "int16") return Display::TypeFormat::UNSIGNED16;
	if( fname == "float16") return Display::TypeFormat::FLOAT16;	
	if( fname == "float32") return Display::TypeFormat::FLOAT32;

	return Display::TypeFormat::FLOAT32;
}

static RendererIface::SamplePattern samplePattern(const std::string& sample_pattern_name) {
	if( sample_pattern_name == "stratified" ) return RendererIface::SamplePattern::Stratified;
	if( sample_pattern_name == "halton" )  return RendererIface::SamplePattern::Halton;
	return RendererIface::SamplePattern::Center;	
}

static RendererIface::PlaneData renderingPlaneFromLSD(scope::Plane::SharedPtr pPlane) {
	RendererIface::PlaneData planeData;

	std::string channel_name = pPlane->getPropertyValue(ast::Style::PLANE, "channel", std::string());
	if(channel_name.size() == 0) {
		LLOG_ERR << "No channel name specified for plane !!!";
	}

	std::string plane_variable = pPlane->getPropertyValue(ast::Style::PLANE, "variable", std::string());
	if(plane_variable.size() == 0) {
		LLOG_ERR << "No plane variable specified for plane !!!";
	}

	std::string quantize = pPlane->getPropertyValue(ast::Style::PLANE, "quantize", std::string("float32"));

	planeData.name = channel_name;
	planeData.format = resolveDisplayTypeFormat(quantize);

	return planeData;
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

Session::~Session() {
	mpRendererIface.reset(nullptr);
}

void Session::cmdSetEnv(const std::string& key, const std::string& value) {
	mpRendererIface->setEnvVariable(key, value);
}

void Session::cmdConfig(lsd::ast::Type type, const std::string& name, const lsd::PropValue& value) {
	auto& configStore = Falcor::ConfigStore::instance();

	switch(type) {
		case ast::Type::BOOL:
			LLOG_WRN << "setting ConfigStore property " << name << " with value " << ((boost::get<int>(value) == 0) ? "false" : "true") << "\n";
			configStore.set<bool>(name, (boost::get<int>(value) == 0) ? false : true );
			break;	
		case ast::Type::STRING:
			LLOG_WRN << "setting ConfigStore property " << name << " with value " << boost::get<std::string>(value) << "\n";
			configStore.set<std::string>(name, boost::get<std::string>(value));
			break;
		case ast::Type::INT:
			LLOG_WRN << "setting ConfigStore property " << name << " with value " << boost::get<int>(value) << "\n";
			configStore.set<int>(name, boost::get<int>(value));
			break;
		default:
			LLOG_WRN << "Unsupported config store property type: " << to_string(type);
			break;
	}

	return;
}

std::string Session::getExpandedString(const std::string& str) {
	return mpRendererIface->getExpandedString(str);
}

void Session::cmdImage(lsd::ast::DisplayType display_type, const std::string& filename) {
	LLOG_DBG << "cmdImage";
	mFrameData.imageFileName = filename;

	if (display_type == lsd::ast::DisplayType::NONE ) {
		mDisplayData.displayType = resolveDisplayTypeByFileName(filename);
	} else {
    	mDisplayData.displayType = display_type;
    }
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

	// quantization parameters
	std::string typeFormatName = mpGlobal->getPropertyValue(ast::Style::IMAGE, "quantize", std::string("float32"));
	mDisplayData.typeFormat = resolveDisplayTypeFormat(typeFormatName);

	return true;
}

// initialize frame independent render data
bool Session::prepareGlobalData() {
	LLOG_DBG << "prepareGlobalData";

	// set up frame resolution (as they don't have to be the same size)
	Int2 resolution = mpGlobal->getPropertyValue(ast::Style::IMAGE, "resolution", Int2{640, 480});
	mGlobalData.imageWidth = resolution[0];
	mGlobalData.imageHeight = resolution[1];


	// set up image sampling
	mGlobalData.imageSamples = mpGlobal->getPropertyValue(ast::Style::IMAGE, "samples", 1);

	std::string samplePatternName = mpGlobal->getPropertyValue(ast::Style::IMAGE, "samplingpattern", std::string("stratified"));
	mGlobalData.samplePattern = samplePattern(samplePatternName);

	// effects
	mGlobalData.use_ssao = mpGlobal->getPropertyValue(ast::Style::RENDERER, "use_ssao", false);

	return true;
}

// initialize frame dependent render data
bool Session::prepareFrameData() {
	LLOG_DBG << "prepareFrameData";
	
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
		
		double height_k = static_cast<double>(mGlobalData.imageHeight) / static_cast<double>(mGlobalData.imageWidth);
		mFrameData.cameraFrameHeight = height_k * 50.0;
	}

	// effects ssao
	mFrameData.ssao_distance = mpGlobal->getPropertyValue(ast::Style::RENDERER, "ssao_distance", (float)0.5);
    mFrameData.ssao_factor = mpGlobal->getPropertyValue(ast::Style::RENDERER, "ssao_factor", (float) 1.0);
    mFrameData.ssao_precision = mpGlobal->getPropertyValue(ast::Style::RENDERER, "ssao_precision", (float)1.0);
    mFrameData.ssao_bent_normals = mpGlobal->getPropertyValue(ast::Style::RENDERER, "ssao_bent_normals", false);
    mFrameData.ssao_bounce_approx = mpGlobal->getPropertyValue(ast::Style::RENDERER, "ssao_bent_normals", false);

	return true;
}

void Session::cmdQuit() {
	mpRendererIface = nullptr;
}

bool Session::cmdRaytrace() {
	LLOG_DBG << "cmdRaytrace";
	mpGlobal->printSummary(std::cout);
	
	// push frame independent data to the rendering interface
	if(mFirstRun) {
		if(!mpRendererIface->initRenderer()) return false;

		if(!prepareGlobalData()) {
			LLOG_ERR << "Unable to prepare global data !!!";
			return false;
		}

		mpRendererIface->initRendererGlobalData(mGlobalData);

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

void Session::pushBgeo(const std::string& name, ika::bgeo::Bgeo::SharedConstPtr pBgeo, bool async) {
	LLOG_DBG << "pushBgeo";
    //bgeo.printSummary(std::cout);

    auto pSceneBuilder = mpRendererIface->getSceneBuilder();
    if(pSceneBuilder) {
    	if (async) {
    		// async mesh add 
    		mMeshMap[name] = pSceneBuilder->addGeometryAsync(pBgeo, name);
    	} else {
    		// immediate mesh add
    		mMeshMap[name] = pSceneBuilder->addGeometry(pBgeo, name);
    	}
    } else {
    	LLOG_ERR << "Can't push geometry (bgeo). SceneBuilder not ready !!!";
    }
}

void Session::pushLight(const scope::Light::SharedPtr pLightScope) {
	LLOG_DBG << "pushLight";
	
    auto pSceneBuilder = mpRendererIface->getSceneBuilder();

    if (!pSceneBuilder) {
		LLOG_ERR << "Unable to push light. SceneBuilder not ready !!!";
		return;
	}

	const std::string& light_type = pLightScope->getPropertyValue(ast::Style::LIGHT, "type", std::string("point"));
	const std::string& light_name = pLightScope->getPropertyValue(ast::Style::OBJECT, "name", std::string(""));
	glm::mat4 transform = pLightScope->getTransformList()[0];

	Falcor::float3 light_color = {1.0, 1.0, 1.0}; // defualt light color
	Falcor::float3 light_pos = {transform[3][0], transform[3][1], transform[3][2]}; // light position
	
	bool singleSidedLight = true;
	bool reverseLight = false;

	Falcor::float3 light_dir = {-transform[2][0], -transform[2][1], -transform[2][2]};
	LLOG_DBG << "Light dir: " << light_dir[0] << " " << light_dir[1] << " " << light_dir[2];

	Property* pShaderProp = pLightScope->getProperty(ast::Style::LIGHT, "shader");
	if(pShaderProp) {
		auto pShaderProps = pShaderProp->subContainer();
		light_color = to_float3(pShaderProps->getPropertyValue(ast::Style::LIGHT, "lightcolor", lsd::Vector3{1.0, 1.0, 1.0}));
		singleSidedLight = pShaderProps->getPropertyValue(ast::Style::LIGHT, "singlesided", bool(false));
		reverseLight = pShaderProps->getPropertyValue(ast::Style::LIGHT, "reverse", bool(false));
	} else {
		LLOG_ERR << "No shader property set for light " << light_name;
	}

	Falcor::Light::SharedPtr pLight = nullptr;

	if( (light_type == "distant") || (light_type == "sun") ) {
		auto pDistantLight = Falcor::DistantLight::create("noname_distant");
		pDistantLight->setWorldDirection(light_dir);
		
		pLight = std::dynamic_pointer_cast<Falcor::Light>(pDistantLight);
	} else if( light_type == "point") {
		auto pPointLight = Falcor::PointLight::create("noname_point");
		pPointLight->setWorldPosition(light_pos);
		pPointLight->setWorldDirection(light_dir);

		pLight = std::dynamic_pointer_cast<Falcor::Light>(pPointLight);
	} else if( light_type == "grid" || light_type == "disk" || light_type == "sphere") {
		Falcor::AnalyticAreaLight::SharedPtr pAreaLight = nullptr;

		lsd::Vector2 area_size = pLightScope->getPropertyValue(ast::Style::LIGHT, "areasize", lsd::Vector2{1.0, 1.0});

		if( light_type == "grid") {
			pAreaLight = Falcor::RectLight::create("noname_rect");
			pAreaLight->setScaling({area_size[0], area_size[1], 1.0f});
		} else if ( light_type == "disk") {
			pAreaLight = Falcor::DiscLight::create("noname_disk");
			pAreaLight->setScaling({area_size[0], area_size[1], 1.0f});
		} else if ( light_type == "sphere") {
			pAreaLight = Falcor::SphereLight::create("noname_sphere");
			pAreaLight->setScaling({area_size[0], area_size[1], area_size[0]});
		}

		if (!pAreaLight) {
			LLOG_ERR << "Error creating AnalyticAreaLight !!! Skipping...";
			return;
		}
		pAreaLight->setTransformMatrix(transform);
		pAreaLight->setSingleSided(singleSidedLight);
		

		pLight = std::dynamic_pointer_cast<Falcor::Light>(pAreaLight);
	} else if( light_type == "env") {
		// Environment light probe is not a classid light source. It should be created later by scene builder or renderer

		std::string texture_file_name = pLightScope->getPropertyValue(ast::Style::LIGHT, "areamap", std::string(""));

		auto pDevice = pSceneBuilder->device();
		//LightProbe::SharedPtr pLightProbe;
		
		if (texture_file_name.size() == 0) {
			// solid color lightprobe
			//pLightProbe = LightProbe::create(pDevice->getRenderContext());
		} else {
    		bool loadAsSrgb = false;
    		//uint32_t diffSampleCount = 8192; // preintegration
    		//uint32_t specSampleCount = 8192; // preintegration
    		auto pResourceManager = pDevice->resourceManager();
    		if (pResourceManager) {
        		auto pEnvMapTexture = pResourceManager->createTextureFromFile(texture_file_name, true, loadAsSrgb);
    			EnvMap::SharedPtr pEnvMap = EnvMap::create(pDevice, pEnvMapTexture);
    			pEnvMap->setTint(light_color);
    			pSceneBuilder->setEnvMap(pEnvMap);
    		}
    	}
    	return;
	} else { 
		LLOG_WRN << "Unsupported light type " << light_type << ". Skipping...";
		return;
	}

	if(pLight) {
		LLOG_DBG << "Light " << light_name << "  type " << pLight->getData().type;

		if (light_name != "") {
			pLight->setName(light_name);
		}

		pLight->setHasAnimation(false);

		Property* pShadowProp = pLightScope->getProperty(ast::Style::LIGHT, "shadow");
		
		// set shadow parameters
		if (pShadowProp) {
			auto pShadowProps = pShadowProp->subContainer();
			if (pShadowProps) {
				const std::string shadow_type_name = pShadowProps->getPropertyValue(ast::Style::LIGHT, "shadowtype", std::string(""));
				if (shadow_type_name == "filter") {
					// alpha aware ray traced shadows
					pLight->setShadowType(LightShadowType::RayTraced);
				} else if (shadow_type_name == "fast") {
					// opaque ray traced shadows
					pLight->setShadowType(LightShadowType::RayTraced);
				} else if (shadow_type_name == "deep") {
					pLight->setShadowType(LightShadowType::ShadowMap);
				}
				const Falcor::float3 shadow_color = to_float3(pShadowProps->getPropertyValue(ast::Style::LIGHT, "shadow_color", lsd::Vector3{0.0, 0.0, 0.0}));
				pLight->setShadowColor(shadow_color);
			}
		}

		pLight->setIntensity(light_color);
		uint32_t light_id = pSceneBuilder->addLight(pLight);
		mLightsMap[light_name] = light_id;
	}
}

void Session::cmdProperty(lsd::ast::Style style, const std::string& token, const Property::Value& value) {
	LLOG_DBG << "cmdProperty " << token << " " << to_string(value);
	if(!mpCurrentScope) {
		LLOG_ERR << "No current scope is set !!!";
		return; 
	}
	mpCurrentScope->setProperty(style, token, value);
}

void Session::cmdPropertyV(lsd::ast::Style style, const std::vector<std::pair<std::string, Property::Value>>& values) {
	LLOG_DBG << "cmdPropertyV ";
	if(!mpCurrentScope) {
		LLOG_ERR << "No current scope is set !!!";
		return; 
	}

	if (values.size() < 2) {
		LLOG_ERR << "Property array size should be at least 2 elements !!!";
		return;
	}

	mpCurrentScope->setProperty(style, values[0].first, values[0].second);
	Property* pProp = mpCurrentScope->getProperty(style, values[0].first);

	if(!pProp) {
		LLOG_ERR << "Error getting property " << values[0].first;
		return;
	}

	auto pSubContainer = pProp->subContainer();
	if(!pSubContainer) {
		LLOG_ERR << "No sub-container for property " << values[0].first;
		return;
	}

	for(auto it = values.begin() + 1; it != values.end(); it++) {
		pSubContainer->setProperty(style, it->first, it->second);
	}
}

void Session::cmdEdge(const std::string& src_node_uuid, const std::string& src_node_output_socket, const std::string& dst_node_uuid, const std::string& dst_node_input_socket) {
	LLOG_DBG << "cmdEdge";
	auto pNode = std::dynamic_pointer_cast<scope::Node>(mpCurrentScope);
	if(pNode) {
		pNode->addChildEdge(src_node_uuid, src_node_output_socket, dst_node_uuid, dst_node_input_socket);
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

scope::ScopeBase::SharedPtr Session::getCurrentScope() {
	return std::dynamic_pointer_cast<scope::ScopeBase>(mpCurrentScope);
}

void Session::cmdIPRmode(const std::string& mode) {
	LLOG_DBG << "cmdIPRmode";
	mIPRmode = true;
	//mpRendererIface->setIPRMode(mIPRmode);
}

bool Session::cmdStart(lsd::ast::Style object_type) {
	LLOG_DBG << "cmdStart";

	switch (object_type) {
		case lsd::ast::Style::GEO:
			{ 
				auto pGlobal = std::dynamic_pointer_cast<scope::Global>(mpCurrentScope);
				if(!pGlobal) {
					LLOG_FTL << "Geometries creation allowed only inside global scope !!!";
					return false;
				}
				mpCurrentScope = pGlobal->addGeo();
				break;
			}
		case lsd::ast::Style::OBJECT:
			{
				auto pGlobal = std::dynamic_pointer_cast<scope::Global>(mpCurrentScope);
				if(!pGlobal) {
					LLOG_FTL << "Objects creation allowed only inside global scope !!!";
					return false;
				} 
				mpCurrentScope = pGlobal->addObject();
				break;
			}
		case lsd::ast::Style::LIGHT:
			{
				auto pGlobal = std::dynamic_pointer_cast<scope::Global>(mpCurrentScope);
				if(!pGlobal) {
					LLOG_FTL << "Lights creation allowed only inside global scope !!!";
					return false;
				}
				mpCurrentScope = pGlobal->addLight();
				break;
			}
		case lsd::ast::Style::PLANE:
			{
				auto pGlobal = std::dynamic_pointer_cast<scope::Global>(mpCurrentScope);
				if(!pGlobal) {
					LLOG_FTL << "Planes creation allowed only inside global scope !!!";
					return false;
				}
				mpCurrentScope = pGlobal->addPlane();
				break;
			}
		case lsd::ast::Style::SEGMENT:
			{
				auto pGlobal = std::dynamic_pointer_cast<scope::Global>(mpCurrentScope);
				if(!pGlobal) {
					LLOG_FTL << "Segments creation allowed only inside global scope !!!";
					return false;
				}
				mpCurrentScope = pGlobal->addSegment();
				break;
			}
		case lsd::ast::Style::MATERIAL:
			{
				auto pGlobal = std::dynamic_pointer_cast<scope::Global>(mpCurrentScope); 
				if(!pGlobal) {
					LLOG_FTL << "Materials creation allowed only inside global scope !!!";
					return false;
				}
				mpCurrentScope = pGlobal->addMaterial();
				mpMaterialScope = std::dynamic_pointer_cast<scope::Material>(mpCurrentScope);
				break;
			}
		case lsd::ast::Style::NODE:
			{ 
				auto pNode = std::dynamic_pointer_cast<scope::Node>(mpCurrentScope);
				if(!pNode) {
					LLOG_FTL << "Nodes creation allowed only inside material/node scope !!!";
					return false;
				}
				mpCurrentScope = pNode->addChildNode();
				break;
			}
		default:
			LLOG_FTL << "Unsupported cmd_start style: " << to_string(object_type);
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

	const auto& configStore = Falcor::ConfigStore::instance();

	bool pushGeoAsync = configStore.get<bool>("async_geo", true);
	bool result = true;

	scope::Geo::SharedPtr pGeo;
	scope::Object::SharedPtr pObj;
	scope::Plane::SharedPtr pPlane;
	scope::Light::SharedPtr pLight;
	scope::Material::SharedPtr pMaterialScope;
	scope::Node::SharedPtr pNode;

	switch(mpCurrentScope->type()) {
		case ast::Style::GEO:
			pGeo = std::dynamic_pointer_cast<scope::Geo>(mpCurrentScope);
			if( pGeo->isInline()) {
				pGeo->bgeo()->printSummary(std::cout);
				pushBgeo(pGeo->detailName(), pGeo->bgeo(), pushGeoAsync);
			} else {
				pGeo->bgeo()->printSummary(std::cout);
				pushBgeo(pGeo->detailName(), pGeo->bgeo(), pushGeoAsync);
			}
			break;
		case ast::Style::OBJECT:
			pObj = std::dynamic_pointer_cast<scope::Object>(mpCurrentScope);
			if(!pushGeometryInstance(pObj)) {
				//return false;
				result = false;
			}
			break;
		case ast::Style::PLANE:
			pPlane = std::dynamic_pointer_cast<scope::Plane>(mpCurrentScope);
			mpRendererIface->addPlane(renderingPlaneFromLSD(pPlane));
			break;
		case ast::Style::LIGHT:
			pLight = std::dynamic_pointer_cast<scope::Light>(mpCurrentScope);
			pushLight(pLight);
			break;
		case ast::Style::MATERIAL:
			pMaterialScope = std::dynamic_pointer_cast<scope::Material>(mpCurrentScope);
			if(pMaterialScope) {
				auto pMaterialX = createMaterialXFromLSD(pMaterialScope);
				if (pMaterialX) {
					mpRendererIface->addMaterialX(std::move(pMaterialX));
				}
			} else {
				result = false;
			}
			mpMaterialScope = nullptr;
			break;
		case ast::Style::NODE:
			pNode = std::dynamic_pointer_cast<scope::Node>(mpCurrentScope);
			break;
		case ast::Style::SEGMENT:
		case ast::Style::GLOBAL:
			break;
		default:
			LLOG_ERR << "cmd_end makes no sense. Current scope type is " << to_string(mpCurrentScope->type()) << " !!!";
			break;
	}

	mpCurrentScope = pParent;
	return result;
}

void Session::addMxNode(Falcor::MxNode::SharedPtr pParent, scope::Node::SharedConstPtr pNodeLSD) {
	return;
	assert(pParent);

	bool is_subnet = pNodeLSD->getPropertyValue(ast::Style::OBJECT, "is_subnet", bool(false));
	std::string node_name = pNodeLSD->getPropertyValue(ast::Style::OBJECT, "node_name", std::string(""));
	std::string node_type = pNodeLSD->getPropertyValue(ast::Style::OBJECT, "node_type", std::string(""));
	std::string node_uuid = pNodeLSD->getPropertyValue(ast::Style::OBJECT, "node_uuid", std::string(""));
	std::string node_namespace = pNodeLSD->getPropertyValue(ast::Style::OBJECT, "node_namespace", std::string("houdini"));

	MxNode::TypeCreateInfo info = {};
    info.nameSpace = node_namespace;
    info.typeName = node_type;
    info.version = 0;

	auto pNode = pParent->createNode(info, node_name);
	if (pNode) {
		if (mpMaterialScope->insertNode(node_uuid, pNode)) {

			for( const auto& tmpl: pNodeLSD->socketTemplates()) {
				auto pSocket = pNode->addDataSocket(tmpl.name, tmpl.dataType, tmpl.direction);
				if (!pSocket) {
					LLOG_ERR << "Error creating shading node data socket " << tmpl.name << " !!!";
				} else {
					LLOG_DBG << "Created node " << tmpl.direction << " socket " << pSocket->path(); 
				}
			}

			if (is_subnet) {
				// add child nodes
				for( scope::Node::SharedConstPtr pChildNodeLSD: pNodeLSD->childNodes()) {
					addMxNode(pNode, pChildNodeLSD);
				}

				// link sockets
				for( const scope::Node::EdgeInfo& edge: pNodeLSD->childEdges()) {
					Falcor::MxNode::SharedPtr pSrcNode = pNode->node(edge.src_node_uuid);
					Falcor::MxNode::SharedPtr pDstNode = pNode->node(edge.dst_node_uuid);

					if( pSrcNode && pDstNode) {
						Falcor::MxSocket::SharedPtr pSrcSocket = pSrcNode->outputSocket(edge.src_node_output_socket);
						Falcor::MxSocket::SharedPtr pDstSocket = pDstNode->inputSocket(edge.dst_node_input_socket);
						if( pSrcSocket && pDstSocket) {
							if (!pDstSocket->setInput(pDstSocket)) {
								LLOG_ERR << "Error connecting socket " << edge.src_node_output_socket << " to " << edge.dst_node_input_socket;
							}
						} else {
							if( !pSrcSocket) LLOG_ERR << "No socket named " << edge.src_node_output_socket << " at node " << pSrcNode->name();
							if( !pDstSocket) LLOG_ERR << "No socket named " << edge.dst_node_input_socket << " at node " << pDstNode->name();
						}
					} else {
						if( !pSrcNode) LLOG_ERR << "No shading node " << edge.src_node_uuid << " exist !!!";
						if( !pDstNode) LLOG_ERR << "No shading node " << edge.dst_node_uuid << " exist !!!";
					}
				}
			}
		}
	}
}

Falcor::MaterialX::UniquePtr Session::createMaterialXFromLSD(scope::Material::SharedConstPtr pMaterialLSD) {
	
	std::string material_name = pMaterialLSD->getPropertyValue(ast::Style::OBJECT, "material_name", std::string(""));

	// We create material without device at this stage. Actual device would be set up for this material later by the
	// renderer itself.
	auto pMx = MaterialX::createUnique(nullptr, material_name);

	for( scope::Node::SharedConstPtr pNodeLSD: pMaterialLSD->childNodes()) {
		addMxNode(pMx->rootNode(), pNodeLSD);
	}

	return std::move(pMx);
}

bool Session::cmdSocket(Falcor::MxSocketDirection direction, Falcor::MxSocketDataType dataType, const std::string& name) {
	assert(mpCurrentScope);

	if (mpCurrentScope->type() != ast::Style::NODE) {
		LLOG_ERR << "Error adding node socket. Current scope is not \"node\" !!!";
		return false;
	}

	auto pNode = std::dynamic_pointer_cast<scope::Node>(mpCurrentScope);
	pNode->addDataSocketTemplate(name, dataType, direction);
	return true;
}

bool Session::pushGeometryInstance(scope::Object::SharedConstPtr pObj) {
	LLOG_DBG << "pushGeometryInstance for geometry (mesh) name: " << pObj->geometryName();
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

	assert(pSceneBuilder->device());

	std::string obj_name = pObj->getPropertyValue(ast::Style::OBJECT, "name", std::string("unnamed"));

	uint32_t mesh_id = std::numeric_limits<uint32_t>::max();

	try {
		LLOG_DBG << "getting sync mesh_id for obj name instance: "  << obj_name << " geo name: " << pObj->geometryName();
		mesh_id = std::get<uint32_t>(it->second);	
	} catch (const std::bad_variant_access&) {
		LLOG_DBG << "getting async mesh_id for obj_name " << obj_name;

		std::shared_future<uint32_t>& f = std::get<std::shared_future<uint32_t>>(it->second);
		try {
			mesh_id = f.get();	
		} catch(const std::exception& e) {
        	std::cerr << "Exception from the thread: " << e.what() << '\n';
        	return false;
    	}
	} catch (...) {
		logError("Unable to get mesh id for object: " + obj_name);
	}

	if (mesh_id == std::numeric_limits<uint32_t>::max()) {
		return false;
	}

	LLOG_DBG << "mesh_id " << mesh_id;

	Falcor::SceneBuilder::Node node = {};
	node.name = it->first;
	node.transform = pObj->getTransformList()[0];
	node.meshBind = glm::mat4(1);          // For skinned meshes. World transform at bind time.
    node.localToBindPose = glm::mat4(1);   // For bones. Inverse bind transform.
	/*
		glm::mat4(1), // For skinned meshes. World transform at bind time.
		glm::mat4(1), // For bones. Inverse bind transform.
		Falcor::SceneBuilder::kInvalidNode // just a node with no parent
	};
	*/

	uint32_t node_id = pSceneBuilder->addNode(node);

	// TODO: this is naive test. fetch basic material data
	const Property* pShaderProp = pObj->getProperty(ast::Style::OBJECT, "surface");
    
    Falcor::float3 	surface_base_color = {1.0, 1.0, 1.0};
    std::string 	surface_base_color_texture = "";
    std::string 	surface_base_normal_texture = "";
    std::string 	surface_metallic_texture = "";
    std::string 	surface_rough_texture = "";

    bool 			surface_use_basecolor_texture = false;
    bool 			surface_use_roughness_texture = false;
    bool 			surface_use_metallic_texture = false;
    bool 			surface_use_basenormal_texture = false;

    float 		 	surface_ior = 1.5;
    float 			surface_metallic = 0.0;
    float 			surface_roughness = 0.5;
    float 			surface_reflectivity = 1.0;

    if(pShaderProp) {
    	auto pShaderProps = pShaderProp->subContainer();
    	surface_base_color = to_float3(pShaderProps->getPropertyValue(ast::Style::OBJECT, "basecolor", lsd::Vector3{0.2, 0.2, 0.2}));
    	surface_base_color_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "basecolor_texture", std::string());
    	surface_base_normal_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseNormal_texture", std::string());
    	surface_metallic_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "metallic_texture", std::string());
    	surface_rough_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "rough_texture", std::string());

    	surface_use_basecolor_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "basecolor_useTexture", false);
    	surface_use_metallic_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "metallic_useTexture", false);
    	surface_use_roughness_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "rough_useTexture", false);
    	surface_use_basenormal_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseBumpAndNormal_enable", false);

    	surface_ior = pShaderProps->getPropertyValue(ast::Style::OBJECT, "ior", 1.5);
    	surface_metallic = pShaderProps->getPropertyValue(ast::Style::OBJECT, "metallic", 0.0);
    	surface_roughness = pShaderProps->getPropertyValue(ast::Style::OBJECT, "rough", 0.3);
    	surface_reflectivity = pShaderProps->getPropertyValue(ast::Style::OBJECT, "reflect", 1.0);
    } else {
    	LLOG_ERR << "No surface property set for object " << obj_name;
    }

    auto pMaterial = Falcor::Material::create(pSceneBuilder->device(), obj_name);
    pMaterial->setBaseColor({surface_base_color, 1.0});
    pMaterial->setIndexOfRefraction(surface_ior);
    pMaterial->setMetallic(surface_metallic);
    pMaterial->setRoughness(surface_roughness);
    pMaterial->setReflectivity(surface_reflectivity);

    LLOG_DBG << "setting material textures";
  	bool loadAsSrgb = true;

    if(surface_base_color_texture != "" && surface_use_basecolor_texture) 
    	pMaterial->loadTexture(Falcor::Material::TextureSlot::BaseColor, surface_base_color_texture, true); // load as srgb texture

    if(surface_metallic_texture != "" && surface_use_metallic_texture) 
    	pMaterial->loadTexture(Falcor::Material::TextureSlot::Specular, surface_metallic_texture, false); // load as linear texture

    if(surface_rough_texture != "" && surface_use_roughness_texture) 
    	pMaterial->loadTexture(Falcor::Material::TextureSlot::Roughness, surface_rough_texture, false); // load as linear texture

    if(surface_base_normal_texture != "" && surface_use_basenormal_texture) 
    	pMaterial->loadTexture(Falcor::Material::TextureSlot::Normal, surface_base_normal_texture, false); // load as linear texture

    // add a mesh instance to a node
    pSceneBuilder->addMeshInstance(node_id, mesh_id, pMaterial);
    
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