#ifndef SRC_LAVA_LIB_DISPLAY_H_
#define SRC_LAVA_LIB_DISPLAY_H_

#include "lava_dll.h"

#include <map>
#include <string>
#include <vector>
#include <memory>

//#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Formats.h"
#include "Falcor/Utils/Scripting/Dictionary.h"
#include "prman/ndspy.h"

namespace lava {

  using uint = uint32_t;

class LAVA_API Display {
  public:
    using MetaData = Falcor::Dictionary;
    using UserParm = UserParameter;
    enum class DisplayType { NONE, NUL, IP, MD, HOUDINI, OPENEXR, JPEG, TIFF, PNG, SDL, IDISPLAY, __HYDRA__ }; // __HYDRA is a virtual pseudo type
    enum class TypeFormat { FLOAT32, FLOAT16, UNSIGNED32, SIGNED32, UNSIGNED16, SIGNED16, UNSIGNED8, SIGNED8, UNKNOWN };

    struct Channel {
      std::string name;
      Display::TypeFormat  format;
    };

    enum NamingScheme {
      RGBA,
      XYZW,
    };

    struct ChannelNamingDesc {
      Display::NamingScheme namingScheme;
      std::array<std::string, 4> channelNames;
    };

  public:
    using SharedPtr = std::shared_ptr<Display>;

    virtual ~Display() {};
    
    virtual bool openImage(
      const std::string& image_name, uint width, uint height, const std::vector<Channel>& channels, uint &imageHandle, 
      const std::vector<UserParameter>& userParams, const MetaData* pMetaData = nullptr) = 0;
    
    virtual bool openImage(const std::string& image_name, uint width, uint height, Falcor::ResourceFormat format, uint &imageHandle, 
      const std::vector<UserParameter>& userParams, const std::string& channel_prefix, const MetaData* pMetaData = nullptr) = 0;
    
    virtual bool closeImage(uint imageHandle) = 0;
    virtual bool closeAll() = 0;

    virtual bool sendImageRegion(uint imageHandle, uint x, uint y, uint width, uint height, const uint8_t *data) = 0;
    virtual bool sendImage(uint imageHandle, uint width, uint height, const uint8_t *data) = 0;

    virtual bool opened(uint imageHandle) const = 0;
    virtual bool closed(uint imageHandle) const = 0;

    virtual const std::string& imageName(uint imageHandle) const = 0;
    virtual uint imageWidth(uint imageHandle) const = 0;
    virtual uint imageHeight(uint imageHandle) const = 0;

    virtual bool setStringParameter(const std::string& name, const std::vector<std::string>& strings) = 0;
    virtual bool setIntParameter(const std::string& name, const std::vector<int>& ints) = 0;
    virtual bool setFloatParameter(const std::string& name, const std::vector<float>& floats) = 0;

    virtual bool supportsMetaData() const { return false; }
    inline bool isInteractive() const { return mInteractiveSupport; }
    inline DisplayType type() const { return mDisplayType; };

  public:

    static void makeStringsParameter(const std::string& name, const std::vector<std::string>& strings, UserParameter& parameter);
    static void makeIntsParameter(const std::string& name, const std::vector<int>& ints, UserParameter& parameter);
    static void makeFloatsParameter(const std::string& name, const std::vector<float>& floats, UserParameter& parameter);

    static UserParameter makeStringsParameter(const std::string& name, const std::vector<std::string>& strings);
    static UserParameter makeIntsParameter(const std::string& name, const std::vector<int>& ints);
    static UserParameter makeFloatsParameter(const std::string& name, const std::vector<float>& floats);

  public:
    static const std::string& makeChannelName(Display::NamingScheme namingScheme, uint32_t channelIndex);

  protected:
    static std::string makeImageHashString(const std::string& image_name, uint width, uint height, const std::vector<Channel>& channels, uint &imageHandle, 
      const std::vector<UserParameter>& userParams, const MetaData* pMetaData = nullptr);

  protected:
    DisplayType mDisplayType = DisplayType::NONE;

