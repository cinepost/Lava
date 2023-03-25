#include <memory>
#include <array>

#include <dlfcn.h>
#include <stdlib.h>
#include <boost/format.hpp>

#include "display_prman.h"
#include "lava_utils_lib/logging.h"

// Make sure that we've got the correct sizes for the PtDspy* integral constants
BOOST_STATIC_ASSERT(sizeof(PtDspyUnsigned32) == 4);
BOOST_STATIC_ASSERT(sizeof(PtDspySigned32) == 4);
BOOST_STATIC_ASSERT(sizeof(PtDspyUnsigned16) == 2);
BOOST_STATIC_ASSERT(sizeof(PtDspySigned16) == 2);
BOOST_STATIC_ASSERT(sizeof(PtDspyUnsigned8) == 1);
BOOST_STATIC_ASSERT(sizeof(PtDspySigned8) == 1);

namespace lava {

inline static std::string getDspyErrorMessage(const PtDspyError& err) {
    switch (err) {
        case PkDspyErrorNoMemory:
            return "Out of memory";
        case PkDspyErrorUnsupported:
            return "Unsupported";
        case PkDspyErrorBadParams:
            return "Bad params";
        case PkDspyErrorNoResource:
            return "No resource";
        case PkDspyErrorUndefined:
        default:
            return "Undefined"; 
    }
}

inline static unsigned getTypeFormat(Display::TypeFormat tformat) {
    switch (tformat) {
        case Display::TypeFormat::UNSIGNED32:
            return PkDspyUnsigned32;
        case Display::TypeFormat::SIGNED32:
            return PkDspySigned32;
        case Display::TypeFormat::UNSIGNED16:
            return PkDspyUnsigned16;
        case Display::TypeFormat::FLOAT16:
        case Display::TypeFormat::SIGNED16:
            return PkDspySigned16;
        case Display::TypeFormat::UNSIGNED8:
            return PkDspyUnsigned8;
        case Display::TypeFormat::SIGNED8:
            return PkDspySigned8;
        case Display::TypeFormat::FLOAT32:
        default:
            return PkDspyFloat32;
    }
}

inline static bool isHalfFloatFormat(Display::TypeFormat tformat) {
    return (tformat == Display::TypeFormat::FLOAT16) ? true : false;
}

DisplayPrman::DisplayPrman() {
    mImages.clear();
}

DisplayPrman::~DisplayPrman() {
    if (!closeAll())
        std::cerr << "Error closing images !" << std::endl;

    if (mLibHandle) {
        dlclose(mLibHandle);
    }
}


Display::SharedPtr DisplayPrman::create(Display::DisplayType display_type) {
	char *error;
	char *lava_home = getenv("LAVA_HOME");

    std::string display_driver_name = getDisplayDriverFileName(display_type);
    if (display_driver_name == "") {
        LLOG_ERR << "No display driver name specified !!!";
        return nullptr;
    }

	boost::format libdspy_name("%1%/etc/d_%2%.so");
	libdspy_name % lava_home % display_driver_name;

	void* mLibHandle = dlopen(libdspy_name.str().c_str(), RTLD_NOW);
	if (!mLibHandle) {
        printf("[%s] Unable to load display driver: %s\n", __FILE__, dlerror());
        return nullptr;
    }

    DisplayPrman* pDisplay = new DisplayPrman();
    
    /* Necessary function pointers */

    pDisplay->mOpenFunc = (PtDspyOpenFuncPtr)dlsym(mLibHandle, "DspyImageOpen");
    if ((error = dlerror()) != NULL)  {
        fputs(error, stderr);
        delete pDisplay;
        return nullptr;
    }

    pDisplay->mCloseFunc = (PtDspyCloseFuncPtr)dlsym(mLibHandle, "DspyImageClose");
    if ((error = dlerror()) != NULL)  {
        fputs(error, stderr);
        delete pDisplay;
        return nullptr;
    }

    pDisplay->mWriteFunc = (PtDspyWriteFuncPtr)dlsym(mLibHandle, "DspyImageData");
    if ((error = dlerror()) != NULL)  {
        fputs(error, stderr);
        delete pDisplay;
        return nullptr;
    }    

    pDisplay->mQueryFunc = (PtDspyQueryFuncPtr)dlsym(mLibHandle, "DspyImageQuery");
    if ((error = dlerror()) != NULL)  {
        fputs(error, stderr);
        delete pDisplay;
        return nullptr;
    }

    /* Optional function pointers */

    pDisplay->mActiveRegionFunc = (PtDspyActiveRegionFuncPtr)dlsym(mLibHandle, "DspyImageActiveRegion");
    if ((error = dlerror()) != NULL)  {
        pDisplay->mActiveRegionFunc = nullptr;
    }  

    pDisplay->mDriverName = display_driver_name;
    pDisplay->mDisplayType = display_type;

    LLOG_DBG << "Prman display driver " << pDisplay->mDriverName;
    LLOG_DBG << "Prman display type " << to_string(pDisplay->mDisplayType);

    switch (pDisplay->mDisplayType) {
        case DisplayType::IP:
        case DisplayType::MD:
        case DisplayType::HOUDINI:
            pDisplay->mInteractiveSupport = true;
            break;
        default:
            break;
    }

    return SharedPtr((Display*)pDisplay);
}

bool DisplayPrman::openImage(const std::string& image_name, uint width, uint height, Falcor::ResourceFormat format, uint &imageHandle, const std::vector<UserParameter>& userParams, std::string channel_prefix) {
    std::vector<Channel> channels;

    Falcor::FormatType format_type = Falcor::getFormatType(format);
    uint32_t numChannels = Falcor::getFormatChannelCount(format);

    for( uint32_t i = 0; i < numChannels; i++) {
        uint32_t numChannelBits = Falcor::getNumChannelBits(format, (int)i);
        channels.push_back(makeDisplayChannel(channel_prefix, i, format_type, numChannelBits, NamingScheme::RGBA));
    }
    return openImage(image_name, width, height, channels, imageHandle, userParams);
}

bool DisplayPrman::openImage(const std::string& image_name, uint width, uint height, const std::vector<Channel>& channels, uint &imageHandle, const std::vector<UserParameter>& userParams) {
    if( width == 0 || height == 0) {
        LLOG_FTL << "Wrong image dimensions !!!";
        return false;
    }

    uint entrySize = 0;
    bool halfFloatFormat = false;

    PtDspyDevFormat *outformat = new PtDspyDevFormat[channels.size()]();
    PtDspyDevFormat *f_ptr = &outformat[0];
    for(const auto& channel: channels){
        f_ptr->type = getTypeFormat(channel.format);
        halfFloatFormat = isHalfFloatFormat(channel.format);

        const std::string& channel_name = channel.name;
        char* pname = reinterpret_cast<char*>(malloc(channel_name.size()+1));
        strcpy(pname, channel_name.c_str());
        
        entrySize += getFormatSizeInBytes(channel.format);

        f_ptr->name = pname;
        f_ptr++;
    }

    PtDspyImageHandle pvImage;
    std::vector<UserParameter> _userParams = userParams;

    if(halfFloatFormat) {
        _userParams.push_back(Display::makeStringsParameter("type", {std::string("half")}));
    }

    PtDspyError err = mOpenFunc(&pvImage, mDriverName.c_str(), image_name.c_str(), width, height, _userParams.size(), _userParams.data(), channels.size(), outformat, &mFlagstuff);

    PtDspySizeInfo img_info;

    // check for an error
    if(err != PkDspyErrorNone ) {
        LLOG_ERR << "Unable to open display \"" << mDriverName << "\" : ";
        LLOG_ERR << getDspyErrorMessage(err);
        return false;
    }

    // check image info
    if (mQueryFunc != nullptr) {
        err = mQueryFunc(pvImage, PkSizeQuery, sizeof(PtDspySizeInfo), &img_info);
        if(err) {
            LLOG_ERR << "Unable to query image size info";
            LLOG_ERR << getDspyErrorMessage(err);
            return false;
        } else {
            LLOG_DBG << "Opened image name: " << image_name << " size: " << img_info.width << " " << img_info.height << " aspect ratio: " << img_info.aspectRatio;
        }
    }

    // check flags
    if (mFlagstuff.flags & PkDspyFlagsWantsScanLineOrder) {
        if (!mActiveRegionFunc) {
            mForceScanLines = true;
        }
        LLOG_DBG << "PkDspyFlagsWantsScanLineOrder";
    }

    if (mFlagstuff.flags & PkDspyFlagsWantsEmptyBuckets) {
        //mForceScanLines = false;
        LLOG_DBG << "PkDspyFlagsWantsEmptyBuckets";
    }

    if (mFlagstuff.flags & PkDspyFlagsWantsNullEmptyBuckets) {
        //mForceScanLines = false;
        LLOG_DBG << "PkDspyFlagsWantsNullEmptyBuckets";
    }

    // Check driver supports interactive updates
    PtDspyOverwriteInfo overwriteInfo;
    if (mQueryFunc != nullptr) {
        err = mQueryFunc(pvImage, PkOverwriteQuery, sizeof(PtDspyOverwriteInfo), &overwriteInfo);
        if(!err) {
            if(overwriteInfo.overwrite) {
                mOverwriteSupport = true;
            }
            if(overwriteInfo.interactive == 1) {
                mInteractiveSupport = true;
                mForceScanLines = false;
            }

            LLOG_DBG << "Image driver overwrite info: overwrite " << std::to_string(overwriteInfo.overwrite);
            LLOG_DBG << "Image dirver overwrite info: interactive " << std::to_string(overwriteInfo.interactive);
        }
    }

    // Check if driver supports interactive updates
    PtDspyRedrawInfo redrawInfo;
    if (mQueryFunc != nullptr) {
        err = mQueryFunc(pvImage, PkRedrawQuery, sizeof(PtDspyRedrawInfo), &redrawInfo);
        if(!err) {
            //overwrite_info.overwrite;
            if(redrawInfo.redraw == 1) {
                mOverwriteSupport = true;
                mInteractiveSupport = true;
                mForceScanLines = false;
            }
            LLOG_DBG << "Image driver redraw info: redraw " << std::to_string(redrawInfo.redraw);
        }
    }

    imageHandle = mImages.size();
    mImages.push_back({});

    auto& image_data = mImages.back();
    image_data.name = image_name;
    image_data.handle = pvImage;
    image_data.width = img_info.width;
    image_data.height = img_info.height;
    image_data.opened = true;
    image_data.entrySize = entrySize;

    if (mForceScanLines) {
        image_data.tmpDataBuffer.resize(image_data.width * image_data.height * image_data.entrySize);
        std::fill(image_data.tmpDataBuffer.begin(), image_data.tmpDataBuffer.end(), (uint8_t)0);
    }

    if (mForceScanLines) LLOG_WRN << "Performance warning !!! Display driver " << mDriverName << " wants scan lines only. We do this using temporary image buffer.";

    return true;
}

bool DisplayPrman::closeAll() {
    bool ret = true;

    for (uint i = 0; i < mImages.size(); i++) {
        if (!closeImage(i)) {
            ret = false;
        }
    }
    
    mImages.clear();
    return ret;
}

bool DisplayPrman::closeImage(uint imageHandle) {
    auto& image_data = mImages[imageHandle];

    if(image_data.closed) // already closed
        return true;

    if(!image_data.handle) // mOpenFunc was unsuccessfull
        return false;

    bool result = true;

    // If driver expects ONLY scan lines then the data is stored in temporary buffer
    if (mForceScanLines) {
        LLOG_DBG << "Sending " <<  std::to_string(image_data.height) << " forced scan lines from temporary buffer";
        
        uint32_t scanline_offset = image_data.width * image_data.entrySize;
        const uint8_t* pData = image_data.tmpDataBuffer.data();
        for(uint32_t y = 0; y < image_data.height; y++) {
            PtDspyError err = mWriteFunc(image_data.handle, 0, image_data.width, y, y + 1, image_data.entrySize, pData);
            if(err != PkDspyErrorNone ) {
                LLOG_ERR << getDspyErrorMessage(err);
                result = false;
                break;
            }
            pData += scanline_offset;
        }
    }

    LLOG_DBG << "Closing display \"" << mDriverName << "\"";
    PtDspyError err = mCloseFunc(image_data.handle);
    
    if(err != PkDspyErrorNone ) {
        LLOG_ERR << "Unable to close display " << mDriverName << " ! Here is details: ";
        LLOG_ERR << getDspyErrorMessage(err);
        return false; 
    }

    image_data.opened = false;
    image_data.closed = true;
    return result;
}

bool DisplayPrman::sendImageRegion(uint imageHandle, uint x, uint y, uint width, uint height, const uint8_t *pData) {
    auto const& image_data = mImages[imageHandle];

    if(!image_data.opened) {
        LLOG_ERR << "Can't send image data. Display not opened !!!";
        return false;
    }

    if(!mForceScanLines) {
        // Send data to direct
        PtDspyError err = mWriteFunc(image_data.handle, x, x + width, y, y + height, image_data.entrySize, pData);
        if(err != PkDspyErrorNone ) {
            LLOG_ERR << getDspyErrorMessage(err);
            return false;
        }
    } else {
        // Store data to temporary buffer
        size_t src_data_line_size = image_data.entrySize * width;
        size_t dst_data_line_size = image_data.entrySize * image_data.width;
        
        const uint8_t* pSrcData = pData;
        uint8_t* pDstData = (uint8_t *)image_data.tmpDataBuffer.data() + ((y * image_data.width) + x) * image_data.entrySize;

        for(uint i = y; i < (y + height); i++) {
            if(pData) ::memcpy(pDstData, pSrcData, src_data_line_size);
            else ::memset(pDstData, 0, src_data_line_size);
            pSrcData += src_data_line_size;
            pDstData += dst_data_line_size;
        }
    }

    return true;
}

bool DisplayPrman::sendImage(uint imageHandle, uint width, uint height, const uint8_t *pData) {
    auto const& image_data = mImages[imageHandle];

    if(!image_data.opened) {
        LLOG_ERR << "Can't send image data. Display not opened !!!";
        return false;
    }

    if( width != image_data.width || height != image_data.height) {
        LLOG_ERR << "Display and sended image sizes are different !!!";
        return false;
    }

    if(mActiveRegionFunc) {
        LLOG_DBG << "Asking display to set active region";
        PtDspyError err = mActiveRegionFunc(image_data.handle, 0, width, 0, height);
            if(err != PkDspyErrorNone ) {
            LLOG_ERR << getDspyErrorMessage(err);
            return false;
        }
    }

    if(mForceScanLines) {
        if (pData) ::memcpy(mImages[imageHandle].tmpDataBuffer.data(), pData, width * height * image_data.entrySize);
        else ::memset (mImages[imageHandle].tmpDataBuffer.data(), 0, width * height * image_data.entrySize);
    } else {
        return sendImageRegion(imageHandle, 0, 0, width, height, pData);
    }

    return true;
}

bool DisplayPrman::setStringParameter(const std::string& name, const std::vector<std::string>& strings) {
    LLOG_DBG << "String parameter " << name;
    //if(mOpened) {
    //    LLOG_ERR << "Can't push parameter. Display opened already !!!";
    //    return false;
    //}
    mUserParameters.push_back({});
    Display::makeStringsParameter(name, strings, mUserParameters.back());
    return true;
}

bool DisplayPrman::setIntParameter(const std::string& name, const std::vector<int>& ints) {
    LLOG_DBG << "Int parameter " << name;
    //if(mOpened) {
    //    LLOG_ERR << "Can't push parameter. Display opened already !!!";
    //    return false;
    //}
    mUserParameters.push_back({});
    Display::makeIntsParameter(name, ints, mUserParameters.back());
    return true;
}

bool DisplayPrman::setFloatParameter(const std::string& name, const std::vector<float>& floats) {
    LLOG_DBG << "Float parameter " << name;
    //if(mOpened) {
    //    LLOG_ERR << "Can't push parameter. Display opened already !!!";
    //    return false;
    //}
    mUserParameters.push_back({});
    Display::makeFloatsParameter(name, floats, mUserParameters.back());
    return true;
}

}  // namespace lava