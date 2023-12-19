#include <utility>
#include <mutex>
#include <limits>
#include <fstream>
#include <cstdlib>

#include <math.h>

#include "Falcor/Core/API/Texture.h"
#include "Falcor/Scene/Lights/Light.h"

#include "Falcor/Scene/Material/StandardMaterial.h"
#include "Falcor/Scene/Material/MaterialTypes.slang"

#include "Falcor/Scene/MaterialX/MxNode.h"
#include "Falcor/Scene/MaterialX/MaterialX.h"

#include "Falcor/Utils/ConfigStore.h"
#include "Falcor/Utils/Math/FalcorMath.h"
#include "Falcor/Utils/Timing/TimeReport.h"

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
static constexpr uint32_t kMaxMeshID = std::numeric_limits<uint32_t>::max();
static constexpr uint32_t kMaxNodeID = std::numeric_limits<uint32_t>::max();

namespace lava {

namespace lsd {

using DisplayType = Display::DisplayType;


static std::string random_string( size_t length ) {
  auto randchar = []() -> char
  {
      const char charset[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
      const size_t max_index = (sizeof(charset) - 1);
      return charset[ rand() % max_index ];
  };
  std::string str(length,0);
  std::generate_n( str.begin(), length, randchar );
  return str;
}

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
#define get_bool(_a) (boost::get<int>(_a) == 0) ? false : true

	if (name == "vtoff") { mRendererConfig.useVirtualTexturing = get_bool(value); return; }
	if (name == "fconv") { mRendererConfig.forceVirtualTexturesReconversion = get_bool(value); return; }
	if (name == "async_geo") { mRendererConfig.useAsyncGeometryProcessing = get_bool(value); return; }
	if (name == "generate_meshlets") { mRendererConfig.generateMeshlets = get_bool(value); return; }
	if (name == "cull_mode") { mRendererConfig.cullMode = boost::get<std::string>(value); return; }
	if (name == "vtex_conv_quality") { mRendererConfig.virtualTexturesCompressionQuality = boost::get<std::string>(value); return; }
	if (name == "vtex_tlc") { mRendererConfig.virtualTexturesCompressorType = boost::get<std::string>(value); return; }
	if (name == "vtex_tlc_level") { mRendererConfig.virtualTexturesCompressionLevel = (uint8_t)boost::get<int>(value); return; }
	if (name == "geo_tangent_generation") { mRendererConfig.tangentGenerationMode = boost::get<std::string>(value); return; }

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

	const std::string camera_projection_name = mpGlobal->getPropertyValue(ast::Style::CAMERA, "projection", std::string("perspective"));
	const std::string camera_background_image_name = mpGlobal->getPropertyValue(ast::Style::CAMERA, "backgroundimage", std::string());
	Vector4 camera_background_color = mpGlobal->getPropertyValue(ast::Style::CAMERA, "backgroundcolor", Vector4{0.0, 0.0, 0.0, 0.0});

	auto dims = mCurrentFrameInfo.renderRegionDims();

	float aspect_ratio = static_cast<float>(mCurrentFrameInfo.imageWidth) / static_cast<float>(mCurrentFrameInfo.imageHeight);
	
	pCamera->setAspectRatio(aspect_ratio);
	pCamera->setViewMatrix(mpGlobal->getTransformList()[0]);
	pCamera->setNearPlane(camera_clip[0]);
	pCamera->setFarPlane(camera_clip[1]);
	pCamera->setCropRegion(cropRegion);
	pCamera->setBackgroundImageFilename(camera_background_image_name);
	pCamera->setBackgroundColor(to_float4(camera_background_color));

	const auto& segments = mpGlobal->segments();
	if(segments.size()) {
		const auto& pSegment = segments[0];

		const float camera_focus_distance = pSegment->getPropertyValue(ast::Style::CAMERA, "focus", 10000.0f);
		const float camera_fstop = pSegment->getPropertyValue(ast::Style::CAMERA, "fstop", 5.6f);
		const float camera_focal = pSegment->getPropertyValue(ast::Style::CAMERA, "focal", 50.0f);
		const bool use_dof = pSegment->getPropertyValue(ast::Style::IMAGE, "usedof", bool(false));

		float apertureRadius = 0.0f;
		if (use_dof) {
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
	cleanup();

	mpRenderer = nullptr;
  mpDevice = nullptr;
}

void Session::cleanup() {
// Remove temporary geometries from filesystem
  const size_t temporary_geometries_count = mTemporaryGeometriesPaths.size();
  if(!mTemporaryGeometriesPaths.empty()) {
    for(auto const& fullpath: mTemporaryGeometriesPaths) {
      boost::system::error_code ec;
      bool retval = fs::remove_all(fullpath, ec);
      if(!ec) {  // success
        if(retval) {
          LLOG_DBG << "Removed temporary geometry file " << fullpath;
        } else {
          LLOG_DBG << "Temporary geometry file " << fullpath << " already deleted. All ok!";
        }
      } else {  // error removing temp file
        LLOG_ERR << "Error removing temporary geometry file " << fullpath;
      }
    }
  }
}

void Session::cmdReset() {
	auto pSceneBuilder = mpRenderer->sceneBuilder();
  if(!pSceneBuilder) return;

  pSceneBuilder->freeTemporaryResources();
}

bool Session::cmdRaytrace() {
	PROFILE(mpDevice, "cmdRaytrace");

	// Set up image sampling
	const int imageSamples = mCurrentFrameInfo.imageSamples = mpGlobal->getPropertyValue(ast::Style::IMAGE, "samples", 1);
	int  sampleUpdateInterval = mpGlobal->getPropertyValue(ast::Style::IMAGE, "sampleupdate", 0);

	const Int2 tileSize = mpGlobal->getPropertyValue(ast::Style::IMAGE, "tilesize", Int2{256, 256});
	const bool tiled_rendering_mode = mpGlobal->getPropertyValue(ast::Style::IMAGE, "tiling", false);

	// Renderer and it's rendergraph configuration
	auto& confDict = mpRenderer->getRendererConfDict();
	confDict["primaryraygentype"] = mpGlobal->getPropertyValue(ast::Style::RENDERER, "primaryraygentype", std::string("hwraster"));
	confDict["shadingpasstype"] = mpGlobal->getPropertyValue(ast::Style::RENDERER, "shadingpasstype", std::string("deferred"));

	// Rendering passes configuration
	auto& passDict = mpRenderer->getRenderPassesDict();

  passDict["russRoulleteLevel"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "rrouletlevel", int(2));
  passDict["rayContribThreshold"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "raythreshold", float(0.1f));
	passDict["useDOF"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "usedof", bool(false));
	passDict["useSTBN"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "stbn_sampling", bool(false));
	passDict["shadingRate"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "shadingrate", int(1));

	passDict["asyncLtxLoading"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "asyncltxloading", bool(true));

	passDict["rayBias"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "raybias", 0.001f);
	passDict["colorLimit"] = to_float3(mpGlobal->getPropertyValue(ast::Style::IMAGE, "colorlimit", lsd::Vector3{10.0f, 10.0f, 10.0f}));
	passDict["indirectColorLimit"] = to_float3(mpGlobal->getPropertyValue(ast::Style::IMAGE, "indirectcolorlimit", lsd::Vector3{3.0f, 3.0f, 3.0f}));
	passDict["rayReflectLimit"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "reflectlimit", int(0));
	passDict["rayRefractLimit"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "refractlimit", int(0));
	passDict["rayDiffuseLimit"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "diffuselimit", int(0));
	passDict["areaLightsSamplingMode"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "areasampling", std::string("urena"));

	passDict["MAIN.ToneMappingPass.enable"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "ToneMappingPass.enable", bool(false));
	passDict["MAIN.ToneMappingPass.operator"] = (uint32_t)mpGlobal->getPropertyValue(ast::Style::IMAGE, "ToneMappingPass.operator", int(4));

	passDict["MAIN.ToneMappingPass.filmSpeed"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "ToneMappingPass.filmSpeed", float(100.0));
	passDict["MAIN.ToneMappingPass.exposureValue"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "ToneMappingPass.exposureValue", float(0.0));
	passDict["MAIN.ToneMappingPass.autoExposure"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "ToneMappingPass.autoExposure", bool(false));

	passDict["MAIN.OpenDenoisePass.enable"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "OpenDenoisePass.enable", bool(false));
	passDict["MAIN.OpenDenoisePass.useAlbedo"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "OpenDenoisePass.useAlbedo", bool(true));
	passDict["MAIN.OpenDenoisePass.useNormal"] = mpGlobal->getPropertyValue(ast::Style::IMAGE, "OpenDenoisePass.useNormal", bool(true));

	auto pMainOutputPlane = mpRenderer->getAOVPlane("MAIN");

	if(!pMainOutputPlane) {
		LLOG_ERR << "No main output plane exists !";
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
	};

	if (!mpDisplay->isInteractive()) {
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
	LLOG_INF << "Image resolution: " << resolution[0] << "x" << resolution[1];
	for(const TileInfo& tileInfo: tiles) LLOG_INF << to_string(tileInfo);

	// Get renderer aov planes
	std::vector<std::pair<uint, AOVPlane::SharedPtr>> aovPlanes;
	for(auto& entry: mpRenderer->aovPlanes()) {
		if(!entry.second->isMain()) aovPlanes.push_back({0, entry.second});
	}

	// Open display image
	uint hImage;

  if(!mIPR) mpDisplay->closeAll(); // close previous frame display images (if still opened) 

	std::string imageFileName = mCurrentDisplayInfo.outputFileName;
	std::string renderLabel = mpGlobal->getPropertyValue(ast::Style::RENDERER, "renderlabel", std::string(""));

	bool clearDisplayBeforeRendering = false;

	// In case display supports metadata output and there are planes that has metadata we need to open images
	// after the rendring process is completed as there is no mechanism to such metadata after image already opened
	std::vector<std::function<bool()>> delayedImageOpens; // Tasks to open display images if requested to be opened later

	TimeReport initDisplayTimeReport;

	{
		std::vector<Display::UserParm> userParams;
	
		// houdini display driver section
		if((mpDisplay->type() == DisplayType::HOUDINI) || (mpDisplay->type() == DisplayType::MD) || (mpDisplay->type() == DisplayType::IP)) {
			clearDisplayBeforeRendering = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.zeroimage", bool(true)) && !mIPR;
			const int houdiniPortNum = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.houdiniportnum", int(0)); 
			const int mplayPortNum = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.socketport", int(0));
			const std::string mplayHostname = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.sockethost", std::string("localhost"));
			const std::string mplayRendermode = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.rendermode", std::string(""));
			//std::string mplayLabel = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.label", renderLabel);
			const std::string mplayFrange = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.framerange", std::string("1 1"));
			const int mplayCframe = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.currentframe", int(1));

			imageFileName = mpGlobal->getPropertyValue(ast::Style::PLANE, "IPlay.rendersource", std::string(mCurrentDisplayInfo.outputFileName));

			userParams.push_back(Display::makeStringsParameter("label", {renderLabel}));

			if (mplayRendermode != "") userParams.push_back(Display::makeStringsParameter("numbering", {mplayRendermode}));

			userParams.push_back(Display::makeStringsParameter("frange", {std::to_string(mplayCframe) + " " + mplayFrange}));
	    	
			if ((mplayPortNum > 0) && mIPR) {
	    	userParams.push_back(Display::makeStringsParameter("remotedisplay", {mplayHostname + ":" + std::to_string(mplayPortNum)}));
	    	imageFileName = "iprsocket:" + std::to_string(mplayPortNum);
	    }

	    if (houdiniPortNum > 0) userParams.push_back(Display::makeIntsParameter("houdiniportnum", {houdiniPortNum}));
		}

    	// Open main image plane
    	const bool delayedMainImageFileCreation = mpDisplay->supportsMetaData();
    	auto _openMainImage = [this, imageFileName, userParams, pMainOutputPlane, &hImage]() {
    		bool result = false;
    		Falcor::ResourceFormat format = pMainOutputPlane->format();
    		if(pMainOutputPlane->hasMetaData()) {
    			auto metaData = pMainOutputPlane->getMetaData();	
    			result = mpDisplay->openImage(imageFileName, mCurrentFrameInfo.imageWidth, mCurrentFrameInfo.imageHeight, format, hImage, userParams, "C", &metaData);
      	} else {
      		result = mpDisplay->openImage(imageFileName, mCurrentFrameInfo.imageWidth, mCurrentFrameInfo.imageHeight, format, hImage, userParams, "C");
      	}
    		if(!result) LLOG_FTL << "Unable to open main output plane image !!!";
    		return result;
    	};

    	if(delayedMainImageFileCreation) {
    		delayedImageOpens.push_back(_openMainImage);
    	} else {
    		if(!_openMainImage()) return false;
    	}
    	initDisplayTimeReport.measure("Display main image open");
	}

	// Auxiliary image planes
	for(auto& entry: aovPlanes) {
    	auto& pPlane = entry.second;
    	if(!pPlane->isEnabled()) continue;
    	
    	const std::string aovImageFileName = (pPlane->filename() != "") ? pPlane->filename() : imageFileName;
    	std::vector<Display::UserParm> userParams;
    	userParams.push_back(Display::makeStringsParameter("label", {renderLabel}));
    	auto pPlaneDisplay = pPlane->hasDisplay() ? pPlane->getDisplay() : mpDisplay;
    	const bool delayedImagFileCreation = pPlaneDisplay->supportsMetaData();

    	auto _openImage = [this, aovImageFileName, userParams, pPlaneDisplay, pPlane, &entry]() {
    		bool result = false;
    		const std::string channel_prefix = pPlane->outputName();
    		const Falcor::ResourceFormat format = pPlane->format();
    		if(pPlane->hasMetaData()) {
    			auto metaData = pPlane->getMetaData();
    			result = pPlaneDisplay->openImage(aovImageFileName, mCurrentFrameInfo.imageWidth, mCurrentFrameInfo.imageHeight, format, entry.first, userParams, channel_prefix, &metaData); 
    		} else {
    			result = pPlaneDisplay->openImage(aovImageFileName, mCurrentFrameInfo.imageWidth, mCurrentFrameInfo.imageHeight, format, entry.first, userParams, channel_prefix);
    		}
    		if(!result) LLOG_ERR << "Unable to open image file " << aovImageFileName << " for output plane " << channel_prefix << " !!!";
    		return result;
    	};

    	if(delayedImagFileCreation) {
    		delayedImageOpens.push_back(_openImage);
    	} else {
    		if(!_openImage()) entry.second = nullptr;
    	}
    }
    initDisplayTimeReport.measure("Display additional images open");

///////
	if( 1 == 2 ) {
		// Simple performace test by sending zero image a bunch of times
    	for(uint i = 0; i < 100; i++) sendImageRegionData(hImage, mpDisplay.get(), mCurrentFrameInfo, nullptr);
    	initDisplayTimeReport.measure("Display initial zero data sent 100 times in");
	}
///////

  initDisplayTimeReport.addTotal("Display init total time");
  initDisplayTimeReport.printToLog();

  // Frame rendering
  TimeReport renderingTimeReport;
	
	LLOG_INF << "Rendering image started...";
	setUpCamera(mpRenderer->currentCamera());
  
  for(const auto& tile: tiles) {
  	LLOG_DBG << "Rendering " << to_string(tile);

  	Renderer::FrameInfo frameInfo = mCurrentFrameInfo;
    frameInfo.renderRegion = tile.renderRegion;

  	mpRenderer->currentCamera()->setCropRegion(tile.cameraCropRegion);
  	mpRenderer->prepareFrame(frameInfo);

		AOVPlaneGeometry aov_geometry;
		if(!pMainOutputPlane->getAOVPlaneGeometry(aov_geometry)) {
			LLOG_FTL << "No AOV !!!";
			break;
		}

		// It there was request to fill image with zero data before the rendering starts, we have to check out configuration meets certain criteria
		// First fo all there should be no additional displays that refers to the same image as main output plane driver (mpDisplay) that are postponed
		// to be opened later (after frame is rendered). Aslo we skip this step if the main output driver is not interactive, e.g it writes to a filesystem
		// as there is no benifit in doing this.
		// TODO: mark displays that we are going to open later and check they are writing to the same image as main mpDisplay!
		if(clearDisplayBeforeRendering && mpDisplay->isInteractive()) {
			sendImageRegionData(hImage, mpDisplay.get(), frameInfo, nullptr);
		}

		long int sampleUpdateIterations = 0;
		const bool doInteractiveImageUpdates = (sampleUpdateInterval > 0) && mpDisplay->isInteractive() && mpDisplay->opened(hImage);

		for(uint32_t sample_number = 0; sample_number < mCurrentFrameInfo.imageSamples; sample_number++) {
			mpRenderer->renderSample();
			if (doInteractiveImageUpdates) {
				long int updateIter = ldiv(sample_number, sampleUpdateInterval).quot;
				if (updateIter > sampleUpdateIterations) {
					LLOG_DBG << "Updating display data at sample number " << std::to_string(sample_number);
					// Send image/region region for interactive/live image update
					if (!sendImageRegionData(hImage, mpDisplay.get(), frameInfo, pMainOutputPlane.get())) break;
					sampleUpdateIterations = updateIter;
				}
			}
		}

		mpRenderer->device()->getRenderContext()->flush(true);
		renderingTimeReport.measure("Image rendering time");
		LLOG_INF << renderingTimeReport.printToString();
		
		// Open delayed images 
		LLOG_DBG << "Open " << delayedImageOpens.size() << " delayed images.";
		for(size_t i = 0; i < delayedImageOpens.size(); i++) {
			delayedImageOpens[i]();
		}

		LLOG_DBG << "Sending MAIN output " << std::string(pMainOutputPlane->name()) << " data to image handle " << std::to_string(hImage);
		// Send image region
		if (!sendImageRegionData(hImage, mpDisplay.get(), frameInfo, pMainOutputPlane.get())) break;
	
		// Send secondary aov image planes data
  	for(auto& entry: aovPlanes) {
  		uint hImage = entry.first;
    	auto& pPlane = entry.second;
    	if (pPlane && pPlane->isEnabled()) {
    		AOVPlaneGeometry aov_geometry;
				if(!pPlane->getAOVPlaneGeometry(aov_geometry)) {
					LLOG_ERR << "Error getting AOV " << std::string(pPlane->name()) << " geometry!";
					continue;
				}

				LLOG_DBG << "Sending AOV " << std::string(pPlane->name()) << " data to image handle " << std::to_string(hImage);
				// Send image region
				auto pDisplay = pPlane->hasDisplay() ? pPlane->getDisplay() : mpDisplay;
				if (!sendImageRegionData(hImage, pDisplay.get(), frameInfo, pPlane.get())) {
					LLOG_ERR << "Error sending AOV " << std::string(pPlane->name()) << " to display!";
					continue;
				}
    	}
    }
	}

	LLOG_DBG << "Closing display...";
  if(!mIPR) mpDisplay->closeImage(hImage);

  // Close secondary aov images
	for(auto& entry: aovPlanes) {
	uint hImage = entry.first;
  	auto& pPlane = entry.second;
  	if (pPlane) {
  		auto pDisplay = pPlane->hasDisplay() ? pPlane->getDisplay() : mpDisplay;
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
	assert(pGeo);

  auto pSceneBuilder = mpRenderer->sceneBuilder();
  if(!pSceneBuilder) {
		LLOG_ERR << "Unable to push geometry \"" << name << "\" (bgeo). SceneBuilder not ready !!!";
		return;
  }

 	// immediate mesh add
 	ika::bgeo::Bgeo::SharedPtr pBgeo = pGeo->bgeo();
 	std::string fullpath = pGeo->detailFilePath().string();
  pBgeo->readGeoFromFile(fullpath.c_str(), false); // FIXME: don't check version for now

 	if(!pBgeo) {
 		LLOG_ERR << "Unable to load \"" << name << "\" geometry (bgeo) !!!";
 		return;
 	}

#ifdef _DEBUG
    pBgeo->printSummary(std::cout);
#endif

  pSceneBuilder->addGeometry(pBgeo, name);
}

void Session::pushBgeoAsync(const std::string& name, lsd::scope::Geo::SharedPtr pGeo) {
	assert(pGeo);

	auto pSceneBuilder = mpRenderer->sceneBuilder();
	if(!pSceneBuilder) {
		LLOG_ERR << "Can't push geometry (bgeo). SceneBuilder not ready !!!";
		return;
	}

	// async mesh add 
  pSceneBuilder->addGeometryAsync(pGeo, name);
}

void Session::pushLight(const scope::Light::SharedPtr pLightScope) {
	assert(pLightScope);

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

	const bool visible_primary = pLightScope->getPropertyValue(ast::Style::LIGHT, "visible_primary", bool(false));

	const bool update = (mIPRmode == ast::IPRMode::UPDATE);

	Falcor::Light::SharedPtr pLight = update ? pSceneBuilder->getLight(light_name) : nullptr;

	if(light_type == "distant") {
		// Directional light

		auto pDirectionalLight = Falcor::DirectionalLight::create("noname_distant");
		pDirectionalLight->setWorldDirection(light_dir);

		pLight = std::dynamic_pointer_cast<Falcor::Light>(pDirectionalLight);
	} else if (light_type == "sun") {
		// Distant/Sun light

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
		bool is_physical_sky = pLightScope->getPropertyValue(ast::Style::LIGHT, "physical_sky", bool(false));

		auto pDevice = pSceneBuilder->device();
		
		Texture::SharedPtr pEnvMapTexture;
		if (!is_physical_sky && texture_file_name.size() > 0) {
			bool loadAsSRGB = false;
			bool loadAsSparse = false;
			bool generateMipLevels = true;
			Resource::BindFlags bindFlags = Resource::BindFlags::ShaderResource;
			std::string udimMask = "<UDIM>";	
    	pEnvMapTexture = pDevice->textureManager()->loadTexture(texture_file_name, generateMipLevels, loadAsSRGB, bindFlags, udimMask, loadAsSparse);
    }
    	
  	// New EnvironmentLight test
  	if(is_physical_sky) {
  		auto pEnvLight = PhysicalSunSkyLight::create(light_name);

  		pEnvLight->setDevice(pDevice);
  		LLOG_WRN << "Physical Sky Ligth build " << (pEnvLight->buildTest() ? "done!" : "failed!" );
  		pLight = std::dynamic_pointer_cast<Falcor::Light>(pEnvLight);
  	} else {
  		auto pEnvLight = EnvironmentLight::create(light_name, pEnvMapTexture);
  		pEnvLight->setTransformMatrix(transform);
			
  		if(pEnvMapTexture) pEnvLight->setTexture(pEnvMapTexture);
  		pLight = std::dynamic_pointer_cast<Falcor::Light>(pEnvLight);
  	}

	} else { 
		LLOG_WRN << "Unsupported light type " << light_type << ". Skipping...";
		return;
	}

	if(pLight) {
		LLOG_DBG << "Light " << light_name << "  type " << Falcor::to_string(pLight->getData().getLightType());

		if (light_name != "") {
			pLight->setName(light_name);
		}

		pLight->setHasAnimation(false);
		pLight->setCameraVisibility(visible_primary);

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

		if(update && pLight) {
			LLOG_INF << "Update light " << light_name;
			pSceneBuilder->updateLight(light_name, *pLight.get());
		} else {
			uint32_t light_id = pSceneBuilder->addLight(pLight);
			mLightsMap[light_name] = light_id;
		}
	}
}

void Session::cmdProperty(lsd::ast::Style style, const std::string& token, const Property::Value& value) {
	if(!mpCurrentScope) {
		LLOG_ERR << "No current scope is set !!!";
		return; 
	}
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

void Session::cmdProcedural(const std::string& procedural, const Vector3& bbox_min, const Vector3& bbox_max, const std::map<std::string, Property::Value>& arguments) {
	if(procedural == "ptinstance") {

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

void Session::cmdDelete(lsd::ast::Style style, const std::string& name) {
	auto pSceneBuilder = mpRenderer->sceneBuilder();
  if(!pSceneBuilder) return;

	switch (style) {
		case lsd::ast::Style::OBJECT:
			pSceneBuilder->deleteMeshInstance(name);
		default:
			break;
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
	if (!mIPR && (mode == ast::IPRMode::GENERATE || mode == ast::IPRMode::UPDATE)) {
		mRendererConfig.optimizeForIPR = mIPR = true;
	}
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

	return mpCurrentScope ? true : false;
}

bool Session::cmdEnd() {
	auto pGlobalScope = std::dynamic_pointer_cast<scope::Global>(mpCurrentScope);
	if(pGlobalScope) {
		LLOG_FTL << "Can't end global scope !!!";
		return false;
	}

	auto pParentScope = mpCurrentScope->parent();
	if(!pParentScope) {
		LLOG_FTL << "Unable to end scope with no parent !!!";
		return false;
	}

	const auto& configStore = Falcor::ConfigStore::instance();

	bool result = true;

	switch(mpCurrentScope->type()) {
		case ast::Style::GEO:
			{
				scope::Geo::SharedPtr pScopeGeo = std::dynamic_pointer_cast<scope::Geo>(mpCurrentScope);
				if(!pScopeGeo) {
					LLOG_ERR << "Error ending scope of type \"geometry\"!!!";
					return false;
				}

				// Check if we are in IPR update mode. If so and geometry is being flagged as temporary we should ignore it.
				// We also want to check that geometry was already pushed to scene builder during IPR generate mode.
				if(pScopeGeo->isTemporary() && mIPRmode == ast::IPRMode::UPDATE) {
					auto pSceneBuilder = mpRenderer->sceneBuilder();
					if (!pSceneBuilder) {
						return false;
					}

					if (pSceneBuilder->meshExist(pScopeGeo->detailName())) {
						// mesh already exist. all ok
						break;
					} else {
						LLOG_WRN << "Mesh " << pScopeGeo->detailName() << " doesn't exit. Pushing...";
					}
				}

				// If temporary bgeo, mark it so we can delete it later
				if(pScopeGeo->isTemporary() && !pScopeGeo->isInline()) {
          mTemporaryGeometriesPaths.insert(pScopeGeo->detailFilePath().string());
        }

				bool pushGeoAsync = mpGlobal->getPropertyValue(ast::Style::GLOBAL, "async_geo", bool(true));
				if( pScopeGeo->isInline() || !pushGeoAsync) {
					pushBgeo(pScopeGeo->detailName(), pScopeGeo);
				} else {
					pushBgeoAsync(pScopeGeo->detailName(), pScopeGeo);
				}
			}
			break;
		case ast::Style::OBJECT:
			{
				scope::Object::SharedPtr pScopeObj = std::dynamic_pointer_cast<scope::Object>(mpCurrentScope);
				if(!pScopeObj) {
					LLOG_ERR << "Error ending scope of type \"object\"!!!";
					return false;
				}

				if(pScopeObj->geometryName().empty()) {
					LLOG_WRN << "Empty geometry instance skipped.";
					break;
				}

				const bool update = (mIPRmode == ast::IPRMode::UPDATE) ? true : false;
				if(!pushGeometryInstance(pScopeObj, update)) {
					LLOG_WRN << "Error " << (update ? "updating" : "creating") << " geometry " << pScopeObj->geometryName() << " instance !";
					setFailed(true);
					return false;
				}
			}
			break;
		case ast::Style::PLANE:
			{
				scope::Plane::SharedPtr pScopePlane = std::dynamic_pointer_cast<scope::Plane>(mpCurrentScope);
				auto pPlane = mpRenderer->addAOVPlane(aovInfoFromLSD(pScopePlane), mIPR);
				
				// Check if output plane wants own display for image output
				const std::string& display_filename = pPlane->filename();
				if(display_filename != "" && !pPlane->hasDisplay()) {
					Display::SharedPtr pDisplay;
					if (mDisplays.find(display_filename) != mDisplays.end()) {
						pDisplay = mDisplays[display_filename];
					}else {
						DisplayInfo createDisplayInfo;
						createDisplayInfo.displayType = resolveDisplayTypeByFileName(display_filename);
						mDisplays[display_filename] = pDisplay = createDisplay(createDisplayInfo);
					}
					if(!pDisplay) {
						LLOG_FTL << "Output plane " << pPlane->name() << " needs own display but has none!";
						return false;
					}
					pPlane->setDisplay(pDisplay);
				}
				
				if (!pPlane) {
					LLOG_FTL << "Error creating output plane " << pPlane->name();
					return false;
				}
				translateLSDPlanePropertiesToLavaDict(pScopePlane, pPlane->getRenderPassesDict());
			}
			break;
		case ast::Style::LIGHT:
			{
				scope::Light::SharedPtr pScopeLight = std::dynamic_pointer_cast<scope::Light>(mpCurrentScope);
				if(!pScopeLight) {
					LLOG_ERR << "Error ending scope of type \"light\"!!!";
					return false;
				}
				pushLight(pScopeLight);
			}
			break;
		case ast::Style::MATERIAL:
			{
				scope::Material::SharedPtr pMaterialScope = std::dynamic_pointer_cast<scope::Material>(mpCurrentScope);
				if(pMaterialScope) {
					//auto pMaterialX = createMaterialXFromLSD(pMaterialScope);
					//if (pMaterialX) {
						//mpRenderer->addMaterialX(std::move(pMaterialX));
					//}

					// Standard (fixed) material
					std::string material_name = pMaterialScope->getPropertyValue(ast::Style::OBJECT, "materialname", std::string(random_string(8) + "_material"));
					const Property* pShaderProp = pMaterialScope->getProperty(ast::Style::OBJECT, "surface");

					auto pMaterial = createStandardMaterialFromLSD(material_name, pShaderProp);

					if(pMaterial) mpRenderer->addStandardMaterial(pMaterial);
					
				} else {
					result = false;
				}
			}
			break;
		case ast::Style::NODE:
			{
				scope::Node::SharedPtr pScopeNode = std::dynamic_pointer_cast<scope::Node>(mpCurrentScope);
			}
			break;
		case ast::Style::SEGMENT:
		case ast::Style::GLOBAL:
			break;
		default:
			LLOG_ERR << "cmd_end makes no sense. Current scope type is " << to_string(mpCurrentScope->type()) << " !!!";
			break;
	}

	mpCurrentScope = pParentScope;
	return result;
}

void Session::setFailed(bool state) {
	if(!state) return;
	mFailed = state;
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

Falcor::StandardMaterial::SharedPtr Session::createStandardMaterialFromLSD(const std::string& material_name, const Property* pShaderProp) {
	auto pSceneBuilder = mpRenderer->sceneBuilder();
	if (!pSceneBuilder) {
		LLOG_ERR << "Unable to create standard material \"" << material_name << "\". SceneBuilder not ready !!!";
		return nullptr;
	}

	Falcor::float3 	surface_base_color = {0.2, 0.2, 0.2};
  std::string 	surface_base_color_texture_path  = "";
  std::string 	surface_base_normal_texture_path = "";
  std::string   surface_base_bump_texture_path   = "";
  std::string 	surface_metallic_texture_path    = "";
  std::string 	surface_roughness_texture_path   = "";
  std::string		surface_emission_texture_path    = "";

  bool 			surface_use_basecolor_texture  = false;
  bool 			surface_use_roughness_texture  = false;
  bool 			surface_use_metallic_texture   = false;
  bool 			surface_use_basenormal_texture = false;
  bool 			surface_use_emission_texture   = false;

  bool      front_face = false;

  float 		surface_ior = 1.5;
  float 		surface_metallic = 0.0;
  float 		surface_roughness = 0.3;
  float 		surface_reflectivity = 1.0;

  Falcor::float3  emissive_color = {0.0, 0.0, 0.0};
  float           emissive_factor = 1.0f;

  Falcor::float3  trans_color = {1.0, 1.0, 1.0};
  float           transmission = 0.0f;

  bool            basenormal_flip_x = false;
  bool            basenormal_flip_y = false;

  float           basenormal_scale = 1.0f;
  float           basebump_scale = 0.05f;

  std::string     basenormal_mode = "normal";

  float           ao_distance = 1.0f;

  if(pShaderProp) {
  	auto pShaderProps = pShaderProp->subContainer();
  	surface_base_color = to_float3(pShaderProps->getPropertyValue(ast::Style::OBJECT, "basecolor", lsd::Vector3{0.2, 0.2, 0.2}));
  	surface_base_color_texture_path = pShaderProps->getPropertyValue(ast::Style::OBJECT, "basecolor_texture", std::string());
  	surface_base_normal_texture_path = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseNormal_texture", std::string());
  	surface_base_bump_texture_path = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseBump_bumpTexture", std::string());
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

  	emissive_color = to_float3(pShaderProps->getPropertyValue(ast::Style::OBJECT, "emitcolor", lsd::Vector3{1.0, 1.0, 1.0}));
  	emissive_factor = pShaderProps->getPropertyValue(ast::Style::OBJECT, "emitint", 0.0f);

  	trans_color = to_float3(pShaderProps->getPropertyValue(ast::Style::OBJECT, "transcolor", lsd::Vector3{1.0, 1.0, 1.0}));
  	transmission = pShaderProps->getPropertyValue(ast::Style::OBJECT, "transparency", 0.0f);

  	basenormal_flip_x = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseNormal_flipX", false);
  	basenormal_flip_y = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseNormal_flipY", false);

  	basenormal_scale = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseNormal_scale", 1.0f);
  	basebump_scale = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseBump_bumpScale", 0.05f);

  	basenormal_mode = pShaderProps->getPropertyValue(ast::Style::OBJECT, "baseBumpAndNormal_type", std::string("normal"));

  	ao_distance = pShaderProps->getPropertyValue(ast::Style::OBJECT, "ao_distance", 1.0f);

  	front_face = pShaderProps->getPropertyValue(ast::Style::OBJECT, "frontface", false);
  } else {
  	LLOG_ERR << "No surface property set for materialname " << material_name;
  }

    
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

    pMaterial->setNormalMapFlipX(!basenormal_flip_x);
    pMaterial->setNormalMapFlipY(basenormal_flip_y);

    pMaterial->setTransmissionColor(trans_color);
    pMaterial->setSpecularTransmission(transmission);

    pMaterial->setNormalMapMode(basenormal_mode == "bump" ? Falcor::NormalMapMode::Bump : Falcor::NormalMapMode::Normal );

    float _normal_bump_scale = (pMaterial->getNormalMapMode() == NormalMapMode::Bump) ? (basebump_scale /* to match Mantra */) : basenormal_scale;
    pMaterial->setNormalBumpMapFactor(_normal_bump_scale);

  	//bool loadAsSrgb = true;
    bool loadTexturesAsSparse = !mpGlobal->getPropertyValue(ast::Style::GLOBAL, "vtoff", bool(false));

    LLOG_TRC << "Setting " << (loadTexturesAsSparse ? "sparse" : "simple") << " textures for material: " << pMaterial->getName();

    if(surface_base_color_texture_path != "" && surface_use_basecolor_texture) {
    	if(!pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::BaseColor, surface_base_color_texture_path, loadTexturesAsSparse)) {
    		return nullptr;
    	}
    }

    if(surface_metallic_texture_path != "" && surface_use_metallic_texture) {
    	if(!pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::Metallic, surface_metallic_texture_path, loadTexturesAsSparse)) {
    		return nullptr;
    	}
    }

    if(surface_emission_texture_path != "" && surface_use_emission_texture) { 
    	if(!pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::Emissive, surface_emission_texture_path, loadTexturesAsSparse)) {
    		return nullptr;
    	}
    }

    if(surface_roughness_texture_path != "" && surface_use_roughness_texture) {
    	if(!pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::Roughness, surface_roughness_texture_path, loadTexturesAsSparse)) {
    		return nullptr;
    	}
    }

    if((surface_base_normal_texture_path != ""  || surface_base_bump_texture_path != "") && surface_use_basenormal_texture) { 
    	
    	std::string _base_normal_bump_texture_path = (pMaterial->getNormalMapMode() == NormalMapMode::Bump) ? surface_base_bump_texture_path : surface_base_normal_texture_path;

    	if(!pSceneBuilder->loadMaterialTexture(pMaterial, Falcor::Material::TextureSlot::Normal, _base_normal_bump_texture_path, loadTexturesAsSparse)) {
    		return nullptr;
    	}
    }
  }

  return pMaterial;
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


bool Session::pushGeometryInstance(scope::Object::SharedConstPtr pObj, bool update) {
	assert(pObj);

	auto const& mesh_name = pObj->geometryName();

	LLOG_DBG << "pushGeometryInstance for geometry (mesh) name: " << mesh_name;
	
	auto pSceneBuilder = mpRenderer->sceneBuilder();
	if (!pSceneBuilder) {
		LLOG_ERR << "Unable to push geometry instance. SceneBuilder not ready !!!";
		return false;
	}

	const int _id = pObj->getPropertyValue(ast::Style::OBJECT, "id", (int)SceneBuilder::kInvalidExportedID);

	uint32_t exportedInstanceID = (_id < 0) ? SceneBuilder::kInvalidExportedID : static_cast<uint32_t>(_id);
	std::string obj_name = pObj->getPropertyValue(ast::Style::OBJECT, "name", std::string());

	const uint32_t meshID = pSceneBuilder->getMeshID(mesh_name);
	if(meshID == SceneBuilder::kInvalidMeshID) {
		LLOG_FTL << "Error getting id for mesh " << mesh_name;
		return false;
	}

	if(!pSceneBuilder->meshHasInstance(meshID, obj_name)) update = false;

	LLOG_DBG << (update ? "Updating" : "Creating") << " mesh " << meshID << " instance named " << obj_name;

	Falcor::SceneBuilder::Node transformNode = {};
	transformNode.name = obj_name;
	transformNode.transform = pObj->getTransformList()[0];
	transformNode.meshBind = glm::mat4(1);          // For skinned meshes. World transform at bind time.
 	transformNode.localToBindPose = glm::mat4(1);   // For bones. Inverse bind transform.

	const Property* pShaderProp = pObj->getProperty(ast::Style::OBJECT, "surface");
  std::string material_name = pObj->getPropertyValue(ast::Style::OBJECT, "materialname", std::string(obj_name + "_material"));
    
	Falcor::StandardMaterial::SharedPtr pMaterial = std::dynamic_pointer_cast<Falcor::StandardMaterial>(pSceneBuilder->getMaterial(material_name));
	
	if(update) {
		// Update material if needed
		auto pUpdatedMaterial = createStandardMaterialFromLSD(material_name, pShaderProp);
		if(!pSceneBuilder->updateMaterial(material_name, pUpdatedMaterial)) {
			LLOG_ERR << "Error updating material " << material_name;
			return false;
		}
	} else if(!pMaterial) {
		pMaterial = createStandardMaterialFromLSD(material_name, pShaderProp);
	}

	// Instance exported data
	SceneBuilder::InstanceExportedDataSpec exportedSpec;
	exportedSpec.id = exportedInstanceID;
	exportedSpec.name = obj_name;

  // Instance shading spec
  SceneBuilder::InstanceShadingSpec shadingSpec;
  shadingSpec.isMatte = pObj->getPropertyValue(ast::Style::OBJECT, "matte", false);
  shadingSpec.fixShadowTerminator = pObj->getPropertyValue(ast::Style::OBJECT, "fix_shadow", true);
  shadingSpec.biasAlongNormal = pObj->getPropertyValue(ast::Style::OBJECT, "biasnormal", false);
  shadingSpec.doubleSided = pObj->getPropertyValue(ast::Style::OBJECT, "double_sided", true);

  // Instance visibility spec
  SceneBuilder::InstanceVisibilitySpec visibilitySpec;
  visibilitySpec.visibleToPrimaryRays = pObj->getPropertyValue(ast::Style::OBJECT, "visible_primary", true);
  visibilitySpec.visibleToShadowRays = pObj->getPropertyValue(ast::Style::OBJECT, "visible_shadows", true);
  visibilitySpec.visibleToDiffuseRays = pObj->getPropertyValue(ast::Style::OBJECT, "visible_diffuse", true);
  visibilitySpec.visibleToReflectionRays = pObj->getPropertyValue(ast::Style::OBJECT, "visible_reflect", true);
  visibilitySpec.visibleToRefractionRays = pObj->getPropertyValue(ast::Style::OBJECT, "visible_refract", true);
  visibilitySpec.receiveShadows = pObj->getPropertyValue(ast::Style::OBJECT, "receive_shadows", true);
  visibilitySpec.receiveSelfShadows = pObj->getPropertyValue(ast::Style::OBJECT, "receive_self_shadows", true);
  
  SceneBuilder::MeshInstanceCreationSpec creationSpec;
  creationSpec.pExportedDataSpec = &exportedSpec;
  creationSpec.pVisibilitySpec = &visibilitySpec;
  creationSpec.pShadingSpec = &shadingSpec;
  creationSpec.pMaterialOverride = pMaterial;

  if(update) {
  	// Update mesh instance
  	return pSceneBuilder->updateMeshInstance(meshID, &creationSpec, transformNode);
  } else {
  	// Add a mesh instance
  	LLOG_WRN << "1";
  	const uint32_t nodeID = pSceneBuilder->addNode(transformNode);
  	LLOG_WRN << "2";
  	return pSceneBuilder->addMeshInstance(nodeID, meshID, &creationSpec);
	}
}


bool Session::cmdGeometry(const std::string& name) {
	switch(mpCurrentScope->type()) {
		case ast::Style::OBJECT:
			{
				auto pObj = std::dynamic_pointer_cast<scope::Object>(mpCurrentScope);
 				pObj->setGeometryName(name);
			}
			return true;
		case ast::Style::LIGHT:
			{
				auto pLight = std::dynamic_pointer_cast<scope::Light>(mpCurrentScope);
 				//pLight->setGeometryName(name);
			}
			return true;
		default:
			LLOG_ERR << "cmd_geometry call within unsupported scope " << to_string(mpCurrentScope->type()) << " !!!";
			return false;
	}
}

void Session::cmdTime(double time) {
	mCurrentTime = time;
	mpRenderer->init(mRendererConfig);
}


}  // namespace lsd

}  // namespace lava