    bool mOverwriteSupport = false;
    bool mInteractiveSupport = false;
    bool mForceScanLines = false;
};

inline size_t getFormatSizeInBytes(Display::TypeFormat format) {
  switch (format) {
    case Display::TypeFormat::UNSIGNED32:
    case Display::TypeFormat::SIGNED32:
    case Display::TypeFormat::FLOAT32:
      return 4;
    case Display::TypeFormat::UNSIGNED16:
    case Display::TypeFormat::SIGNED16:
    case Display::TypeFormat::FLOAT16:
      return 2;
    case Display::TypeFormat::UNSIGNED8:
    case Display::TypeFormat::SIGNED8:
    default:
      return 1;
  }
}

inline std::string getDisplayDriverFileName(Display::DisplayType display_type) {
  switch(display_type) {
    case Display::DisplayType::IP:
    case Display::DisplayType::MD:
    case Display::DisplayType::HOUDINI:
      return "houdini";
    case Display::DisplayType::SDL:
      return "sdl";
    case Display::DisplayType::IDISPLAY:
      return "idisplay";
    case Display::DisplayType::OPENEXR:
      return "openexr";
    case Display::DisplayType::JPEG:
      return "jpeg";
    case Display::DisplayType::TIFF:
      return "tiff";
    case Display::DisplayType::PNG:
      return "png";
    case Display::DisplayType::NONE:
    default:
      return "null";
  }
}

inline std::string to_string(lava::Display::DisplayType display_type) {
  switch(display_type) {
    case Display::DisplayType::IP:
      return "IP";
    case Display::DisplayType::MD:
      return "MD";
    case Display::DisplayType::HOUDINI:
      return "HOUDINI";
    case Display::DisplayType::SDL:
      return "SDL";
    case Display::DisplayType::IDISPLAY:
      return "IDISPLAY";
    case Display::DisplayType::OPENEXR:
      return "OPENEXR";
    case Display::DisplayType::JPEG:
      return "JPEG";
    case Display::DisplayType::TIFF:
      return "TIFF";
    case Display::DisplayType::PNG:
      return "PNG";
    case Display::DisplayType::NONE:
    default:
      return "NONE";
  }
}

inline std::string to_string(Display::TypeFormat typeFormat) {
  switch(typeFormat) {
    case Display::TypeFormat::FLOAT32:
      return "FLOAT32";
    case Display::TypeFormat::FLOAT16:
      return "FLOAT16";
    case Display::TypeFormat::UNSIGNED32:
      return "UNSIGNED32";
    case Display::TypeFormat::SIGNED32:
      return "SIGNED32";
    case Display::TypeFormat::UNSIGNED16:
      return "UNSIGNED16";
    case Display::TypeFormat::SIGNED16:
      return "SIGNED16";
    case Display::TypeFormat::UNSIGNED8:
      return "UNSIGNED8";
    case Display::TypeFormat::SIGNED8:
      return "SIGNED8";
    default:
      return "UNKNOWN";
  }
}

inline Display::TypeFormat falcorTypeToDiplay(Falcor::FormatType format_type, uint32_t numChannelBits) {
  assert((numChannelBits == 8) || (numChannelBits == 16) || (numChannelBits == 32));

  switch (numChannelBits) {
    case 8:
      switch (format_type) {
        case Falcor::FormatType::Uint:
        case Falcor::FormatType::Unorm:
        case Falcor::FormatType::UnormSrgb:
          return Display::TypeFormat::UNSIGNED8;
        case Falcor::FormatType::Sint:
        case Falcor::FormatType::Snorm:
          return Display::TypeFormat::SIGNED8;
        default:
          break;
      }
    case 16:
      switch (format_type) {
        case Falcor::FormatType::Uint:
        case Falcor::FormatType::Unorm:
        case Falcor::FormatType::UnormSrgb:
          return Display::TypeFormat::UNSIGNED16;
        case Falcor::FormatType::Sint:
        case Falcor::FormatType::Snorm:
          return Display::TypeFormat::SIGNED16;
        case Falcor::FormatType::Float:
          return Display::TypeFormat::FLOAT16;
        default:
          break;
      }
    case 32:
      switch (format_type) {
        case Falcor::FormatType::Float:
          return Display::TypeFormat::FLOAT32;
        default:
          return Display::TypeFormat::UNSIGNED32;
      }
    default:
      LLOG_ERR << "Unsupported number of bits per channel (" << numChannelBits << ") !!!";
      break;
  }

  return Display::TypeFormat::UNKNOWN;
}

inline Display::Channel makeDisplayChannel(const std::string& prefix, uint32_t index, Falcor::FormatType format_type, uint32_t numChannelBits, Display::NamingScheme namingScheme) {
  Display::Channel channel;
  if (!prefix.empty()) {
    channel.name = prefix + "." + Display::makeChannelName(namingScheme, index);
  } else {
    channel.name = Display::makeChannelName(namingScheme, index);
  }
  channel.format = falcorTypeToDiplay(format_type, numChannelBits);
  return channel;
}

}  // namespace lava

#endif  // SRC_LAVA_LIB_DISPLAY_H_