#include "renderer_iface_lsd.h"

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

RendererIfaceLSD::RendererIfaceLSD(Renderer *renderer): RendererIfaceBase(renderer) { }

RendererIfaceLSD::~RendererIfaceLSD() { }

bool RendererIfaceLSD::loadDisplayByType(const lsd::ast::DisplayType& display_type) {
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

	return loadDisplay(display_name);
}

bool RendererIfaceLSD::loadDisplayByFileName(const std::string& file_name) {
	return loadDisplay(lsd::resolveDisplayDriverByFileName(file_name));
}

void RendererIfaceLSD::cmdConfig(const std::string& file_name) {
	// actual render graph configs loading postponed unitl renderer is initialized
	mGraphConfigs.push_back(file_name);
}

// initialize renderer and push render data
bool RendererIfaceLSD::initRenderData() {
	LLOG_DBG << "initRenderData";
	if(!initRenderer()) return false;

	for(auto const& graph_conf_file: mGraphConfigs) {
		if(!loadScript(graph_conf_file)) return false;
	}

	return true;
}

void RendererIfaceLSD::cmdRaytrace() {
	LLOG_DBG << "cmdRaytrace";
	initRenderer(); // push postponed data
	renderFrame();
}

}  // namespace lava