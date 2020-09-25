#ifndef SRC_LAVA_LIB_DISPLAY_H_
#define SRC_LAVA_LIB_DISPLAY_H_

#include <string>
#include <vector>
#include <memory>


#include "prman/ndspy.h"

namespace lava {

class Display {
 public:
    using UniquePtr = std::unique_ptr<Display>;

    ~Display();
    static UniquePtr create(const std::string& driver_name);

    bool open(const std::string& image_name, uint width, uint height);
    bool close();

 private:
    Display(const std::string& driver_name);

    void makeStringsParameter(const char* name, const char** strings, int count, UserParameter& parameter);

 private:
    std::string mDriverName = "";
    std::string mImageName = "";
    uint mImageWidth, mImageHeight;
    bool mClosed = false;

    PtDspyImageHandle   mImage;
    PtFlagStuff         mFlagstuff;

    PtDspyOpenFuncPtr               mOpenFunc = nullptr;
    PtDspyWriteFuncPtr              mWriteFunc;
    PtDspyDeepWriteFuncPtr          mDeepWriteFunc;
    PtDspyActiveRegionFuncPtr       mActiveRegionFunc;
    PtDspyCloseFuncPtr              mCloseFunc;
    PtDspyFlushFuncPtr              mFlushFunc;
    PtDspyReopenFuncPtr             mReopenFunc;
    PtDspyDelayCloseFuncPtr         mDelayCloseFunc;
    PtDspyQueryFuncPtr              mQueryFunc;

};

}  // namespace lava

#endif  // SRC_LAVA_LIB_DISPLAY_H_