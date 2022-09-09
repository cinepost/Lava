#include <utility>
#include <mutex>
#include <limits>
#include <fstream>
#include <cstdlib>

#include "Falcor/Core/API/Texture.h"
#include "Falcor/Scene/Lights/Light.h"
#include "Falcor/Scene/Material/StandardMaterial.h"
#include "Falcor/Scene/MaterialX/MxNode.h"
#include "Falcor/Scene/MaterialX/MaterialX.h"
#include "Falcor/Utils/ConfigStore.h"

#include "session.h"
#include "session_helpers.h"
#include "display.h"

#include "../aov.h"
#include "../renderer.h"
#include "../scene_builder.h" 

#include "lava_utils_lib/ut_fsys.h"
#include "lava_utils_lib/ut_string.h"
#include "lava_utils_lib/logging.h"

#include "glm/gtx/string_cast.hpp"



namespace lava {

namespace lsd {

using DisplayType = Display::DisplayType;


Session::UniquePtr Session::create(std::shared_ptr<Renderer> pRenderer) {
	assert(pRenderer);

	auto pSession = Session::UniquePtr(new Session(pRenderer));
	auto pGlobal = scope::Global::create();
	if (!pGlobal) {
		return nullptr;
	}

	pSession->mpGlobal = pGlobal;
	pSession->mpCurrentScope = pGlobal;

	return std::move(pSession);
}

Session::Session(std::shared_ptr<Renderer> pRenderer):mFirstRun(true) { 
	mpRenderer = pRenderer;
	mpDevice = mpRenderer->device();
}

Session::~Session() {
	if(mpDisplay) mpDisplay = nullptr;
}

void Session::cmdSetEnv(const std::string& key, const std::string& value) {
	setEnvVariable(key, value);
}

void Session::cmdConfig(lsd::ast::Type type, const std::string& name, const lsd::PropValue& value) {
/*
	auto& config = ConfigStore::instance();
	switch(type) {
		case lsd::ast::Type::BOOL:
			config.set<bool>(name, value);
			break;
		default:
			break;
	}
*/
#define get_bool(_a) (boost::get<int>(_a) == 0) ? false : true

	if (name == "vtoff") {
		mRendererConfig.useVirtualTexturing = get_bool(value); return;
	}
	if (name == "fconv") {
		mRendererConfig.forceVirtualTexturesReconversion = get_bool(value); return;
	}
	if (name == "async_geo") {
		mRendererConfig.useAsyncGeometryProcessing = get_bool(value); return;
	}
	if (name == "cull_mode") {
		mRendererConfig.cullMode = boost::get<std::string>(value); return;
	}
	if (name == "vtex_conv_quality") {
		mRendererConfig.virtualTexturesCompressionQuality = boost::get<std::string>(value); return;
	}
	if (name == "vtex_tlc") {
		mRendererConfig.virtualTexturesCompressorType = boost::get<std::string>(value); return;
	}
	if (name == "vtex_tlc_level") {
		mRendererConfig.virtualTexturesCompressionLevel = (uint8_t)boost::get<int>(value); return;
	}
	if (name == "geo_tangent_generation") {
		mRendererConfig.tangentGenerationMode = boost::get<std::string>(value); return;
	}

	LLOG_WRN << "Unsupported renderer configuration property: " << name << " of type:" << to_string(type);
	return;

#undef get_bool
}

void Session::setEnvVariable(const std::string& key, const std::string& value){
	LLOG_DBG << "setEnvVariable: " << key << " : " << value;
	mEnvmap[key] = value;
}

std::string Session::getExpandedString(const std::string& s) {
		std::string result = s;

	for( auto const& [key, val] : mEnvmap )
		result = ut::string::replace(result, '$' + key, val);

	return result;
}

void Session::cmdImage(lsd::ast::DisplayType display_type, const std::string& filename) {
	LLOG_DBG << "cmdImage";
	mCurrentDisplayInfo.outputFileName = ((!filename.empty()) ? filename : std::string("unnamed"));

	if (mIPR) {
		mCurrentDisplayInfo.displayType = Display::DisplayType::IP;
	} else if (display_type == lsd::ast::DisplayType::NONE ) {
		mCurrentDisplayInfo.displayType = resolveDisplayTypeByFileName(filename);
	} else {
    	mCurrentDisplayInfo.displayType = display_type;
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
				mCurrentDisplayInfo.displayFloatParameters.push_back(std::pair<std::string, std::vector<float>>( parm_name, {prop.get<float>()} ));
				break;
			case ast::Type::INT:
				mCurrentDisplayInfo.displayIntParameters.push_back(std::pair<std::string, std::vector<int>>( parm_name, {prop.get<int>()} ));
				break;
			case ast::Type::STRING:
				mCurrentDisplayInfo.displayStringParameters.push_back(std::pair<std::string, std::vector<std::string>>( parm_name, {prop.get<std::string>()} ));
				break;
			default:
				break;
		}
	}

