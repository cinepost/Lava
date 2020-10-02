#include "session_lsd.h"

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

}  // namespace lsd

SessionLSD::SessionLSD(std::unique_ptr<RendererIface> pRendererIface) { 
	mpRendererIface = std::move(pRendererIface);
}

SessionLSD::~SessionLSD() { }


bool SessionLSD::loadDisplayByType(const lsd::ast::DisplayType& display_type) {
	std::string display_name;

	switch(display_type) {
		case lsd::ast::DisplayType::IP:
		case lsd::ast::DisplayType::MD:
			display_name = "houdini";
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

bool SessionLSD::loadDisplayByFileName(const std::string& file_name) {
	return mpRendererIface->loadDisplay(lsd::resolveDisplayDriverByFileName(file_name));
}

void SessionLSD::cmdSetEnv(const std::string& key, const std::string& value) {
	mpRendererIface->setEnvVariable(key, value);
}

void SessionLSD::cmdConfig(const std::string& file_name) {
	// actual render graph configs loading postponed unitl renderer is initialized
	mGraphConfigs.push_back(file_name);
}

// initialize renderer and push render data
bool SessionLSD::initRenderData() {
	LLOG_DBG << "initRenderData";
	if(!mpRendererIface->initRenderer()) return false;

	for(auto const& graph_conf_file: mGraphConfigs) {
		if(!mpRendererIface->loadScript(graph_conf_file)) return false;
	}

	return true;
}

void SessionLSD::cmdRaytrace() {
	LLOG_DBG << "cmdRaytrace";
	//initRenderData(); // push postponed data
	//mpRendererIface->renderFrame();
}

}  // namespace lava