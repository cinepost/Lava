#include <utility>
#include <mutex>
#include <limits>

#include "session_helpers.h"
#include "lava_utils_lib/ut_fsys.h"

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

static uint32_t componentsCountFromLSDTypeName(const std::string& type_name) {
	if(type_name == "vector1") return 1;
	if(type_name == "vector2") return 2;
	if(type_name == "vector3") return 3;
	if(type_name == "vector4") return 4;

	LLOG_WRN << "Unsupported type: << " << type_name;
	return 4;
}

Falcor::ResourceFormat resolveAOVResourceFormat(const std::string& format_name, uint32_t numChannels) {
	assert((0 < numChannels) && ( numChannels <= 4));

	if( format_name == "int8") {
		switch (numChannels) {
			case 1: return Falcor::ResourceFormat::R8Unorm;
			case 2: return Falcor::ResourceFormat::RG8Unorm;
			case 3: return Falcor::ResourceFormat::RGB8Unorm;
			default: return Falcor::ResourceFormat::RGBA8Unorm;
		}
	}
	if( format_name == "int16") {
		switch (numChannels) {
			case 1: return Falcor::ResourceFormat::R16Unorm;
			case 2: return Falcor::ResourceFormat::RG16Unorm;
			case 3: return Falcor::ResourceFormat::RGB16Unorm;
			default: return Falcor::ResourceFormat::RGBA16Unorm;
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
	if( format_name == "float32") {
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

	std::string quantization_name = pPlane->getPropertyValue(ast::Style::PLANE, "quantize", std::string("float32"));
	std::string type_name = pPlane->getPropertyValue(ast::Style::PLANE, "type", std::string("vector4"));

	aovInfo.name = channel_name;
	aovInfo.format = resolveAOVResourceFormat(quantization_name, componentsCountFromLSDTypeName(type_name));
	aovInfo.variableName = output_variable_name;

	return aovInfo;
}

Display::SharedPtr createDisplay(const Session::DisplayInfo& display_info) {
	Display::SharedPtr pDisplay = Display::create(display_info.displayType);
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



}  // namespace lsd

}  // namespace lava