	// quantization parameters
	std::string typeFormatName = mpGlobal->getPropertyValue(ast::Style::IMAGE, "quantize", std::string("float32"));

	mCurrentDisplayInfo.typeFormat = resolveDisplayTypeFormat(typeFormatName);
	return true;
}

void Session::setUpCamera(Falcor::Camera::SharedPtr pCamera, Falcor::float4 cropRegion) {
	LLOG_DBG << "setUpCamera";
	assert(pCamera);
	
	// set up camera data
	Vector2 camera_clip = mpGlobal->getPropertyValue(ast::Style::CAMERA, "clip", Vector2{0.01, 1000.0});
	std::string camera_projection_name = mpGlobal->getPropertyValue(ast::Style::CAMERA, "projection", std::string("perspective"));

	auto dims = mCurrentFrameInfo.renderRegionDims();

	float aspect_ratio = static_cast<float>(mCurrentFrameInfo.imageWidth) / static_cast<float>(mCurrentFrameInfo.imageHeight);
	
	pCamera->setAspectRatio(aspect_ratio);
	pCamera->setNearPlane(camera_clip[0]);
	pCamera->setFarPlane(camera_clip[1]);
	pCamera->setViewMatrix(mpGlobal->getTransformList()[0]);
	pCamera->setCropRegion(cropRegion);

	const auto& segments = mpGlobal->segments();
	if(segments.size()) {
		const auto& pSegment = segments[0];
		pCamera->setFocalLength(50.0 * pSegment->getPropertyValue(ast::Style::CAMERA, "zoom", (double)1.0));
		pCamera->setFrameHeight((1.0f / aspect_ratio) * 50.0);
	}
}

void Session::cmdQuit() {
	//mpRendererIface = nullptr;
}

static void makeImageTiles(Renderer::FrameInfo& frameInfo, Falcor::uint2 tileSize, std::vector<Session::TileInfo>& tiles) {
	assert(frameInfo.renderRegion[0] <= frameInfo.renderRegion[2]);
	assert(frameInfo.renderRegion[1] <= frameInfo.renderRegion[3]);

	auto imageRegionDims = frameInfo.renderRegionDims();
	uint32_t imageRegionWidth = imageRegionDims[0];
	uint32_t imageRegionHeight = imageRegionDims[1];

	tileSize[0] = std::min(imageRegionWidth, tileSize[0]);
	tileSize[1] = std::min(imageRegionHeight, tileSize[1]);

	auto hdiv = ldiv((uint32_t)imageRegionWidth, (uint32_t)tileSize[0]);
	auto vdiv = ldiv((uint32_t)imageRegionHeight, (uint32_t)tileSize[1]);

	tiles.clear();
	// First put whole tiles
	for( int x = 0; x < hdiv.quot; x++) {
		for( int y = 0; y < vdiv.quot; y++) {
			Falcor::uint2 offset = {tileSize[0] * x, tileSize[1] * y};
			Falcor::uint4 tileRegion = {
				frameInfo.renderRegion[0] + offset[0],
				frameInfo.renderRegion[1] + offset[1],
				frameInfo.renderRegion[0] + offset[0] + tileSize[0] - 1,
				frameInfo.renderRegion[1] + offset[1] + tileSize[1] - 1
			};

			tiles.push_back({
				{tileRegion}, // image rendering region
				{             // camera crop region
					tileRegion[0] / (float)frameInfo.imageWidth,
					tileRegion[1] / (float)frameInfo.imageHeight,
					(tileRegion[2] + 1) / (float)frameInfo.imageWidth,
					(tileRegion[3] + 1) / (float)frameInfo.imageHeight
				}
			});
		}
	}

	// bottom row tiles
	if( vdiv.rem > 0) {
		for ( int x = 0; x < hdiv.quot; x++) {
			Falcor::uint2 offset = {tileSize[0] * x, tileSize[1] * vdiv.quot};
			Falcor::uint4 tileRegion = {
				frameInfo.renderRegion[0] + offset[0],
				frameInfo.renderRegion[1] + offset[1],
				frameInfo.renderRegion[0] + offset[0] + tileSize[0] - 1,
				frameInfo.renderRegion[1] + offset[1] + vdiv.rem - 1
			};
			tiles.push_back({
				{tileRegion}, // image rendering region
				{             // camera crop region
					tileRegion[0] / (float)frameInfo.imageWidth,
					tileRegion[1] / (float)frameInfo.imageHeight,
					(tileRegion[2] + 1) / (float)frameInfo.imageWidth,
					(tileRegion[3] + 1) / (float)frameInfo.imageHeight
				}
			});
		}
	}

	// right row tiles
	if( hdiv.rem > 0) {
		for ( int y = 0; y < vdiv.quot; y++) {
			Falcor::uint2 offset = {tileSize[0] * hdiv.quot, tileSize[1] * y};
			Falcor::uint4 tileRegion = {
				frameInfo.renderRegion[0] + offset[0],
				frameInfo.renderRegion[1] + offset[1],
				frameInfo.renderRegion[0] + offset[0] + hdiv.rem - 1,
				frameInfo.renderRegion[1] + offset[1] + tileSize[1] - 1
			};
			tiles.push_back({
				{tileRegion}, // image rendering region
				{             // camera crop region
					tileRegion[0] / (float)frameInfo.imageWidth,
					tileRegion[1] / (float)frameInfo.imageHeight,
					(tileRegion[2] + 1) / (float)frameInfo.imageWidth,
					(tileRegion[3] + 1) / (float)frameInfo.imageHeight
				}
			});
		}
	}

	// last corner tile
	if ((hdiv.rem >= 1) && (vdiv.rem >=1)) {
		Falcor::uint2 offset = {tileSize[0] * hdiv.quot, tileSize[1] * vdiv.quot};
		Falcor::uint4 tileRegion = {
			frameInfo.renderRegion[0] + offset[0],
			frameInfo.renderRegion[1] + offset[1],
			frameInfo.renderRegion[0] + offset[0] + hdiv.rem - 1,
			frameInfo.renderRegion[1] + offset[1] + vdiv.rem - 1
		};
		tiles.push_back({
			{tileRegion}, // image rendering region
			{             // camera crop region
				tileRegion[0] / (float)frameInfo.imageWidth,
				tileRegion[1] / (float)frameInfo.imageHeight,
				(tileRegion[2] + 1) / (float)frameInfo.imageWidth,
				(tileRegion[3] + 1) / (float)frameInfo.imageHeight
			}
		});
	}
}

static bool sendImageRegionData(uint hImage, Display::SharedPtr pDisplay, Renderer::FrameInfo& frameInfo,  AOVPlane::SharedPtr pAOVPlane, std::vector<uint8_t>& textureData) {
	if (!pAOVPlane) return false;
	if (!pDisplay) return false;

	LLOG_DBG << "Reading " << pAOVPlane->name() << " AOV image region data";
	if(!pAOVPlane->getImageData(textureData.data())) {
		LLOG_ERR << "Error reading AOV " << pAOVPlane->name() << " texture data !!!";
		return false;
	}
	LLOG_DBG << "Image data read done!";
	
	auto renderRegionDims = frameInfo.renderRegionDims();
	if (!pDisplay->sendImageRegion(hImage, frameInfo.renderRegion[0], frameInfo.renderRegion[1], renderRegionDims[0], renderRegionDims[1], textureData.data())) {
        LLOG_ERR << "Error sending image region to display !";
        return false;
    }
    return true;
};

static bool sendImageData(uint hImage, Display::SharedPtr pDisplay, AOVPlane::SharedPtr pAOVPlane, std::vector<uint8_t>& textureData) {
	if (!pAOVPlane) return false;
	if (!pDisplay) return false;

	LLOG_DBG << "Reading " << pAOVPlane->name() << " AOV image data";
	if(!pAOVPlane->getImageData(textureData.data())) {
		LLOG_ERR << "Error reading AOV " << pAOVPlane->name() << " texture data !!!";
		return false;
	}
	LLOG_DBG << "Image data read done!";
	
	AOVPlaneGeometry aov_plane_geometry;
	if(!pAOVPlane->getAOVPlaneGeometry(aov_plane_geometry)) {
		return false;
	}

	if (!pDisplay->sendImage(hImage, aov_plane_geometry.width, aov_plane_geometry.height, textureData.data())) {
        LLOG_ERR << "Error sending image to display !";
        return false;
    }
    return true;
};

bool Session::cmdRaytrace() {
	LLOG_DBG << "cmdRaytrace";
	PROFILE(mpDevice, "cmdRaytrace");

	int  sampleUpdateInterval = mpGlobal->getPropertyValue(ast::Style::IMAGE, "sampleupdate", 0);

	Int2 tileSize = mpGlobal->getPropertyValue(ast::Style::IMAGE, "tilesize", Int2{256, 256});
	bool tiled_rendering_mode = mpGlobal->getPropertyValue(ast::Style::IMAGE, "tiling", false);


	auto pMainAOVPlane = mpRenderer->getAOVPlane("MAIN");

	if(!pMainAOVPlane) {
		LLOG_ERR << "NO main AOV plane specified !!!";
		return false;
	}

#ifdef _DEBUG
	mpGlobal->printSummary(std::cout);
#endif

	if(!mpDisplay) {
		mpDisplay = createDisplay(mCurrentDisplayInfo);
		if(!mpDisplay) {
			return false;
		}
	}

	bool doLiveUpdate = mpDisplay->supportsLiveUpdate();

	if (!mpDisplay->supportsLiveUpdate()) {
		// Non interactive display. No need to make intermediate updates
		sampleUpdateInterval = 0;
	}

	// Prepare frame data
	// Set up frame resolution (as they don't have to be the same size)
	Int2 resolution = mpGlobal->getPropertyValue(ast::Style::IMAGE, "resolution", Int2{640, 480});
	mCurrentFrameInfo.imageWidth = resolution[0];
	mCurrentFrameInfo.imageHeight = resolution[1];

	if((mCurrentFrameInfo.imageWidth == 0) || (mCurrentFrameInfo.imageHeight == 0)) {
		LLOG_ERR << "Wrong render frame size: " << to_string(resolution) << " !!!";
		return false;
	}

	// Crop region
	Vector4 crop = mpGlobal->getPropertyValue(ast::Style::IMAGE, "crop", Vector4({0, 0, 0, 0})); // default no crop. houdini crop is: 0-left, 1-right, 2-bottom, 3-top
	
	if((crop[0] > 0.0) || (crop[1] < 1.0) || (crop[2] > 0) || (crop[3] < 1.0)) { 
		mCurrentFrameInfo.renderRegion = {
			uint((float)mCurrentFrameInfo.imageWidth * crop[0]), 
			uint((float)mCurrentFrameInfo.imageHeight * (1.0f - crop[3])), 
			uint((float)mCurrentFrameInfo.imageWidth * crop[1]), 
			uint((float)mCurrentFrameInfo.imageHeight * (1.0f - crop[2]))
		};

		if((mCurrentFrameInfo.imageWidth == 0) || (mCurrentFrameInfo.imageHeight == 0)) {
			LLOG_ERR << "Wrong render image region: " << to_string(crop) << " !!!";
			return false;
		}
	} else {
		// No render region specified so it takes a whole frame
		mCurrentFrameInfo.renderRegion = {0, 0, resolution[0] - 1, resolution[1] - 1};
	}

	// Set up tiles if required or one full frame tile
	std::vector<TileInfo> tiles;
	if (tiled_rendering_mode) {
		LLOG_DBG << "Tiles mode";
		makeImageTiles(mCurrentFrameInfo, to_uint2(tileSize), tiles);
	} else {
		LLOG_DBG << "Full frame mode";
		makeImageTiles(mCurrentFrameInfo, mCurrentFrameInfo.renderRegionDims(), tiles);
	}

	for(const TileInfo& tileInfo: tiles) LLOG_WRN << to_string(tileInfo);

	// Set up image sampling
	mCurrentFrameInfo.imageSamples = mpGlobal->getPropertyValue(ast::Style::IMAGE, "samples", 1);

	// Open display image
	uint hImage;

    mpDisplay->closeAll(); // close previous frame display images (if still opened) 

	std::string imageFileName = mCurrentDisplayInfo.outputFileName;

	// houdini display driver section
	if(mpDisplay->type() == DisplayType::HOUDINI) {
		int houdiniPortNum = mpCurrentScope->getPropertyValue(ast::Style::PLANE, "IPlay.houdiniportnum", int(0)); // Do we need it only for interactive rendeings/IPR ?????
		imageFileName = mpCurrentScope->getPropertyValue(ast::Style::PLANE, "IPlay.rendersource", std::string(mCurrentDisplayInfo.outputFileName));
		if ((houdiniPortNum > 0) && (mIPR)) {
    		// Negative port num is for IPR
    		imageFileName = "iprsocket:" + std::to_string(houdiniPortNum);
    	}
	}

    //

    if(!mpDisplay->openImage(imageFileName, mCurrentFrameInfo.imageWidth, mCurrentFrameInfo.imageHeight, pMainAOVPlane->format(), hImage)) {
        LLOG_FTL << "Unable to open image " << imageFileName << " !!!";
        return false;
    }

	Falcor::ResourceFormat outputResourceFormat;
	uint32_t outputChannelsCount = 0;
	std::vector<uint8_t> textureData;

    // Frame rendering

	setUpCamera(mpRenderer->currentCamera());
    for(const auto& tile: tiles) {
    	LLOG_DBG << "Rendering " << to_string(tile);

    	Renderer::FrameInfo frameInfo = mCurrentFrameInfo;
      	frameInfo.renderRegion = tile.renderRegion;

    	mpRenderer->currentCamera()->setCropRegion(tile.cameraCropRegion);
    	mpRenderer->prepareFrame(frameInfo);

		AOVPlaneGeometry aov_geometry;
		if(!pMainAOVPlane->getAOVPlaneGeometry(aov_geometry)) {
			LLOG_FTL << "No AOV !!!";
			break;
		}

		uint32_t textureDataSize = aov_geometry.width * aov_geometry.height * aov_geometry.bytesPerPixel;
		if (textureData.size() != textureDataSize) textureData.resize(textureDataSize);

		long int sampleUpdateIterations = 0;
		for(uint32_t sample_number = 0; sample_number < mCurrentFrameInfo.imageSamples; sample_number++) {
			mpRenderer->renderSample();
			if (doLiveUpdate && (sampleUpdateInterval > 0)) {
				long int updateIter = ldiv(sample_number, sampleUpdateInterval).quot;
				if (updateIter > sampleUpdateIterations) {
					LLOG_DBG << "Updating display data at sample number " << std::to_string(sample_number);
					if (!sendImageRegionData(hImage, mpDisplay, frameInfo, pMainAOVPlane, textureData)) {
						break;
					}
					sampleUpdateIterations = updateIter;
				}
			}
		}

		LLOG_DBG << "Rendering image samples done !";

		//if (!sendImageRegionData(hImage, mpDisplay, frameInfo, pMainAOVPlane, textureData)) {
		//	break;
		//}

		if (!sendImageData(hImage, mpDisplay, pMainAOVPlane, textureData)) {
			break;
		}
	}

	LLOG_DBG << "Closing display...";
    mpDisplay->closeImage(hImage);
    LLOG_DBG << "Display closed!";

//#ifdef FALCOR_ENABLE_PROFILER
//    auto profiler = Falcor::Profiler::instance(mpDevice);
//    profiler.endFrame();
//#endif

	return true;
}

void Session::pushBgeo(const std::string& name, lsd::scope::Geo::SharedPtr pGeo) {
	LLOG_DBG << "pushBgeo";

#ifdef _DEBUG
    bgeo.printSummary(std::cout);
#endif

    auto pSceneBuilder = mpRenderer->sceneBuilder();
    if(!pSceneBuilder) {
		LLOG_ERR << "Can't push geometry (bgeo). SceneBuilder not ready !!!";
		return;
    }

   	// immediate mesh add
   	ika::bgeo::Bgeo::SharedPtr pBgeo = pGeo->bgeo();
   	std::string fullpath = pGeo->detailFilePath().string();
    pBgeo->readGeoFromFile(fullpath.c_str(), false); // FIXME: don't check version for now

   	if(!pBgeo) {
   		LLOG_ERR << "Can't load geometry (bgeo) !!!";
   		return;
   	}

   	mMeshMap[name] = pSceneBuilder->addGeometry(pBgeo, name);
}

void Session::pushBgeoAsync(const std::string& name, lsd::scope::Geo::SharedPtr pGeo) {
	auto pSceneBuilder = mpRenderer->sceneBuilder();
	if(!pSceneBuilder) {
		LLOG_ERR << "Can't push geometry (bgeo). SceneBuilder not ready !!!";
		return;
	}

	// async mesh add 
   	mMeshMap[name] = pSceneBuilder->addGeometryAsync(pGeo, name);
}

void Session::pushLight(const scope::Light::SharedPtr pLightScope) {
	LLOG_DBG << "pushLight";
	
    auto pSceneBuilder = mpRenderer->sceneBuilder();

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

	if(light_type == "distant") {
		// Directional lights
		auto pDirectionalLight = Falcor::DirectionalLight::create("noname_distant");
		pDirectionalLight->setWorldDirection(light_dir);

		pLight = std::dynamic_pointer_cast<Falcor::Light>(pDirectionalLight);
	} else if (light_type == "sun") {
		// Directional lights
		auto pDistantLight = Falcor::DistantLight::create("noname_sun");
		pDistantLight->setWorldDirection(light_dir);
		
		pLight = std::dynamic_pointer_cast<Falcor::Light>(pDistantLight);
	} else if( light_type == "point") {
		// Point light
		auto pPointLight = Falcor::PointLight::create("noname_point");
		pPointLight->setWorldPosition(light_pos);
		pPointLight->setWorldDirection(light_dir);

		pLight = std::dynamic_pointer_cast<Falcor::Light>(pPointLight);
	} else if( light_type == "grid" || light_type == "disk" || light_type == "sphere") {
		// Area lights
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
		
		if (texture_file_name.size() > 0) {
			bool loadAsSrgb = false;
    		
    		auto pEnvMapTexture = Texture::createFromFile(pDevice, texture_file_name, true, loadAsSrgb);
    		EnvMap::SharedPtr pEnvMap = EnvMap::create(pDevice, pEnvMapTexture);
    		pEnvMap->setTint(light_color);
    		pSceneBuilder->setEnvMap(pEnvMap);
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
	auto pProp = mpCurrentScope->getProperty(style, token);
	if(!mpCurrentScope->getProperty(style, token)) {
		mpCurrentScope->declareProperty(style, valueType(value), token, value, Property::Owner::USER);
	} else {
		mpCurrentScope->setProperty(style, token, value);
	}
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
		LLOG_WRN << "No sub-container for property " << values[0].first;
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

void Session::cmdIPRmode(lsd::ast::IPRMode mode, bool stash) {
	LLOG_ERR << "cmdIPRmode " << to_string(mode) << " stash " << stash << " !!!!!!!!!!!!!!!!!!!!!!!!!!";
	if (!mIPR) mIPR = true;
	//mpRendererIface->setIPRMode(mIPRmode);
}

bool Session::cmdStart(lsd::ast::Style object_type) {
	LLOG_DBG << "cmdStart";

	mpRenderer->init(mRendererConfig);

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

	bool pushGeoAsync = false;// configStore.get<bool>("async_geo", false);
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
			if( pGeo->isInline() || !pushGeoAsync) {
				pushBgeo(pGeo->detailName(), pGeo);
			} else {
				pushBgeoAsync(pGeo->detailName(), pGeo);
			}
			break;
		case ast::Style::OBJECT:
			pObj = std::dynamic_pointer_cast<scope::Object>(mpCurrentScope);
			if(!pushGeometryInstance(pObj)) {
				result = false;
			}
			break;
		case ast::Style::PLANE:
			pPlane = std::dynamic_pointer_cast<scope::Plane>(mpCurrentScope);
			mpRenderer->addAOVPlane(aovInfoFromLSD(pPlane));
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
					//mpRenderer->addMaterialX(std::move(pMaterialX));
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
	auto pMx = Falcor::MaterialX::createUnique(nullptr, material_name);

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
	
	auto pSceneBuilder = mpRenderer->sceneBuilder();
	if (!pSceneBuilder) {
		LLOG_ERR << "Unable to push geometry instance. SceneBuilder not ready !!!";
		return false;
	}

	auto it = mMeshMap.find(pObj->geometryName());
	if(it == mMeshMap.end()) {
		LLOG_ERR << "No geometry found for name " << pObj->geometryName();
		return false;
	}

	std::string obj_name = pObj->getPropertyValue(ast::Style::OBJECT, "name", std::string("unnamed"));

	uint32_t mesh_id = std::numeric_limits<uint32_t>::max();

	try {
		mesh_id = std::get<uint32_t>(it->second);	
		LLOG_DBG << "Getting sync mesh_id for obj name instance: "  << obj_name << " geo name: " << pObj->geometryName();
	} catch (const std::bad_variant_access&) {
		auto& f = std::get<std::shared_future<uint32_t>>(it->second);
		try {
			mesh_id = f.get();
			LLOG_DBG << "Getting async mesh_id for obj_name " << obj_name;	
		} catch(const std::exception& e) {
        	LLOG_ERR << "Async geo instance creation error!!! Exception from the thread: " << e.what();
        	return false;
    	}
	} catch (...) {
		LLOG_ERR << "Async geo instance creation error!!! Unable to get mesh id for object: " << obj_name;
	}

	if (mesh_id == std::numeric_limits<uint32_t>::max()) {
		return false;
	}
	it->second = mesh_id;

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
    std::string 	surface_base_color_texture_path  = "";
    std::string 	surface_base_normal_texture_path = "";
    std::string 	surface_metallic_texture_path    = "";
    std::string 	surface_roughness_texture_path   = "";

    bool 			surface_use_basecolor_texture  = false;
    bool 			surface_use_roughness_texture  = false;
    bool 			surface_use_metallic_texture   = false;
    bool 			surface_use_basenormal_texture = false;

    bool            front_face = false;

    float 		 	surface_ior = 1.5;
    float 			surface_metallic = 0.0;
    float 			surface_roughness = 0.5;
    float 			surface_reflectivity = 1.0;

    Falcor::float3  emissive_color = {0.0, 0.0, 0.0};
    float           emissive_factor = 1.0f;

    if(pShaderProp) {
    	auto pShaderProps = pShaderProp->subContainer();
    	surface_base_color = to_float3(pShaderProps->getPropertyValue(ast::Style::OBJECT, "basecolor", lsd::Vector3{0.2, 0.2, 0.2}));
    	surface_base_color_texture_path = pShaderProps->getPropertyValue(ast::Style::OBJECT, "basecolor_texture", std::string());
    	surface_base_normal_texture_path = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseNormal_texture", std::string());
    	surface_metallic_texture_path = pShaderProps->getPropertyValue(ast::Style::OBJECT, "metallic_texture", std::string());
    	surface_roughness_texture_path = pShaderProps->getPropertyValue(ast::Style::OBJECT, "rough_texture", std::string());

    	surface_use_basecolor_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "basecolor_useTexture", false);
    	surface_use_metallic_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "metallic_useTexture", false);
    	surface_use_roughness_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "rough_useTexture", false);
    	surface_use_basenormal_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseBumpAndNormal_enable", false);

    	surface_ior = pShaderProps->getPropertyValue(ast::Style::OBJECT, "ior", 1.5);
    	surface_metallic = pShaderProps->getPropertyValue(ast::Style::OBJECT, "metallic", 0.0);
    	surface_roughness = pShaderProps->getPropertyValue(ast::Style::OBJECT, "rough", 0.3);
    	surface_reflectivity = pShaderProps->getPropertyValue(ast::Style::OBJECT, "reflect", 1.0);

    	emissive_color = to_float3(pShaderProps->getPropertyValue(ast::Style::OBJECT, "emitcolor", lsd::Vector3{0.0, 0.0, 0.0}));
    	emissive_factor = pShaderProps->getPropertyValue(ast::Style::OBJECT, "emitint", 1.0);

    	front_face = pShaderProps->getPropertyValue(ast::Style::OBJECT, "frontface", false);
    } else {
    	LLOG_ERR << "No surface property set for object " << obj_name;
    }

    auto pMaterial = Falcor::StandardMaterial::create(mpDevice, obj_name);
    pMaterial->setBaseColor(surface_base_color);
    pMaterial->setIndexOfRefraction(surface_ior);
    pMaterial->setMetallic(surface_metallic);
    pMaterial->setRoughness(surface_roughness);
    pMaterial->setReflectivity(surface_reflectivity);
    pMaterial->setEmissiveColor(emissive_color);
    pMaterial->setEmissiveFactor(emissive_factor);
    pMaterial->setDoubleSided(!front_face);
  	
  	//bool loadAsSrgb = true;
    bool loadTexturesAsSparse = !ConfigStore::instance().get<bool>("vtoff", false);

    LLOG_DBG << "Setting " << (loadTexturesAsSparse ? "sparse" : "simple") << " textures for material: " << pMaterial->getName();

    if(surface_base_color_texture_path != "" && surface_use_basecolor_texture) {
    	pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::BaseColor, surface_base_color_texture_path, loadTexturesAsSparse);
    }

