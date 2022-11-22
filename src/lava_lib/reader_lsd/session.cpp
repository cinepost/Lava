#include <utility>
#include <mutex>
#include <limits>
#include <fstream>
#include <cstdlib>

#include <math.h>


#include "Falcor/Core/API/Texture.h"
#include "Falcor/Scene/Lights/Light.h"
#include "Falcor/Scene/Material/StandardMaterial.h"
#include "Falcor/Scene/MaterialX/MxNode.h"
#include "Falcor/Scene/MaterialX/MaterialX.h"
#include "Falcor/Utils/ConfigStore.h"
#include "Falcor/Utils/Math/FalcorMath.h"

#include "session.h"
#include "session_helpers.h"

#include "../display.h"
#include "../aov.h"
#include "../renderer.h"
#include "../scene_builder.h" 

#include "lava_utils_lib/ut_fsys.h"
#include "lava_utils_lib/ut_string.h"
#include "lava_utils_lib/logging.h"

#include "glm/gtx/string_cast.hpp"


static constexpr float halfC = (float)M_PI / 180.0f;

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
	std::string typeFormatName = mpGlobal->getPropertyValue(ast::Style::IMAGE, "quantize", std::string("float16"));

	mCurrentDisplayInfo.typeFormat = resolveDisplayTypeFormat(typeFormatName);
	return true;
}

