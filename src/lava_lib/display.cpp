#include <memory>

#include <dlfcn.h>
#include <stdlib.h>
#include <boost/format.hpp>

#include "display.h"
#include "lava_utils_lib/logging.h"

// Make sure that we've got the correct sizes for the PtDspy* integral constants
BOOST_STATIC_ASSERT(sizeof(PtDspyUnsigned32) == 4);
BOOST_STATIC_ASSERT(sizeof(PtDspySigned32) == 4);
BOOST_STATIC_ASSERT(sizeof(PtDspyUnsigned16) == 2);
BOOST_STATIC_ASSERT(sizeof(PtDspySigned16) == 2);
BOOST_STATIC_ASSERT(sizeof(PtDspyUnsigned8) == 1);
BOOST_STATIC_ASSERT(sizeof(PtDspySigned8) == 1);

namespace lava {

static std::string getDspyErrorMessage(const PtDspyError& err) {
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

static unsigned getTypeFormat(Display::TypeFormat tformat) {
    switch (tformat) {
        case Display::TypeFormat::UNSIGNED32:
            return PkDspyUnsigned32;
        case Display::TypeFormat::SIGNED32:
            return PkDspySigned32;
        case Display::TypeFormat::UNSIGNED16:
            return PkDspyUnsigned16;
        case Display::TypeFormat::SIGNED16:
            return PkDspySigned16;
        case Display::TypeFormat::UNSIGNED8:
            return PkDspyUnsigned8;
        case Display::TypeFormat::SIGNED8:
            return PkDspySigned8;
        case Display::TypeFormat::FLOAT16:
            //return PkDspyFloat32;
            return PkDspySigned16;
        case Display::TypeFormat::FLOAT32:
        default:
            return PkDspyFloat32;
    }
}

static std::string getDisplayDriverFileName(Display::DisplayType display_type) {
    if (display_type != Display::DisplayType::NONE) {
        switch(display_type) {
            case Display::DisplayType::IP:
            case Display::DisplayType::MD:
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
            default:
                break;
        }
    }

    return "";
}

Display::Display() {
    mImages.clear();
}

Display::~Display() {
    if (!closeAll())
        std::cerr << "Error closing images !" << std::endl;

    if (mLibHandle) {
        dlclose(mLibHandle);
    }
}


Display::SharedPtr Display::create(Display::DisplayType display_type) {
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

    Display* pDisplay = new Display();
    
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

    return SharedPtr(pDisplay);
}

bool Display::openImage(const std::string& image_name, uint width, uint height, const std::vector<Channel>& channels, uint &imageHandle) {
    if( width == 0 || height == 0) {
        printf("[%s] Wrong image dimensions !!!\n", __FILE__);
        return false;
    }

    //format can be rgb, rgba or rgbaz for now...
    //int formatCount = 4;
    //std::vector<std::string> channels {"r", "g", "b", "a", "z"};

    PtDspyDevFormat *outformat = new PtDspyDevFormat[channels.size()]();
    PtDspyDevFormat *f_ptr = &outformat[0];
    for(const auto& channel: channels){
        f_ptr->type = getTypeFormat(channel.format);//PkDspyFloat32;

        const std::string& channel_name = channel.name;
        char* pname = reinterpret_cast<char*>(malloc(channel_name.size()+1));
        strcpy(pname, channel_name.c_str());
        
        f_ptr->name = pname;
        f_ptr++;
    }

    PtDspyImageHandle pvImage;
    PtDspyError err = mOpenFunc(&pvImage, mDriverName.c_str(), image_name.c_str(), width, height, mUserParameters.size(), mUserParameters.data(), 
        channels.size(), outformat, &mFlagstuff);

    PtDspySizeInfo img_info;

    // check for an error
    if(err != PkDspyErrorNone ) {
        LLOG_ERR << "Unable to open display \"" << mDriverName << "\" : ";
        LLOG_ERR << getDspyErrorMessage(err);
        return false;
    } else {
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
        if (mFlagstuff.flags & PkDspyFlagsWantsScanLineOrder)
            LLOG_DBG << "PkDspyFlagsWantsScanLineOrder";

        if (mFlagstuff.flags & PkDspyFlagsWantsEmptyBuckets)
            LLOG_DBG << "PkDspyFlagsWantsEmptyBuckets";

        if (mFlagstuff.flags & PkDspyFlagsWantsNullEmptyBuckets)
            LLOG_DBG << "PkDspyFlagsWantsNullEmptyBuckets";
    }

    imageHandle = mImages.size();

    ImageData imgData = {};
    imgData.name = image_name;
    imgData.handle = pvImage;
    imgData.width = img_info.width;
    imgData.height = img_info.height;
    imgData.opened = true;

    mImages.push_back(imgData);
    return true;
}

bool Display::closeAll() {
    bool ret = true;

    for (uint i = 0; i < mImages.size(); i++) {
        if (!closeImage(i)) {
            ret = false;
        }
    }

    //for (uint i = 0; i < mImages.size(); i++) {
    //    if(!close(i)) ret = false;
    //}
    
    mImages.clear();
    return ret;
}

bool Display::closeImage(uint imageHandle) {
    if(mImages[imageHandle].closed) // already closed
        return true;

    if(!mImages[imageHandle].handle) // mOpenFunc was unsuccessfull
        return false;

    LLOG_DBG << "Closing display " << mDriverName;
    PtDspyError err = mCloseFunc(mImages[imageHandle].handle);
    
    if(err != PkDspyErrorNone ) {
        LLOG_ERR << "Unable to close display " << mDriverName;
        LLOG_ERR << getDspyErrorMessage(err);
        return false; 
    }

    mImages[imageHandle].opened = false;
    mImages[imageHandle].closed = true;
    return true;
}

bool Display::sendBucket(uint imageHandle, int x, int y, int width, int height, const uint8_t *data) {
    if(!mImages[imageHandle].opened) {
        LLOG_ERR << "Can't send image data. Display not opened !!!";
        return false;
    }

    int entrysize = 4 * 2; // for testing... 4 channels 4 bytes(32bits) each

    PtDspyError err = mWriteFunc(mImages[imageHandle].handle, x, x+width, y, y+height, entrysize, data);
    if(err != PkDspyErrorNone ) {
        LLOG_ERR << getDspyErrorMessage(err);
        return false;
    }

    return true;
}

bool Display::sendImage(uint imageHandle, int width, int height, const uint8_t *data) {
    if(!mImages[imageHandle].opened) {
        LLOG_ERR << "Can't send image data. Display not opened !!!";
        return false;
    }

    if( width != mImages[imageHandle].width || height != mImages[imageHandle].height) {
        LLOG_ERR << "Display and sended image sizes are different !!!";
        return false;
    }

    if(mActiveRegionFunc) {
        LLOG_DBG << "Asking display to set active region";
        PtDspyError err = mActiveRegionFunc(mImages[imageHandle].handle, 0, width, 0, height);
            if(err != PkDspyErrorNone ) {
            LLOG_ERR << getDspyErrorMessage(err);
            return false;
        }
    }

    int entrysize = 4 * 2; // for testing... 4 channels 4 bytes(32bits) each

    uint32_t scanline_offset = width * entrysize;
    if (mFlagstuff.flags & PkDspyFlagsWantsScanLineOrder) {
        LLOG_DBG << "Sending " <<  std::to_string(height) << " scan lines";
        for(uint32_t y = 0; y < height; y++) {
            if(!sendBucket(imageHandle, 0, y, width,y, data)) 
                return false;

            data += scanline_offset;
        }

    } else {
        return sendBucket(imageHandle, 0, 0, width, height, data);
    }

    return true;
}

bool Display::setStringParameter(const std::string& name, const std::vector<std::string>& strings) {
    LLOG_DBG << "String parameter " << name;
    //if(mOpened) {
    //    LLOG_ERR << "Can't push parameter. Display opened already !!!";
    //    return false;
    //}
    mUserParameters.push_back({});
    makeStringsParameter(name, strings, mUserParameters.back());
    return true;
}

bool Display::setIntParameter(const std::string& name, const std::vector<int>& ints) {
    LLOG_DBG << "Int parameter " << name;
    //if(mOpened) {
    //    LLOG_ERR << "Can't push parameter. Display opened already !!!";
    //    return false;
    //}
    mUserParameters.push_back({});
    makeIntsParameter(name, ints, mUserParameters.back());
    return true;
}

bool Display::setFloatParameter(const std::string& name, const std::vector<float>& floats) {
    LLOG_DBG << "Float parameter " << name;
    //if(mOpened) {
    //    LLOG_ERR << "Can't push parameter. Display opened already !!!";
    //    return false;
    //}
    mUserParameters.push_back({});
    makeFloatsParameter(name, floats, mUserParameters.back());
    return true;
}

/* static */
void Display::makeStringsParameter(const std::string& name, const std::vector<std::string>& strings, UserParameter& parameter) {
    // Allocate and fill in the name.
    char* pname = reinterpret_cast<char*>(malloc(name.size()+1));
    strcpy(pname, name.c_str());
    parameter.name = pname;
    
    // Allocate enough space for the string pointers, and the strings, in one big block,
    // makes it easy to deallocate later.
    int count = strings.size();
    int totallen = count * sizeof(char*);
    for ( uint i = 0; i < count; i++ )
        totallen += (strings[i].size()+1) * sizeof(char);

    char** pstringptrs = reinterpret_cast<char**>(malloc(totallen));
    char* pstrings = reinterpret_cast<char*>(&pstringptrs[count]);

    for ( uint i = 0; i < count; i++ ) {
        // Copy each string to the end of the block.
        strcpy(pstrings, strings[i].c_str());
        pstringptrs[i] = pstrings;
        pstrings += strings[i].size()+1;
    }

    parameter.value = reinterpret_cast<RtPointer>(pstringptrs);
    parameter.vtype = 's';
    parameter.vcount = count;
    parameter.nbytes = totallen;
}

void Display::makeIntsParameter(const std::string& name, const std::vector<int>& ints, UserParameter& parameter) {
    // Allocate and fill in the name.
    char* pname = reinterpret_cast<char*>(malloc(name.size()+1));
    strcpy(pname, name.c_str());
    parameter.name = pname;
    

    // Allocate an ints array.
    uint32_t count = ints.size();
    uint32_t totallen = count * sizeof(int);
    int* pints = reinterpret_cast<int*>(malloc(totallen));
    // Then just copy the whole lot in one go.
    memcpy(pints, ints.data(), totallen);
    parameter.value = reinterpret_cast<RtPointer>(pints);
    parameter.vtype = 'i';
    parameter.vcount = count;
    parameter.nbytes = totallen;
}

void Display::makeFloatsParameter(const std::string& name, const std::vector<float>& floats, UserParameter& parameter) {
    // Allocate and fill in the name.
    char* pname = reinterpret_cast<char*>(malloc(name.size()+1));
    strcpy(pname, name.c_str());
    parameter.name = pname;
    

    // Allocate an ints array.
    uint32_t count = floats.size();
    uint32_t totallen = count * sizeof(float);
    float* pfloats = reinterpret_cast<float*>(malloc(totallen));
    // Then just copy the whole lot in one go.
    memcpy(pfloats, floats.data(), totallen);
    parameter.value = reinterpret_cast<RtPointer>(pfloats);
    parameter.vtype = 'f';
    parameter.vcount = count;
    parameter.nbytes = totallen;
}

}  // namespace lava