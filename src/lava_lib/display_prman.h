#ifndef SRC_LAVA_LIB_DISPLAY_PRMAN_H_
#define SRC_LAVA_LIB_DISPLAY_PRMAN_H_

#include "lava_dll.h"

#include <map>
#include <string>
#include <vector>
#include <memory>

#include "display.h"
#include "prman/ndspy.h"

namespace lava {

class LAVA_API DisplayPrman: private Display {
  public:
    
    ~DisplayPrman();
    static SharedPtr create(Display::DisplayType display_type);


    bool openImage(const std::string& image_name, uint width, uint height, 
      const std::vector<Channel>& channels, uint &imageHandle, const std::vector<UserParameter>& userParams, const MetaData* pMetaData = nullptr) override;
    
    bool openImage(const std::string& image_name, uint width, uint height, Falcor::ResourceFormat format, uint &imageHandle, 
      const std::vector<UserParameter>& userParams, const std::string& channel_prefix, const MetaData* pMetaData = nullptr) override;
    
    bool closeImage(uint imageHandle) override;
    bool closeAll() override;

    bool sendImageRegion(uint imageHandle, uint x, uint y, uint width, uint height, const uint8_t *data) override;
    bool sendImage(uint imageHandle, uint width, uint height, const uint8_t *data) override;

    bool setStringParameter(const std::string& name, const std::vector<std::string>& strings) override;
    bool setIntParameter(const std::string& name, const std::vector<int>& ints) override;
    bool setFloatParameter(const std::string& name, const std::vector<float>& floats) override;

    bool opened(uint imageHandle) const final { return mImages[imageHandle].opened; }
    bool closed(uint imageHandle) const final { return mImages[imageHandle].closed; }

    const std::string& imageName(uint imageHandle) const final { return mImages[imageHandle].name; }
    uint imageWidth(uint imageHandle) const final { return mImages[imageHandle].width; }
    uint imageHeight(uint imageHandle) const final { return mImages[imageHandle].height; }

  private:
    struct ImageData {
      PtDspyImageHandle handle;
      std::string hash;
      std::string name = "";
      uint width = 0;
      uint height = 0;
      bool opened = false;
      bool closed = false;
      uint entrySize = 1;
      std::vector<uint8_t> tmpDataBuffer; // temporary data buffer used to store data for scanline only drivers

    };

    DisplayPrman();

  private:
    std::string mDriverName = "";
    void* mLibHandle = nullptr;

    std::vector<UserParameter>      mUserParameters;
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

#endif  // SRC_LAVA_LIB_DISPLAY_PRMAN_H_