void Session::setUpCamera(Falcor::Camera::SharedPtr pCamera, Falcor::float4 cropRegion) {
	assert(pCamera);
	
	// set up camera data
	Vector2 camera_clip = mpGlobal->getPropertyValue(ast::Style::CAMERA, "clip", Vector2{0.01, 1000.0});

	std::string camera_projection_name = mpGlobal->getPropertyValue(ast::Style::CAMERA, "projection", std::string("perspective"));

	auto dims = mCurrentFrameInfo.renderRegionDims();

	float aspect_ratio = static_cast<float>(mCurrentFrameInfo.imageWidth) / static_cast<float>(mCurrentFrameInfo.imageHeight);
	
	pCamera->setAspectRatio(aspect_ratio);
	pCamera->setViewMatrix(mpGlobal->getTransformList()[0]);
	pCamera->setNearPlane(camera_clip[0]);
	pCamera->setFarPlane(camera_clip[1]);
	pCamera->setCropRegion(cropRegion);

	const auto& segments = mpGlobal->segments();
	if(segments.size()) {
		const auto& pSegment = segments[0];

		float 	camera_focus_distance = pSegment->getPropertyValue(ast::Style::CAMERA, "focus", 10000.0f);
		float   camera_fstop = pSegment->getPropertyValue(ast::Style::CAMERA, "fstop", 0.0f);
		float   camera_focal = pSegment->getPropertyValue(ast::Style::CAMERA, "focal", 0.0f);
	
		float apertureRadius = 0.0f;
		{
			float sceneUnit = 1.0f;
			float focalLength = camera_focal;
	 		apertureRadius = apertureFNumberToRadius(camera_fstop, focalLength, sceneUnit);
		}

		pCamera->setFocalDistance(camera_focus_distance);
		pCamera->setApertureRadius(apertureRadius);

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

static bool sendImageData(uint hImage, Display* pDisplay, AOVPlane* pAOVPlane, std::vector<uint8_t>& textureData) {
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

static bool sendImageRegionData(uint hImage, Display* pDisplay, Renderer::FrameInfo& frameInfo,  AOVPlane* pAOVPlane, std::vector<uint8_t>& textureData) {
	if ((frameInfo.imageWidth == frameInfo.regionWidth()) && (frameInfo.imageHeight = frameInfo.regionHeight())) {
		// If sending region that is equal to full frame we just send full image data
		return sendImageData(hImage, pDisplay, pAOVPlane, textureData);
	}
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

bool Session::cmdRaytrace() {
	PROFILE(mpDevice, "cmdRaytrace");

	int  sampleUpdateInterval = mpGlobal->getPropertyValue(ast::Style::IMAGE, "sampleupdate", 0);

	Int2 tileSize = mpGlobal->getPropertyValue(ast::Style::IMAGE, "tilesize", Int2{256, 256});
	bool tiled_rendering_mode = mpGlobal->getPropertyValue(ast::Style::IMAGE, "tiling", false);

	// Rendering passes configuration
	auto& passDict = mpRenderer->getRenderPassesDict();
	passDict["useSTBN"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "stbn_sampling", bool(false));
	passDict["shadingRate"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "shadingrate", int(1));
	passDict["rayBias"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "raybias", 0.001f);
	passDict["colorLimit"] = to_float3(mpGlobal->getPropertyValue(ast::Style::IMAGE, "colorlimit", lsd::Vector3{10.0f, 10.0f, 10.0f}));
	passDict["indirectColorLimit"] = to_float3(mpGlobal->getPropertyValue(ast::Style::IMAGE, "indirectcolorlimit", lsd::Vector3{3.0f, 3.0f, 3.0f}));
	passDict["rayReflectLimit"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "reflectlimit", int(0));
	passDict["rayRefractLimit"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "refractlimit", int(0));
	passDict["rayDiffuseLimit"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "diffuselimit", int(0));
	passDict["areaLightsSamplingMode"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "areasampling", std::string("urena"));

	auto pMainAOVPlane = mpRenderer->getAOVPlane("MAIN").get();

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

	Display* pDisplay = mpDisplay.get();

	bool doLiveUpdate = pDisplay->supportsLiveUpdate();

	if (!pDisplay->supportsLiveUpdate()) {
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

	LLOG_INF << "Rendering image tiles:";
	for(const TileInfo& tileInfo: tiles) LLOG_INF << to_string(tileInfo);

	// Set up image sampling
	mCurrentFrameInfo.imageSamples = mpGlobal->getPropertyValue(ast::Style::IMAGE, "samples", 1);

	// Get renderer aov planes
	std::vector<std::pair<uint, AOVPlane::SharedPtr>> aovPlanes;
	for(auto& entry: mpRenderer->aovPlanes()) {
		if(!entry.second->isMain()) aovPlanes.push_back({0, entry.second});
	}

	// Open display image
	uint hImage;

    if(!mIPR) pDisplay->closeAll(); // close previous frame display images (if still opened) 

	std::string imageFileName = mCurrentDisplayInfo.outputFileName;
	std::string renderLabel = mpGlobal->getPropertyValue(ast::Style::PLANE, "renderlabel", std::string(""));

	{
		std::vector<Display::UserParm> userParams;
	
		// houdini display driver section
		if((pDisplay->type() == DisplayType::HOUDINI) || (pDisplay->type() == DisplayType::MD) || (pDisplay->type() == DisplayType::IP)) {
			int houdiniPortNum = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.houdiniportnum", int(0)); 
			int mplayPortNum = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.socketport", int(0));
			std::string mplayHostname = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.sockethost", std::string("localhost"));
			std::string mplayRendermode = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.rendermode", std::string(""));
			std::string mplayFrange = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.framerange", std::string("1 1"));
			int  mplayCframe = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.currentframe", int(1));

			imageFileName = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.rendersource", std::string(mCurrentDisplayInfo.outputFileName));
			
			if (renderLabel != "") userParams.push_back(Display::makeStringsParameter("label", {renderLabel}));
			
			if (mplayRendermode != "") userParams.push_back(Display::makeStringsParameter("numbering", {mplayRendermode}));

			userParams.push_back(Display::makeStringsParameter("frange", {std::to_string(mplayCframe) + " " + mplayFrange}));
	    	
			if ((mplayPortNum > 0) && mIPR) {
	    		userParams.push_back(Display::makeStringsParameter("remotedisplay", {mplayHostname + ":" + std::to_string(mplayPortNum)}));
	    		imageFileName = "iprsocket:" + std::to_string(mplayPortNum);
	    	}

	    	if (houdiniPortNum > 0) userParams.push_back(Display::makeIntsParameter("houdiniportnum", {houdiniPortNum}));
		}

    	// Open main image plane
    	if(!pDisplay->openImage(imageFileName, mCurrentFrameInfo.imageWidth, mCurrentFrameInfo.imageHeight, pMainAOVPlane->format(), hImage, userParams)) {
        	LLOG_FTL << "Unable to open image " << imageFileName << " !!!";
        	return false;
    	}
	}

    // Open secondary aov image planes
    for(auto& entry: aovPlanes) {
    	auto& pPlane = entry.second;
    	std::string aovImageFileName = imageFileName;
    	std::string channel_prefix = std::string(pPlane->name());

    	std::vector<Display::UserParm> userParams;

    	if(!pDisplay->openImage(aovImageFileName, mCurrentFrameInfo.imageWidth, mCurrentFrameInfo.imageHeight, pPlane->format(), entry.first, userParams, channel_prefix)) {
        	LLOG_ERR << "Unable to open AOV image " << pPlane->name() << " !!!";
    		entry.second = nullptr;
    	}
    }


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
					
					// Send image/region region
					if (!sendImageRegionData(hImage, pDisplay, frameInfo, pMainAOVPlane, textureData)) break;
					sampleUpdateIterations = updateIter;
				}
			}
		}

		LLOG_DBG << "Rendering image samples done !";

		LLOG_DBG << "Sending MAIN output " << std::string(pMainAOVPlane->name()) << " data to image handle " << std::to_string(hImage);
		// Send image region
		if (!sendImageRegionData(hImage, pDisplay, frameInfo, pMainAOVPlane, textureData)) break;
		
		// Send secondary aov image planes data
    	for(auto& entry: aovPlanes) {
    		uint hImage = entry.first;
	    	auto pPlane = entry.second.get();
	    	if (pPlane) {
	    		AOVPlaneGeometry aov_geometry;
				if(!pPlane->getAOVPlaneGeometry(aov_geometry)) {
					LLOG_ERR << "Error getting AOV " << std::string(pPlane->name()) << " geometry!";
					continue;
				}

				uint32_t textureDataSize = aov_geometry.width * aov_geometry.height * aov_geometry.bytesPerPixel;
				std::vector<uint8_t> textureData(textureDataSize);

				LLOG_DBG << "Sending AOV " << std::string(pPlane->name()) << " data to image handle " << std::to_string(hImage);
				
				// Send image region
				if (!sendImageRegionData(hImage, pDisplay, frameInfo, pPlane, textureData)) {
					LLOG_ERR << "Error sending AOV " << std::string(pPlane->name()) << " to display!";
					continue;
				}
	    	}
	    }

		//if (!sendImageData(hImage, pDisplay, pMainAOVPlane, textureData)) {
		//	break;
		//}
	}

	LLOG_DBG << "Closing display...";
    if(!mIPR) pDisplay->closeImage(hImage);

    // Close secondary aov images
	for(auto& entry: aovPlanes) {
		uint hImage = entry.first;
    	auto& pPlane = entry.second;
    	if (pPlane) {
    		if(!mIPR) pDisplay->closeImage(hImage);
    	}
    }

    LLOG_DBG << "Display closed!";

//#ifdef FALCOR_ENABLE_PROFILER
//    auto profiler = Falcor::Profiler::instance(mpDevice);
//    profiler.endFrame();
//#endif

	return true;
}

void Session::pushBgeo(const std::string& name, lsd::scope::Geo::SharedPtr pGeo) {
	
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
	auto pSceneBuilder = mpRenderer->sceneBuilder();

    if (!pSceneBuilder) {
		LLOG_ERR << "Unable to push light. SceneBuilder not ready !!!";
		return;
	}

	const std::string& light_type = pLightScope->getPropertyValue(ast::Style::LIGHT, "type", std::string("point"));
	const std::string& light_name = pLightScope->getPropertyValue(ast::Style::OBJECT, "name", std::string(""));
	glm::mat4 transform = pLightScope->getTransformList()[0];

	lsd::Vector3 light_color = lsd::Vector3{1.0, 1.0, 1.0}; // defualt light color
	Falcor::float3 light_pos = {transform[3][0], transform[3][1], transform[3][2]}; // light position
	Falcor::float3 light_dir = {-transform[2][0], -transform[2][1], -transform[2][2]};
	
	Property* pShaderProp = pLightScope->getProperty(ast::Style::LIGHT, "shader");
	std::shared_ptr<PropertiesContainer> pShaderProps;

	if(pShaderProp) {
		pShaderProps = pShaderProp->subContainer();
		light_color = pShaderProps->getPropertyValue(ast::Style::LIGHT, "lightcolor", lsd::Vector3{1.0, 1.0, 1.0});
	} else {
		LLOG_ERR << "No shader property set for light " << light_name;
	}

	lsd::Vector3 light_diffuse_color = pLightScope->getPropertyValue(ast::Style::LIGHT, "diffuse_color", light_color);
	lsd::Vector3 light_specular_color = pLightScope->getPropertyValue(ast::Style::LIGHT, "specular_color", light_diffuse_color);
	lsd::Vector3 light_indirect_diffuse_color = pLightScope->getPropertyValue(ast::Style::LIGHT, "indirect_diffuse_color", light_diffuse_color);
	lsd::Vector3 light_indirect_specular_color = pLightScope->getPropertyValue(ast::Style::LIGHT, "indirect_specular_color", light_specular_color);

	Falcor::Light::SharedPtr pLight = nullptr;

	if(light_type == "distant") {
		// Directional lights
		auto pDirectionalLight = Falcor::DirectionalLight::create("noname_distant");
		pDirectionalLight->setWorldDirection(light_dir);

		pLight = std::dynamic_pointer_cast<Falcor::Light>(pDirectionalLight);
	} else if (light_type == "sun") {
		// Distant lights
		auto pDistantLight = Falcor::DistantLight::create("noname_sun");
		pDistantLight->setWorldDirection(light_dir);

		const float env_angle = pLightScope->getPropertyValue(ast::Style::LIGHT, "envangle", float(5.0));
		pDistantLight->setAngleDegrees(env_angle);

		pLight = std::dynamic_pointer_cast<Falcor::Light>(pDistantLight);
	} else if( light_type == "point") {
		// Point/Spot light


		float light_radius = pLightScope->getPropertyValue(ast::Style::LIGHT, "lightradius", (float)0.0f);

		auto pPointLight = Falcor::PointLight::create("noname_point");
		pPointLight->setWorldPosition(light_pos);
		pPointLight->setWorldDirection(light_dir);

		if(light_radius > 0.0f) pPointLight->setLightRadius(light_radius);

		bool do_cone = false;
		float coneangle_degrees = 360.0f;
		float conedelta_degrees = 0.0f;

		if(pShaderProps) {
			do_cone = pShaderProps->getPropertyValue(ast::Style::LIGHT, "docone", bool(false));
			coneangle_degrees = pShaderProps->getPropertyValue(ast::Style::LIGHT, "coneangle", (float)360.0f);
			conedelta_degrees = pShaderProps->getPropertyValue(ast::Style::LIGHT, "conedelta", (float)0.0f);
		}

		// Spot light case
		if(do_cone && (coneangle_degrees <= 180.0f)) {
			pPointLight->setOpeningAngle((coneangle_degrees + conedelta_degrees * 2.0f) * halfC);
			pPointLight->setPenumbraHalfAngle(conedelta_degrees * halfC);
		}

		pLight = std::dynamic_pointer_cast<Falcor::Light>(pPointLight);
	} else if( light_type == "grid" || light_type == "disk" || light_type == "sphere") {
		// Area lights
		Falcor::AnalyticAreaLight::SharedPtr pAreaLight = nullptr;

		bool singleSidedLight = pLightScope->getPropertyValue(ast::Style::LIGHT, "singlesided", bool(false));
		bool reverseLight = false;

		lsd::Vector2 area_size = pLightScope->getPropertyValue(ast::Style::LIGHT, "areasize", lsd::Vector2{1.0, 1.0});
		bool area_normalize = pLightScope->getPropertyValue(ast::Style::LIGHT, "areanormalize", bool(true));

		if(pShaderProp) {
			pShaderProps = pShaderProp->subContainer();
			reverseLight = pShaderProps->getPropertyValue(ast::Style::LIGHT, "reverse", bool(false));
		}

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
		pAreaLight->setNormalizeArea(area_normalize);

		pLight = std::dynamic_pointer_cast<Falcor::Light>(pAreaLight);
	} else if( light_type == "env") {
		// Environment light probe is not a classid light source. It should be created later by scene builder or renderer

		std::string texture_file_name = pLightScope->getPropertyValue(ast::Style::LIGHT, "areamap", std::string(""));
		bool phantom = !pLightScope->getPropertyValue(ast::Style::LIGHT, "visible_primary", bool(false));

		auto pDevice = pSceneBuilder->device();
		//LightProbe::SharedPtr pLightProbe;
		
		Texture::SharedPtr pEnvMapTexture;
		if (texture_file_name.size() > 0) {
			bool loadAsSrgb = false;	
    		pEnvMapTexture = Texture::createFromFile(pDevice, texture_file_name, true, loadAsSrgb);
    	}
    	
    	EnvMap::SharedPtr pEnvMap = EnvMap::create(pDevice, pEnvMapTexture);
    	pEnvMap->setTint(to_float3(light_color));
    	pEnvMap->setPhantom(phantom);

    	pSceneBuilder->setEnvMap(pEnvMap);

    	// New EnvironmentLight test
    	auto pEnvLight = EnvironmentLight::create(light_name, pEnvMapTexture);
    	pLight = std::dynamic_pointer_cast<Falcor::Light>(pEnvLight);

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

		pLight->setDiffuseIntensity(to_float3(light_diffuse_color));
		pLight->setSpecularIntensity(to_float3(light_specular_color));
		pLight->setIndirectDiffuseIntensity(to_float3(light_indirect_diffuse_color));
		pLight->setIndirectSpecularIntensity(to_float3(light_indirect_specular_color));
		uint32_t light_id = pSceneBuilder->addLight(pLight);
		mLightsMap[light_name] = light_id;
	}
}

void Session::cmdProperty(lsd::ast::Style style, const std::string& token, const Property::Value& value) {
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
		LLOG_DBG << "No sub-container for property " << values[0].first;
		return;
	}

	for(auto it = values.begin() + 1; it != values.end(); it++) {
		pSubContainer->setProperty(style, it->first, it->second);
	}
}

