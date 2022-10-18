#ifndef SRC_LAVA_LIB_DISPLAY_OIIO_H_
#define SRC_LAVA_LIB_DISPLAY_OIIO_H_

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "display.h"

namespace lava {

namespace oiio = OIIO;

class DisplayOIIO: private Display {
  public:
    
    ~DisplayOIIO();
    static SharedPtr create(Display::DisplayType display_type);

    static bool isDiplayTypeSupported(Display::DisplayType display_type);

    virtual bool openImage(const std::string& image_name, uint width, uint height, const std::vector<Channel>& channels, uint &imageHandle) override;
    virtual bool openImage(const std::string& image_name, uint width, uint height, Falcor::ResourceFormat format, uint &imageHandle, std::string channel_prefix = "") override;
    virtual bool closeImage(uint imageHandle) override;
    virtual bool closeAll() override;

    virtual bool sendImageRegion(uint imageHandle, uint x, uint y, uint width, uint height, const uint8_t *data) override;
    virtual bool sendImage(uint imageHandle, uint width, uint height, const uint8_t *data) override;

    virtual bool setStringParameter(const std::string& name, const std::vector<std::string>& strings) override;
    virtual bool setIntParameter(const std::string& name, const std::vector<int>& ints) override;
    virtual bool setFloatParameter(const std::string& name, const std::vector<float>& floats) override;

    virtual bool opened(uint imageHandle) const final;
    virtual bool closed(uint imageHandle) const final;

    virtual const std::string& imageName(uint imageHandle) const final;
    virtual uint imageWidth(uint imageHandle) const final;
    virtual uint imageHeight(uint imageHandle) const final;

  private:
    struct ImageData {
      std::string name = "";
      uint width = 0;
      uint height = 0;
      oiio::ImageSpec spec;
      bool opened = false;
      bool closed = false;
      bool _isSubImage = false;

      bool supportsRandomAccess = false;
      bool supportsTiles = false;
      bool forceScanlines = false;
      bool supportsMultiImage = false;
      bool supportsAppendSubImage = false;
      bool supportsMipMaps = false;
      bool supportsPerChannelFormats = false;
      bool supportsRectangles = false;

      uint entrySize = 1;
      std::unique_ptr<oiio::ImageOutput> pOut;
      std::vector<Channel> channels;
      std::vector<uint8_t> tmpDataBuffer; // temporary data buffer used to store data for scanline only drivers

      uint masterImageHandle = 0;
      std::vector<uint> subImageHandles;

      inline bool isOpened() const { return this->opened; }
      inline bool isClosed() const { return this->closed; }
    
      inline bool isMultiImage() const { return this->subImageHandles.size() > 0;}
      inline bool isSubImage() const { return this->_isSubImage;}
    };

  private:
    bool   _open(uint imageHandle); // Open OIIO image itself
    inline bool hasImage(uint imageHandle) const { return mImages.find(imageHandle) != mImages.end(); }
    inline ImageData* masterImageData(uint imageHandle) {
      if(!hasImage(imageHandle)) return nullptr;
      auto& imData = mImages.at(imageHandle);
      if(!imData.isSubImage()) { LLOG_ERR << "Image " << std::to_string(imageHandle) << " is not a sub-image!"; return nullptr; }
      if(!hasImage(imData.masterImageHandle)) { LLOG_ERR << "No master image " << std::to_string(imData.masterImageHandle) << " found!"; return nullptr; }
      return &mImages.at(imData.masterImageHandle);
    }

    DisplayOIIO();

 private:
    std::atomic<unsigned int> mCurrentImageID;

    std::vector<UserParameter>            mUserParameters;
    std::unordered_map<uint, ImageData>   mImages;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_DISPLAY_OIIO_H_