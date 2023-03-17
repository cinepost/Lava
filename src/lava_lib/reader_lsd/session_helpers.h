#ifndef SRC_LAVA_LIB_READER_LSD_SESSION_HELPERS_H_
#define SRC_LAVA_LIB_READER_LSD_SESSION_HELPERS_H_

#include <memory>
#include <variant>
#include <future>
#include <map>
#include <unordered_map>

#include "grammar_lsd.h"
#include "../reader_bgeo/bgeo/Bgeo.h"
#include "../renderer.h"
#include "../aov.h"
#include "scope.h"
#include "../display.h"

#include "Falcor/Core/API/Formats.h"
#include "session.h"

namespace lava {

namespace lsd {

Falcor::ResourceFormat  resolveAOVResourceFormat(const std::string& type_name, const std::string& format_name, uint32_t numChannels);

Display::DisplayType    resolveDisplayTypeByFileName(const std::string& file_name);
Display::TypeFormat     resolveDisplayTypeFormat(const std::string& fname);
Renderer::SamplePattern resolveSamplePatternType(const std::string& sample_pattern_name);
AOVPlaneInfo            aovInfoFromLSD(scope::Plane::SharedPtr pPlane);

Display::SharedPtr      createDisplay(const Session::DisplayInfo& display_info);

void makeImageTiles(const Renderer::FrameInfo& frameInfo, Falcor::uint2 tileSize, std::vector<Session::TileInfo>& tiles);
bool sendImageData(uint hImage, Display* pDisplay, AOVPlane* pAOVPlane, std::vector<uint8_t>& textureData);
bool sendImageRegionData(uint hImage, Display* pDisplay, const Renderer::FrameInfo& frameInfo,  AOVPlane* pAOVPlane, std::vector<uint8_t>& textureData);

void translateLSDPlanePropertiesToLavaDict(scope::Plane::SharedConstPtr pScope, Falcor::Dictionary& dict);

}  // namespace lsd

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_SESSION_HELPERS_H_