void Session::cmdEdge(const std::string& src_node_uuid, const std::string& src_node_output_socket, const std::string& dst_node_uuid, const std::string& dst_node_input_socket) {
	auto pNode = std::dynamic_pointer_cast<scope::Node>(mpCurrentScope);
	if(pNode) {
		pNode->addChildEdge(src_node_uuid, src_node_output_socket, dst_node_uuid, dst_node_input_socket);
	}
}

void Session::cmdDeclare(lsd::ast::Style style, lsd::ast::Type type, const std::string& token, const lsd::PropValue& value) {
	if(mpCurrentScope) {
		mpCurrentScope->declareProperty(style, type, token, value.get(), Property::Owner::USER);
	}
}

void Session::cmdTransform(const Matrix4& transform) {
	auto pScope = std::dynamic_pointer_cast<scope::Transformable>(mpCurrentScope);
	if(!pScope) {
		LLOG_DBG << "Trying to set transform on non-transformable scope !!!";
		return;
	}
	pScope->setTransform(transform);
}

void Session::cmdMTransform(const Matrix4& transform) {
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
	LLOG_DBG << "cmdIPRmode " << to_string(mode) << " stash " << stash;
	if (!mIPR) mIPR = true;
	mIPRmode = mode;
}

bool Session::cmdStart(lsd::ast::Style object_type) {
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

	bool pushGeoAsync = mpGlobal->getPropertyValue(ast::Style::GLOBAL, "async_geo", bool(true));
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

	uint32_t exportedInstanceID = SceneBuilder::kInvalidExportedID;
	const Property* pIDProperty = pObj->getProperty(ast::Style::OBJECT, "id");
	if(pIDProperty) {
		exportedInstanceID = pIDProperty->get<uint32_t>();
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

	LLOG_TRC << "mesh_id " << mesh_id;

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
    
    Falcor::float3 	surface_base_color = {0.2, 0.2, 0.2};
    std::string 	surface_base_color_texture_path  = "";
    std::string 	surface_base_normal_texture_path = "";
    std::string 	surface_metallic_texture_path    = "";
    std::string 	surface_roughness_texture_path   = "";
    std::string		surface_emission_texture_path    = "";

    bool 			surface_use_basecolor_texture  = false;
    bool 			surface_use_roughness_texture  = false;
    bool 			surface_use_metallic_texture   = false;
    bool 			surface_use_basenormal_texture = false;
    bool 			surface_use_emission_texture   = false;

    bool            front_face = false;

    float 		 	surface_ior = 1.5;
    float 			surface_metallic = 0.0;
    float 			surface_roughness = 0.3;
    float 			surface_reflectivity = 1.0;

    Falcor::float3  emissive_color = {0.0, 0.0, 0.0};
    float           emissive_factor = 1.0f;

    Falcor::float3  trans_color = {1.0, 1.0, 1.0};
    float           transmission = 0.0f;

    float           ao_distance = 1.0f;

    if(pShaderProp) {
    	auto pShaderProps = pShaderProp->subContainer();
    	surface_base_color = to_float3(pShaderProps->getPropertyValue(ast::Style::OBJECT, "basecolor", lsd::Vector3{0.2, 0.2, 0.2}));
    	surface_base_color_texture_path = pShaderProps->getPropertyValue(ast::Style::OBJECT, "basecolor_texture", std::string());
    	surface_base_normal_texture_path = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseNormal_texture", std::string());
    	surface_metallic_texture_path = pShaderProps->getPropertyValue(ast::Style::OBJECT, "metallic_texture", std::string());
    	surface_roughness_texture_path = pShaderProps->getPropertyValue(ast::Style::OBJECT, "rough_texture", std::string());
    	surface_emission_texture_path = pShaderProps->getPropertyValue(ast::Style::OBJECT, "emitcolor_texture", std::string());

    	surface_use_basecolor_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "basecolor_useTexture", false);
    	surface_use_metallic_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "metallic_useTexture", false);
    	surface_use_roughness_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "rough_useTexture", false);
    	surface_use_emission_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "emitcolor_useTexture", false);
    	surface_use_basenormal_texture = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseBumpAndNormal_enable", false);

    	surface_ior = pShaderProps->getPropertyValue(ast::Style::OBJECT, "ior", 1.5f);
    	surface_metallic = pShaderProps->getPropertyValue(ast::Style::OBJECT, "metallic", 0.0f);
    	surface_roughness = pShaderProps->getPropertyValue(ast::Style::OBJECT, "rough", 0.3f);
    	surface_reflectivity = pShaderProps->getPropertyValue(ast::Style::OBJECT, "reflect", 1.0f);

    	emissive_color = to_float3(pShaderProps->getPropertyValue(ast::Style::OBJECT, "emitcolor", lsd::Vector3{0.0, 0.0, 0.0}));
    	emissive_factor = pShaderProps->getPropertyValue(ast::Style::OBJECT, "emitint", 1.0f);

    	trans_color = to_float3(pShaderProps->getPropertyValue(ast::Style::OBJECT, "transcolor", lsd::Vector3{1.0, 1.0, 1.0}));
    	transmission = pShaderProps->getPropertyValue(ast::Style::OBJECT, "transparency", 0.0f);

    	ao_distance = pShaderProps->getPropertyValue(ast::Style::OBJECT, "ao_distance", 1.0f);

    	front_face = pShaderProps->getPropertyValue(ast::Style::OBJECT, "frontface", false);
    } else {
    	LLOG_ERR << "No surface property set for object " << obj_name;
    }

    std::string material_name = pObj->getPropertyValue(ast::Style::OBJECT, "materialname", std::string(obj_name + "_material"));
    
	Falcor::StandardMaterial::SharedPtr pMaterial = std::dynamic_pointer_cast<Falcor::StandardMaterial>(pSceneBuilder->getMaterial(material_name));
	
	if (!pMaterial) {
		// It's the first time material declaration or instance default material that should be resolved to instanced object material instead 
		pMaterial = Falcor::StandardMaterial::create(mpDevice, material_name);
	}

	if (pMaterial) {
	    pMaterial->setBaseColor(surface_base_color);
	    pMaterial->setIndexOfRefraction(surface_ior);
	    pMaterial->setMetallic(surface_metallic);
	    pMaterial->setRoughness(surface_roughness);
	    pMaterial->setReflectivity(surface_reflectivity);
	    pMaterial->setEmissiveColor(emissive_color);
	    pMaterial->setEmissiveFactor(emissive_factor);
	    pMaterial->setAODistance(ao_distance);
	    pMaterial->setDoubleSided(!front_face);

	    pMaterial->setTransmissionColor(trans_color);
	    pMaterial->setSpecularTransmission(transmission);

	  	//bool loadAsSrgb = true;
	    bool loadTexturesAsSparse = !mpGlobal->getPropertyValue(ast::Style::GLOBAL, "vtoff", bool(false));

	    LLOG_DBG << "Setting " << (loadTexturesAsSparse ? "sparse" : "simple") << " textures for material: " << pMaterial->getName();

	    if(surface_base_color_texture_path != "" && surface_use_basecolor_texture) {
	    	pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::BaseColor, surface_base_color_texture_path, loadTexturesAsSparse);
	    }

	    if(surface_metallic_texture_path != "" && surface_use_metallic_texture) {
	    	pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::Metallic, surface_metallic_texture_path, loadTexturesAsSparse);
	    }

	    if(surface_emission_texture_path != "" && surface_use_emission_texture) { 
	    	pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::Emissive, surface_emission_texture_path, loadTexturesAsSparse);
	    }

	    if(surface_roughness_texture_path != "" && surface_use_roughness_texture) {
	    	pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::Roughness, surface_roughness_texture_path, loadTexturesAsSparse);
	    }

	    if(surface_base_normal_texture_path != "" && surface_use_basenormal_texture) { 
	    	pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::Normal, surface_base_normal_texture_path, loadTexturesAsSparse);
	    }
	}

    // instance shading spec
    SceneBuilder::MeshInstanceShadingSpec shadingSpec;
    shadingSpec.isMatte = pObj->getPropertyValue(ast::Style::OBJECT, "matte", false);
    shadingSpec.fixShadowTerminator = pObj->getPropertyValue(ast::Style::OBJECT, "fix_shadow", true);
    shadingSpec.biasAlongNormal = pObj->getPropertyValue(ast::Style::OBJECT, "biasnormal", false);
    shadingSpec.doubleSided = pObj->getPropertyValue(ast::Style::OBJECT, "double_sided", true);

    // instance visibility spec
    SceneBuilder::MeshInstanceVisibilitySpec visibilitySpec;
    visibilitySpec.visibleToPrimaryRays = pObj->getPropertyValue(ast::Style::OBJECT, "visible_primary", true);
    visibilitySpec.visibleToShadowRays = pObj->getPropertyValue(ast::Style::OBJECT, "visible_shadows", true);
    visibilitySpec.visibleToDiffuseRays = pObj->getPropertyValue(ast::Style::OBJECT, "visible_diffuse", true);
    visibilitySpec.visibleToReflectionRays = pObj->getPropertyValue(ast::Style::OBJECT, "visible_reflect", true);
    visibilitySpec.visibleToRefractionRays = pObj->getPropertyValue(ast::Style::OBJECT, "visible_refract", true);
    visibilitySpec.receiveShadows = pObj->getPropertyValue(ast::Style::OBJECT, "receive_shadows", true);
    
    // add a mesh instance to a node
    pSceneBuilder->addMeshInstance(node_id, mesh_id, exportedInstanceID, pMaterial, &shadingSpec, &visibilitySpec);
    
	return true;
}


bool Session::cmdGeometry(const std::string& name) {
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