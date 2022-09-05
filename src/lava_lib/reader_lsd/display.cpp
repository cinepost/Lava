#include <memory>
#include <array>

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

struct ChannelNamingDesc {
    Display::NamingScheme namingScheme;
    std::array<std::string, 4> channelNames;
};

const ChannelNamingDesc kChannelNameDesc[] = {
    // NamingScheme                             ChannelNames,
    { Display::NamingScheme::RGBA,                        {"r", "g", "b", "a"}},
    { Display::NamingScheme::XYZW,                        {"x", "y", "z", "w"}},
};

static inline const std::string& getChannelName(Display::NamingScheme namingScheme, uint32_t channelIndex) {
    assert(kChannelNameDesc[(uint32_t)namingScheme].namingScheme == namingScheme);
    return kChannelNameDesc[(uint32_t)namingScheme].channelNames[channelIndex];
}

static inline size_t getFormatSizeInBytes(Display::TypeFormat format) {
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
            return PkDspyFloat16;
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
            default:
                break;
        }
    }
    return "";
}

static inline Display::TypeFormat falcorTypeToDiplay(Falcor::FormatType format_type, uint32_t numChannelBits) {
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

static Display::Channel makeDisplayChannel(uint32_t index, Falcor::FormatType format_type, uint32_t numChannelBits, Display::NamingScheme namingScheme) {
    Display::Channel channel;
    channel.name = getChannelName(namingScheme, index);
    channel.format = falcorTypeToDiplay(format_type, numChannelBits);

    return channel;
};

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

    switch (pDisplay->mDisplayType) {
        case DisplayType::IP:
        case DisplayType::MD:
        case DisplayType::HOUDINI:
            pDisplay->mLiveUpdateSupport = true;
            break;
        default:
            break;
    }

    return SharedPtr(pDisplay);
}

bool Display::openImage(const std::string& image_name, uint width, uint height, Falcor::ResourceFormat format, uint &imageHandle) {
    std::vector<Channel> channels;

    Falcor::FormatType format_type = Falcor::getFormatType(format);
    uint32_t numChannels = Falcor::getFormatChannelCount(format);

    for( uint32_t i = 0; i < numChannels; i++) {
        uint32_t numChannelBits = Falcor::getNumChannelBits(format, (int)i);
        channels.push_back(makeDisplayChannel(i, format_type, numChannelBits, NamingScheme::RGBA));
    }

    return openImage(image_name, width, height, channels, imageHandle);
}

bool Display::openImage(const std::string& image_name, uint width, uint height, const std::vector<Channel>& channels, uint &imageHandle) {
    if( width == 0 || height == 0) {
        printf("[%s] Wrong image dimensions !!!\n", __FILE__);
        return false;
    }

    mEntrySize = 0;

    PtDspyDevFormat *outformat = new PtDspyDevFormat[channels.size()]();
    PtDspyDevFormat *f_ptr = &outformat[0];
    for(const auto& channel: channels){
        f_ptr->type = getTypeFormat(channel.format);//PkDspyFloat32;

        const std::string& channel_name = channel.name;
        char* pname = reinterpret_cast<char*>(malloc(channel_name.size()+1));
        strcpy(pname, channel_name.c_str());
        
        mEntrySize += getFormatSizeInBytes(channel.format);

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

    LLOG_DBG << "Closing display \"" << mDriverName << "\"";
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

bool Display::sendImageRegion(uint imageHandle, int x, int y, int width, int height, const uint8_t *data) {
    if(!mImages[imageHandle].opened) {
        LLOG_ERR << "Can't send image data. Display not opened !!!";
        return false;
    }

    PtDspyError err = mWriteFunc(mImages[imageHandle].handle, x, x+width, y, y+height, mEntrySize, data);
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

    uint32_t scanline_offset = width * mEntrySize;
    if (mFlagstuff.flags & PkDspyFlagsWantsScanLineOrder) {
        LLOG_DBG << "Sending " <<  std::to_string(height) << " scan lines";
        for(uint32_t y = 0; y < height; y++) {
            if(!sendImageRegion(imageHandle, 0, y, width,y, data)) 
                return false;

            data += scanline_offset;
        }

    } else {
        return sendImageRegion(imageHandle, 0, 0, width, height, data);
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