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

Display::Display(): mOpened(false), mClosed(false) {
    mImageWidth = mImageHeight = 0;
}

Display::~Display() { }

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

	void* lib_handle = dlopen(libdspy_name.str().c_str(), RTLD_NOW);
	if (!lib_handle) {
        printf("[%s] Unable to load display driver: %s\n", __FILE__, dlerror());
        return nullptr;
    }

    Display* pDisplay = new Display();
    
    /* Necessary function pointers */

    pDisplay->mOpenFunc = (PtDspyOpenFuncPtr)dlsym(lib_handle, "DspyImageOpen");
    if ((error = dlerror()) != NULL)  {
        fputs(error, stderr);
        delete pDisplay;
        return nullptr;
    }

    pDisplay->mCloseFunc = (PtDspyCloseFuncPtr)dlsym(lib_handle, "DspyImageClose");
    if ((error = dlerror()) != NULL)  {
        fputs(error, stderr);
        delete pDisplay;
        return nullptr;
    }

    pDisplay->mWriteFunc = (PtDspyWriteFuncPtr)dlsym(lib_handle, "DspyImageData");
    if ((error = dlerror()) != NULL)  {
        fputs(error, stderr);
        delete pDisplay;
        return nullptr;
    }    

    pDisplay->mQueryFunc = (PtDspyQueryFuncPtr)dlsym(lib_handle, "DspyImageQuery");
    if ((error = dlerror()) != NULL)  {
        fputs(error, stderr);
        delete pDisplay;
        return nullptr;
    }

    /* Optional function pointers */

    pDisplay->mActiveRegionFunc = (PtDspyActiveRegionFuncPtr)dlsym(lib_handle, "DspyImageActiveRegion");
    if ((error = dlerror()) != NULL)  {
        pDisplay->mActiveRegionFunc = nullptr;
    }  

    pDisplay->mDriverName = display_driver_name;
    pDisplay->mDisplayType = display_type;

    return SharedPtr(pDisplay);
}

bool Display::open(const std::string& image_name, uint width, uint height, const std::vector<Channel>& channels) {
    if( width == 0 || height == 0) {
        printf("[%s] Wrong image dimensions !!!\n", __FILE__);
        return false;
    }

    mImageName = image_name;
    mImageWidth = width;
    mImageHeight = height;

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

    PtDspyError err = mOpenFunc(&mImage, mDriverName.c_str(), mImageName.c_str(), mImageWidth, mImageHeight, mUserParameters.size(), mUserParameters.data(), channels.size(), outformat, &mFlagstuff);

    // check for an error
    if(err != PkDspyErrorNone ) {
        LLOG_ERR << "Unable to open display \"" << mDriverName << "\" : ";
        LLOG_ERR << getDspyErrorMessage(err);
        return false;
    } else {
        // check image info
        if (mQueryFunc != nullptr) {
            PtDspySizeInfo img_info;

            err = mQueryFunc(mImage, PkSizeQuery, sizeof(PtDspySizeInfo), &img_info);
            if(err) {
                LLOG_ERR << "Unable to query image size info";
                LLOG_ERR << getDspyErrorMessage(err);
                return false;
            } else {
                mImageWidth = img_info.width; mImageHeight = img_info.height;
                LLOG_DBG << "Opened image name: " << mImageName << " size: " << mImageWidth << " " << mImageHeight << " aspect ratio: " << img_info.aspectRatio;
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

    mOpened = true;
    return true;
}

bool Display::close() {
    if(mClosed) // already closed
        return true;

    if(!mImage) // mOpenFunc was unsuccessfull
        return false;

    LLOG_DBG << "Closing display " << mDriverName;
    PtDspyError err = mCloseFunc(mImage);
    
    if(err) {
        LLOG_ERR << "Unable to close display " << mDriverName;
        LLOG_ERR << getDspyErrorMessage(err);
        return false; 
    }

    mOpened = false;
    mClosed = true;
    return true;
}

bool Display::sendBucket(int x, int y, int width, int height, const uint8_t *data) {
    if(!mOpened) {
        LLOG_ERR << "Can't send image data. Display not opened !!!";
        return false;
    }

    int entrysize = 4 * 4; // for testing... 4 channels 4 bytes(32bits) each

    PtDspyError err = mWriteFunc(mImage, x, x+width, y, y+height, entrysize, data);
    if(err != PkDspyErrorNone ) {
        LLOG_ERR << getDspyErrorMessage(err);
        return false;
    }

    return true;
}

bool Display::sendImage(int width, int height, const uint8_t *data) {
    if(!mOpened) {
        LLOG_ERR << "Can't send image data. Display not opened !!!";
        return false;
    }

    if( width != mImageWidth || height != mImageHeight) {
        LLOG_ERR << "Display and sended image sizes are different !!!";
        return false;
    }

    if(mActiveRegionFunc) {
        LLOG_DBG << "Asking display to set active region";
        PtDspyError err = mActiveRegionFunc(mImage, 0, width, 0, height);
            if(err != PkDspyErrorNone ) {
            LLOG_ERR << getDspyErrorMessage(err);
            return false;
        }
    }

    int entrysize = 4 * 4; // for testing... 4 channels 4 bytes(32bits) each

    uint32_t scanline_offset = width * entrysize;
    if (mFlagstuff.flags & PkDspyFlagsWantsScanLineOrder) {
        LLOG_DBG << "Sending " <<  std::to_string(height) << " scan lines";
        for(uint32_t y = 0; y < height; y++) {
            if(!sendBucket(0, y, width,y, data)) 
                return false;

            data += scanline_offset;
        }

    } else {
        return sendBucket(0, 0, width, height, data);
    }

    return true;
}

bool Display::setStringParameter(const std::string& name, const std::vector<std::string>& strings) {
    LLOG_DBG << "String parameter " << name;
    if(mOpened) {
        LLOG_ERR << "Can't push parameter. Display opened already !!!";
        return false;
    }
    mUserParameters.push_back({});
    makeStringsParameter(name, strings, mUserParameters.back());
    return true;
}

bool Display::setIntParameter(const std::string& name, const std::vector<int>& ints) {
    LLOG_DBG << "Int parameter " << name;
    if(mOpened) {
        LLOG_ERR << "Can't push parameter. Display opened already !!!";
        return false;
    }
    mUserParameters.push_back({});
    makeIntsParameter(name, ints, mUserParameters.back());
    return true;
}

bool Display::setFloatParameter(const std::string& name, const std::vector<float>& floats) {
    LLOG_DBG << "Float parameter " << name;
    if(mOpened) {
        LLOG_ERR << "Can't push parameter. Display opened already !!!";
        return false;
    }
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