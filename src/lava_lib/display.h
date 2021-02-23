#ifndef SRC_LAVA_LIB_DISPLAY_H_
#define SRC_LAVA_LIB_DISPLAY_H_

#include <map>
#include <string>
#include <vector>
#include <memory>


#include "prman/ndspy.h"

namespace lava {

class Display {
 public:
    enum class DisplayType { NONE, NUL, IP, MD, OPENEXR, JPEG, TIFF, PNG, SDL, IDISPLAY };

 public:
    using SharedPtr = std::shared_ptr<Display>;

    ~Display();
    static SharedPtr create(Display::DisplayType display_type);


    bool open(const std::string& image_name, uint width, uint height);
    bool close();

    bool opened() { return mOpened; }
    bool closed() { return mClosed; }

    DisplayType type() const { return mDisplayType; };

    uint width() { return mImageWidth; };
    uint height() { return mImageHeight; };

    bool setStringParameter(const std::string& name, const std::vector<std::string>& strings);
    bool setIntParameter(const std::string& name, const std::vector<int>& ints);
    bool setFloatParameter(const std::string& name, const std::vector<float>& floats);

    bool sendBucket(int x, int y, int width, int height, const uint8_t *data);
    bool sendImage(int width, int height, const uint8_t *data);

 private:
    Display();

    //static void makeStringsParameter(const char* name, const char** strings, int count, UserParameter& parameter);
    static void makeStringsParameter(const std::string& name, const std::vector<std::string>& strings, UserParameter& parameter);
    static void makeIntsParameter(const std::string& name, const std::vector<int>& ints, UserParameter& parameter);
    static void makeFloatsParameter(const std::string& name, const std::vector<float>& floats, UserParameter& parameter);

 private:
    DisplayType mDisplayType = DisplayType::NONE;

    std::string mDriverName = "";
    std::string mImageName = "";
    uint mImageWidth, mImageHeight;
    bool mOpened = false;
    bool mClosed = false;

    std::vector<UserParameter>      mUserParameters;

    PtDspyImageHandle   mImage;
    PtFlagStuff         mFlagstuff;

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