#include <utility>
#include <mutex>
#include <limits>

#include "session_helpers.h"
#include "lava_utils_lib/ut_fsys.h"

#include "../display_prman.h"
#include "../display_oiio.h"

namespace lava {

namespace lsd {

// TODO: handle requred channels (RGB/RGBA)
static Falcor::ResourceFormat resolveShadingResourceFormat(Display::TypeFormat fmt, uint numchannels) {
    assert(numchannels <= 4);

    switch(fmt) {
        case Display::TypeFormat::SIGNED8:
            if( numchannels == 1) return Falcor::ResourceFormat::R8Snorm; 
            if( numchannels == 3) return Falcor::ResourceFormat::RG8Snorm;
            if( numchannels == 3) return Falcor::ResourceFormat::RGB8Snorm;
            return Falcor::ResourceFormat::RGBA8Snorm;

        case Display::TypeFormat::UNSIGNED8:
            if( numchannels == 1) return Falcor::ResourceFormat::R8Unorm; 
            if( numchannels == 3) return Falcor::ResourceFormat::RG8Unorm;
            if( numchannels == 3) return Falcor::ResourceFormat::RGB8Unorm;
            return Falcor::ResourceFormat::RGBA8Unorm;

        case Display::TypeFormat::SIGNED16:
            if( numchannels == 1) return Falcor::ResourceFormat::R16Int; 
            if( numchannels == 3) return Falcor::ResourceFormat::RG16Int;
            if( numchannels == 3) return Falcor::ResourceFormat::RGB16Int;
            return Falcor::ResourceFormat::RGBA16Int;  // TODO: add RGBA16Snorm to Falcor formats
        
        case Display::TypeFormat::UNSIGNED16:
            if( numchannels == 1) return Falcor::ResourceFormat::R16Unorm; 
            if( numchannels == 3) return Falcor::ResourceFormat::RG16Unorm;
            if( numchannels == 3) return Falcor::ResourceFormat::RGB16Unorm;
            return Falcor::ResourceFormat::RGBA16Unorm;
        
        case Display::TypeFormat::FLOAT16:
            if( numchannels == 1) return Falcor::ResourceFormat::R16Float; 
            if( numchannels == 3) return Falcor::ResourceFormat::RG16Float;
            if( numchannels == 3) return Falcor::ResourceFormat::RGB16Float;
            return Falcor::ResourceFormat::RGBA16Float;
        
        case Display::TypeFormat::FLOAT32:
        default:
            if( numchannels == 1) return Falcor::ResourceFormat::R32Float; 
            if( numchannels == 3) return Falcor::ResourceFormat::RG32Float;
            if( numchannels == 3) return Falcor::ResourceFormat::RGB32Float;
            return Falcor::ResourceFormat::RGBA32Float;
    }
}


Display::DisplayType resolveDisplayTypeByFileName(const std::string& file_name) {
	std::string ext = ut::fsys::getFileExtension(file_name);

    if( ext == ".exr" ) return Display::DisplayType::OPENEXR;
    if( ext == ".jpg" ) return Display::DisplayType::JPEG;
    if( ext == ".jpeg" ) return Display::DisplayType::JPEG;
    if( ext == ".png" ) return Display::DisplayType::PNG;
    if( ext == ".tif" ) return Display::DisplayType::TIFF;
    if( ext == ".tiff" ) return Display::DisplayType::TIFF;

    return Display::DisplayType::OPENEXR;
}

Display::Display::TypeFormat resolveDisplayTypeFormat(const std::string& fname) {
	if( fname == "int8") return Display::TypeFormat::UNSIGNED8;
	if( fname == "int16") return Display::TypeFormat::UNSIGNED16;
	if( fname == "float16") return Display::TypeFormat::FLOAT16;	
	if( fname == "float32") return Display::TypeFormat::FLOAT32;

	return Display::TypeFormat::FLOAT32;
}

static inline bool isNormalizedTypeName(const std::string& type_name) {
	if(type_name == "float") return true;
	if(type_name == "vector2") return true;
	if(type_name == "vector3") return true;
	if(type_name == "vector4") return true;

	return false;
}

static inline uint32_t componentsCountFromLSDTypeName(const std::string& type_name) {
	if(type_name == "float") return 1;
	if(type_name == "int") return 1;
	
	if(type_name == "vector2") return 2;
	if(type_name == "int2") return 2;
	
	if(type_name == "vector3") return 3;
	if(type_name == "int3") return 2;

	if(type_name == "vector4") return 4;
	if(type_name == "int4") return 2;

	LLOG_WRN << "Unsupported type: << " << type_name;
	return 4;
}

Falcor::ResourceFormat resolveAOVResourceFormat(const std::string& type_name, const std::string& format_name, uint32_t numChannels) {
	assert((0 < numChannels) && ( numChannels <= 4));

	bool norm = isNormalizedTypeName(type_name);
	
	if( format_name == "int8") {
		switch (numChannels) {
			case 1: return norm ? Falcor::ResourceFormat::R8Snorm : Falcor::ResourceFormat::R8Int;
			case 2: return norm ? Falcor::ResourceFormat::RG8Snorm : Falcor::ResourceFormat::RG8Int;
			case 3: return norm ? Falcor::ResourceFormat::RGB8Snorm : Falcor::ResourceFormat::RGB8Int;
			default: return norm ? Falcor::ResourceFormat::RGBA8Snorm : Falcor::ResourceFormat::RGBA8Int;
		}
	}
	if( format_name == "uint8") {
		switch (numChannels) {
			case 1: return norm ? Falcor::ResourceFormat::R8Unorm : Falcor::ResourceFormat::R8Uint;
			case 2: return norm ? Falcor::ResourceFormat::RG8Unorm : Falcor::ResourceFormat::RG8Uint;
			case 3: return norm ? Falcor::ResourceFormat::RGB8Unorm : Falcor::ResourceFormat::RGB8Uint;
			default: return norm ? Falcor::ResourceFormat::RGBA8Unorm : Falcor::ResourceFormat::RGBA8Uint;
		}
	}
	if( format_name == "int16") {
		switch (numChannels) {
			case 1: return norm ? Falcor::ResourceFormat::R16Snorm : Falcor::ResourceFormat::R16Int;
			case 2: return norm ? Falcor::ResourceFormat::RG16Snorm : Falcor::ResourceFormat::R16Int;
			case 3: return norm ? Falcor::ResourceFormat::RGB16Snorm : Falcor::ResourceFormat::R16Int;
			default: return norm ? Falcor::ResourceFormat::RGBA16Snorm : Falcor::ResourceFormat::R16Int;
		}
	}
	if( format_name == "uint16") {
		switch (numChannels) {
			case 1: return norm ? Falcor::ResourceFormat::R16Unorm : Falcor::ResourceFormat::R16Uint;
			case 2: return norm ? Falcor::ResourceFormat::RG16Unorm : Falcor::ResourceFormat::RG16Uint;
			case 3: return norm ? Falcor::ResourceFormat::RGB16Unorm : Falcor::ResourceFormat::RGB16Uint;
			default: return norm ? Falcor::ResourceFormat::RGBA16Unorm : Falcor::ResourceFormat::RGBA16Uint;
		}
	}
	if(( format_name == "int32") || (format_name == "int")) {
		switch (numChannels) {
			case 1: return Falcor::ResourceFormat::R32Int;
			case 2: return Falcor::ResourceFormat::RG32Int;
			case 3: return Falcor::ResourceFormat::RGB32Int;
			default: return Falcor::ResourceFormat::RGBA32Int;
		}
	}
	if(( format_name == "uint32") || (format_name == "uint")) {
		switch (numChannels) {
			case 1: return Falcor::ResourceFormat::R32Uint;
			case 2: return Falcor::ResourceFormat::RG32Uint;
			case 3: return Falcor::ResourceFormat::RGB32Uint;
			default: return Falcor::ResourceFormat::RGBA32Uint;
		}
	}
	if( format_name == "float16") {
		switch (numChannels) {
			case 1: return Falcor::ResourceFormat::R16Float;
			case 2: return Falcor::ResourceFormat::RG16Float;
			case 3: return Falcor::ResourceFormat::RGB16Float;
			default: return Falcor::ResourceFormat::RGBA16Float;
		}
	}
	if(( format_name == "float32") || (format_name == "float")) {
		switch (numChannels) {
			case 1: return Falcor::ResourceFormat::R32Float;
			case 2: return Falcor::ResourceFormat::RG32Float;
			case 3: return Falcor::ResourceFormat::RGB32Float;
			default: return Falcor::ResourceFormat::RGBA32Float;
		}
	}
	LLOG_WRN << "Unsupported AOV format name: " << format_name;

	return Falcor::ResourceFormat::RGBA32Float;
}


Renderer::SamplePattern resolveSamplePatternType(const std::string& sample_pattern_name) {
	if( sample_pattern_name == "stratified" ) return Renderer::SamplePattern::Stratified;
	if( sample_pattern_name == "halton" )  return Renderer::SamplePattern::Halton;
	return Renderer::SamplePattern::Center;	
}

AOVPlaneInfo aovInfoFromLSD(scope::Plane::SharedPtr pPlane) {
	AOVPlaneInfo aovInfo;

	std::string channel_name = pPlane->getPropertyValue(ast::Style::PLANE, "channel", std::string());
	if(channel_name.size() == 0) {
		LLOG_ERR << "No channel name specified for plane !!!";
	}

	std::string output_variable_name = pPlane->getPropertyValue(ast::Style::PLANE, "variable", std::string());
	if(output_variable_name.size() == 0) {
		LLOG_ERR << "No plane variable specified for plane !!!";
	}

	const std::string quantization_name = pPlane->getPropertyValue(ast::Style::PLANE, "quantize", std::string("float16"));
	const std::string type_name = pPlane->getPropertyValue(ast::Style::PLANE, "type", std::string("vector4"));
	const std::string pixel_filter_name = pPlane->getPropertyValue(ast::Style::PLANE, "pfilter", std::string("box"));
	const std::string source_pass_name = pPlane->getPropertyValue(ast::Style::PLANE, "sourcepass", std::string(""));
	const std::string output_name_override = pPlane->getPropertyValue(ast::Style::PLANE, "outputname_override", std::string(""));
	const Int2 pixel_filter_size = pPlane->getPropertyValue(ast::Style::PLANE, "pfiltersize", Int2{1, 1});
	const bool enable_accumulation = pPlane->getPropertyValue(ast::Style::PLANE, "accumulation", bool(true));

	aovInfo.name = AOVName(channel_name);
	aovInfo.outputOverrideName = output_name_override;
	aovInfo.format = resolveAOVResourceFormat(type_name, quantization_name, componentsCountFromLSDTypeName(type_name));
	aovInfo.variableName = output_variable_name;
	aovInfo.pfilterTypeName = pixel_filter_name;
	aovInfo.pfilterSize = to_uint2(pixel_filter_size);
	aovInfo.sourcePassName = source_pass_name;
	aovInfo.enableAccumulation = enable_accumulation;

	return aovInfo;
}

Display::SharedPtr createDisplay(const Session::DisplayInfo& display_info) {
	Display::SharedPtr pDisplay = nullptr;
	if(DisplayOIIO::isDiplayTypeSupported(display_info.displayType)) {
		pDisplay = DisplayOIIO::create(display_info.displayType);
	} else {
		pDisplay = DisplayPrman::create(display_info.displayType);
	}
	
	if(!pDisplay) {
        LLOG_ERR << "Unable to create display !!!";
		return nullptr;
    }

	// push display driver parameters
	for(auto const& parm: display_info.displayStringParameters)
		pDisplay->setStringParameter(parm.first, parm.second);

	for(auto const& parm: display_info.displayIntParameters)
		pDisplay->setIntParameter(parm.first, parm.second);

	for(auto const& parm: display_info.displayFloatParameters)
		pDisplay->setFloatParameter(parm.first, parm.second);

	return pDisplay;
}

void makeImageTiles(const Renderer::FrameInfo& frameInfo, Falcor::uint2 tileSize, std::vector<Session::TileInfo>& tiles) {
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

bool sendImageData(uint hImage, Display* pDisplay, AOVPlane* pAOVPlane) {
	assert(pAOVPlane);
	if (!pDisplay) return false;

	LLOG_DBG << "Reading " << pAOVPlane->name() << " AOV image data";
	const auto pData = pAOVPlane->getProcessedImageData();
	if(!pData) {
		LLOG_ERR << "Error reading AOV " << pAOVPlane->name() << " texture data !!!";
		return false;
	}
	LLOG_DBG << "Image data read done!";
	
	AOVPlaneGeometry aov_plane_geometry;
	if(!pAOVPlane->getAOVPlaneGeometry(aov_plane_geometry)) {
		return false;
	}
	
	if (!pDisplay->sendImage(hImage, aov_plane_geometry.width, aov_plane_geometry.height, pData)) {
        LLOG_ERR << "Error sending image to display !";
        return false;
    }
    return true;
}

bool sendImageRegionData(uint hImage, Display* pDisplay, const Renderer::FrameInfo& frameInfo, AOVPlane* pAOVPlane) {
	assert(pDisplay);
	if (!pDisplay) return false;

	if(pAOVPlane) {
		// Send AOV image data
		if ((frameInfo.imageWidth == frameInfo.regionWidth()) && (frameInfo.imageHeight == frameInfo.regionHeight())) {
			// If sending region that is equal to full frame we just send full image data
			return sendImageData(hImage, pDisplay, pAOVPlane);
		}

		LLOG_DBG << "Reading " << pAOVPlane->name() << " AOV image region data";
		const auto pData = pAOVPlane->getProcessedImageData();
		if(!pData) {
			LLOG_ERR << "Error reading AOV " << pAOVPlane->name() << " texture data !!!";
			return false;
		}
		LLOG_DBG << "Image data read done!";
	
		if (!pDisplay->sendImageRegion(hImage, frameInfo.renderRegion[0], frameInfo.renderRegion[1], frameInfo.regionWidth(), frameInfo.regionHeight(), pData)) {
			LLOG_ERR << "Error sending image region to display !";
			return false;
		}

	} else {
		// Send zero image data
		if (!pDisplay->sendImageRegion(hImage, frameInfo.renderRegion[0], frameInfo.renderRegion[1], frameInfo.regionWidth(), frameInfo.regionHeight(), nullptr)) {
			LLOG_ERR << "Error sending image region to display !";
			return false;
		}
	}
    return true;
}

void translateLSDPlanePropertiesToLavaDict(scope::Plane::SharedConstPtr pScope, Falcor::Dictionary& dict) {
	//pScope->printSummary(std::cout, 4);
	
	for (const auto& [key, value] : pScope->to_dict(ast::Style::IMAGE)) {
		dict[key] = value;
	}
}

}  // namespace lsd

}  // namespace lava