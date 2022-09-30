#ifndef SRC_LAVA_LIB_DISPLAY_H_
#define SRC_LAVA_LIB_DISPLAY_H_

#include <map>
#include <string>
#include <vector>
#include <memory>

//#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Formats.h"
#include "prman/ndspy.h"

namespace lava {

class Display {
  public:
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

 public:
    using SharedPtr = std::shared_ptr<Display>;

    ~Display();
    static SharedPtr create(Display::DisplayType display_type);


    bool openImage(const std::string& image_name, uint width, uint height, const std::vector<Channel>& channels, uint &imageHandle);
    bool openImage(const std::string& image_name, uint width, uint height, Falcor::ResourceFormat format, uint &imageHandle, std::string channel_prefix = "");
    bool closeImage(uint imageHandle);
    bool closeAll();

    inline bool opened(uint imageHandle) { return mImages[imageHandle].opened; }
    inline bool closed(uint imageHandle) { return mImages[imageHandle].closed; }

    inline DisplayType type() const { return mDisplayType; };

    inline std::string& imageName(uint imageHandle) { return mImages[imageHandle].name; };
    inline uint imageWidth(uint imageHandle) { return mImages[imageHandle].width; };
    inline uint imageHeight(uint imageHandle) { return mImages[imageHandle].height; };

    bool setStringParameter(const std::string& name, const std::vector<std::string>& strings);
    bool setIntParameter(const std::string& name, const std::vector<int>& ints);
    bool setFloatParameter(const std::string& name, const std::vector<float>& floats);

    bool sendImageRegion(uint imageHandle, uint x, uint y, uint width, uint height, const uint8_t *data);
    bool sendImage(uint imageHandle, uint width, uint height, const uint8_t *data);

    inline bool supportsLiveUpdate() const { return mInteractiveSupport; }

 private:
    struct ImageData {
      PtDspyImageHandle handle;
      std::string name = "";
      uint width = 0;
      uint height = 0;
      bool opened = false;
      bool closed = false;
      uint entrySize = 1;
      std::vector<uint8_t> tmpDataBuffer; // temporary data buffer used to store data for scanline only drivers

    };

    Display();

    //static void makeStringsParameter(const char* name, const char** strings, int count, UserParameter& parameter);
    static void makeStringsParameter(const std::string& name, const std::vector<std::string>& strings, UserParameter& parameter);
    static void makeIntsParameter(const std::string& name, const std::vector<int>& ints, UserParameter& parameter);
    static void makeFloatsParameter(const std::string& name, const std::vector<float>& floats, UserParameter& parameter);

 private:
    DisplayType mDisplayType = DisplayType::NONE;

    std::string mDriverName = "";
    void* mLibHandle = nullptr;

    bool mOverwriteSupport = false;
    bool mInteractiveSupport = false;
    bool mForceScanLines = false;

    std::vector<UserParameter>      mUserParameters;

    //PtDspyImageHandle             mImage;
    std::vector<ImageData>          mImages;
    PtFlagStuff                     mFlagstuff;

    PtDspyOpenFuncPtr               mOpenFunc = nullptr;
    PtDspyWriteFuncPtr              mWriteFunc = nullptr;
    PtDspyDeepWriteFuncPtr          mDeepWriteFunc = nullptr;
    PtDspyActiveRegionFuncPtr       mActiveRegionFunc = nullptr;
    PtDspyCloseFuncPtr              mCloseFunc = nullptr;
    PtDspyFlushFuncPtr              mFlushFunc = nullptr;
    PtDspyReopenFuncPtr             mReopenFunc = nullptr;
    PtDspyDelayCloseFuncPtr         mDelayCloseFunc = nullptr;
    PtDspyQueryFuncPtr              mQueryFunc = nullptr;

};

}  // namespace lava

#endif  // SRC_LAVA_LIB_DISPLAY_H_