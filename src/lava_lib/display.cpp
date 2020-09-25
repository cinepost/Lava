#include <memory>

#include <dlfcn.h>
#include <stdlib.h>
#include <boost/format.hpp>

#include "display.h"
#include "lava_utils_lib/logging.h"

namespace lava {

Display::Display(const std::string& driver_name): mDriverName(driver_name) {
    mImageWidth = mImageHeight = 0;
    LLOG_DBG << "Display \"" << mDriverName << "\" constructed!";
}

Display::~Display() {
    LLOG_DBG << "Display \"" << mDriverName << "\" destructed!";
}

Display::UniquePtr Display::create(const std::string& driver_name) {
	char *error;
	char *lava_home = getenv("LAVA_HOME");

	boost::format libdspy_name("%1%/etc/d_%2%.so");
	libdspy_name % lava_home % driver_name;

	void* lib_handle = dlopen(libdspy_name.str().c_str(), RTLD_NOW);
	if (!lib_handle) {
        printf("[%s] Unable to load display driver: %s\n", __FILE__, dlerror());
        return nullptr;
    }

    Display* pDisplay = new Display(driver_name);
    
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

    return UniquePtr(pDisplay);
}

bool Display::open(const std::string& image_name, uint width, uint height) {
    if( width == 0 && height == 0) {
        printf("[%s] Wrong image dimensions !!!\n", __FILE__);
        return false;
    }

    mImageName = image_name;
    mImageWidth = width;
    mImageHeight = height;

    //format can be rgb, rgba, rgbaz or rgbz
    int formatCount = 3;
    //char* channels[5] = {"r","g","b","a","z"};
    char channels[6] = "rgbaz";

    PtDspyDevFormat *outformat = new PtDspyDevFormat[formatCount]();
    PtDspyDevFormat *f_ptr = &outformat[0];
    for(int i=0; i<formatCount; i++){
        f_ptr->type = PkDspyFloat32;
        f_ptr->name = &channels[i];
        f_ptr++;
    }

    UserParameter *prms = new UserParameter[2];

    const char *compression[1] = {"none"};
    makeStringsParameter("exrcompression", compression, 1, prms[0]);

    const char *pixeltype[1] = {"float"};
    makeStringsParameter("exrpixeltype", pixeltype, 1, prms[1]);

    PtDspyError err = mOpenFunc(&mImage, mDriverName.c_str(), mImageName.c_str(), mImageWidth, mImageHeight, 0, prms, formatCount, outformat, &mFlagstuff);

    delete [] prms;
    delete [] outformat;

    // check for an error
    if(err != PkDspyErrorNone ) {
        BOOST_LOG_TRIVIAL(error) << "Unable to open display \"" << mDriverName << "\" : ";
        switch (err) {
            case PkDspyErrorNoMemory:
                LLOG_FTL << "Out of memory";
                break;
            case PkDspyErrorUnsupported:
                LLOG_FTL << "Unsupported";
                break;
            case PkDspyErrorBadParams:
                LLOG_FTL << "Bad params";
                break;
            case PkDspyErrorNoResource:
                LLOG_FTL << "No resource";
                break;
            case PkDspyErrorUndefined:
            default:
                LLOG_FTL << "Undefined";
                break; 
        }
        return false;
    } else {
        // check image info
        if (mQueryFunc != nullptr) {
            PtDspySizeInfo img_info;

            LLOG_DBG << "Querying display image size " << mDriverName;
            err = mQueryFunc(mImage, PkSizeQuery, sizeof(PtDspySizeInfo), &img_info);
            if(err) {
                LLOG_ERR << "Unable to query image size info " << mDriverName;
                return false;
            } else {
                mImageWidth = img_info.width; mImageHeight = img_info.height;
                LLOG_DBG << "Open image size: " << mImageWidth << " " << mImageHeight << " aspect ratio: " << img_info.aspectRatio;
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
        return false; 
    }

    mClosed = true;
    return true;
}

void Display::makeStringsParameter(const char* name, const char** strings, int count, UserParameter& parameter) {
    // Allocate and fill in the name.
    char* pname = reinterpret_cast<char*>(malloc(strlen(name)+1));
    strcpy(pname, name);
    parameter.name = pname;
    
    // Allocate enough space for the string pointers, and the strings, in one big block,
    // makes it easy to deallocate later.
    int totallen = count * sizeof(char*);
    int i;
    for ( i = 0; i < count; i++ )
    totallen += (strlen(strings[i])+1) * sizeof(char);

    char** pstringptrs = reinterpret_cast<char**>(malloc(totallen));
    char* pstrings = reinterpret_cast<char*>(&pstringptrs[count]);

    for ( i = 0; i < count; i++ ) {
        // Copy each string to the end of the block.
        strcpy(pstrings, strings[i]);
        pstringptrs[i] = pstrings;
        pstrings += strlen(strings[i])+1;
    }

    parameter.value = reinterpret_cast<RtPointer>(pstringptrs);
    parameter.vtype = 's';
    parameter.vcount = count;
    parameter.nbytes = totallen;
}

}  // namespace lava