    if(surface_metallic_texture_path != "" && surface_use_metallic_texture) {
    	pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::Metallic, surface_metallic_texture_path, loadTexturesAsSparse);
    }

    if(surface_roughness_texture_path != "" && surface_use_roughness_texture) {
    	pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::Roughness, surface_roughness_texture_path, loadTexturesAsSparse);
    }

    if(surface_base_normal_texture_path != "" && surface_use_basenormal_texture) { 
    	pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::Normal, surface_base_normal_texture_path, loadTexturesAsSparse);
    }

    // instance shading spec
    SceneBuilder::MeshInstanceShadingSpec shadingSpec;
    shadingSpec.isMatte = pObj->getPropertyValue(ast::Style::OBJECT, "matte", false);


    // instance visibility spec
    SceneBuilder::MeshInstanceVisibilitySpec visibilitySpec;
    visibilitySpec.visibleToPrimaryRays = pObj->getPropertyValue(ast::Style::OBJECT, "visible_primary", true);
    visibilitySpec.recvShadows = pObj->getPropertyValue(ast::Style::OBJECT, "shadows_recv", true);
    visibilitySpec.castShadows = pObj->getPropertyValue(ast::Style::OBJECT, "shadows_cast", true);

    // add a mesh instance to a node
    pSceneBuilder->addMeshInstance(node_id, mesh_id, pMaterial, &shadingSpec, &visibilitySpec);
    
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
	mCurrentTime = time;
	mpRenderer->init(mRendererConfig);
}


}  // namespace lsd

}  // namespace lava