/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Side Effects Software Inc
 *	123 Front Street West, Suite 1401
 *	Toronto, Ontario
 *	Canada   M5J 2M2
 *	416-504-9876
 *
 * NAME:	d_houdini.h ( RenderMan Display Driver, C++)
 *
 * COMMENTS:
 * Updated to support PRMan progressive refinement mode by Luke Emrose for Animal Logic 2013
 */


// This next bit is a hacky workaround to use prman's ndspy.h header while not
// actually linking to the lib.
#define PRMANBUILDINGAPI
#include <prman/ndspy.h>
#if !defined(PRMANAPI)
#  if defined(_MSC_VER)
#    define PRMANAPI __declspec(dllexport)
#  else
#    define PRMANAPI
#  endif
#endif


#include "d_houdini.h"
#include "dspyhlpr.c"
#include <stdarg.h>
#include <sstream>

#if 1
	#define D_HOUDINI_DEBUG_LEVEL 3	// Debug logging
#else
	#define D_HOUDINI_DEBUG_LEVEL -1	// No logging
#endif

#if defined(WIN32)
	#define GETPID() 0
#else
	#include <unistd.h>
	#define GETPID() getpid()
#endif

using namespace std;

// One master image for each DspyOpen on an non-LOD image. prman can have
// multiple Display calls in a RIB leading to one DspyOpen each. Sometimes AOV's
// are split out that way.
vector<ImagePtr> g_masterImages;

// One multiRes per DspyOpen, prman may open the same image name multiple times
// at different LOD's.
vector<MultiResPtr> g_multiRes;

static void
log(const int level, const char* fmt, ...) {
	va_list	 args;
	static FILE* log = 0;
	if (level < 0) {
		if (log)
			fprintf(log, "=== Close Log[%d] ===\n", GETPID());
	}
	else if (fmt && D_HOUDINI_DEBUG_LEVEL > level) {
		if (!log) {
			log = fopen("/tmp/ri_display.log", "w");
			if (log)
				fprintf(log, "=== OpenLog[%d] ===\n", GETPID());
		}
		if (log) {
			va_start(args, fmt);
			vfprintf(log, fmt, args);
			va_end(args);
		}
	}
	if (log)
		fflush(log);
}

#ifdef WIN32
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

// All this popen/pclose code for Windows was copied over from
// UT_NTStreamUtil.C.

HANDLE
getNewNamedPipe(const char* pipeName) {
	HANDLE pipe = NULL;
	SECURITY_ATTRIBUTES sec_info = {sizeof( sec_info ), NULL, FALSE};
	static int numPipes = 0;

	do {
		if (pipe)
			CloseHandle(pipe);
		sprintf(const_cast<char*>(pipeName), 
			"//./pipe/HoudiniUTWritePipe%d", numPipes++);
		pipe = CreateFile(pipeName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	} while(GetLastError() != ERROR_FILE_NOT_FOUND);

	if(pipe)
		CloseHandle(pipe);

	pipe = CreateNamedPipe(pipeName, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE,
				  PIPE_UNLIMITED_INSTANCES, 4096, 4096,
				  NMPWAIT_USE_DEFAULT_WAIT, &sec_info);

	return pipe;
}
		
class PipeData {
public:
	PipeData() {
		fileptr = NULL;
		hProc = NULL;
		hPipe = NULL;
	}
	
	FILE* fileptr;
	HANDLE hProc, hPipe;
};

static PipeData* pipeArray = NULL;

// This function is a general pipe routine. It allows the caller to
// set the in/out/err pipes of the created process by passing them
// in. If values aren't passed in, in and out pipes are created,
// and the out parameter is set to the read end of the pipe
// connected to stdout, and the in parameter is set to the write
// end of the pipe connected to stdin. All handles that are not
// passed back to the caller are closed automatically. The caller
// must close all other handles.

void*
UnixPOpen(const std::string& file, const void** in, const void** out, const void** err) {
	SECURITY_ATTRIBUTES sec = {sizeof(sec), NULL, TRUE};
	PROCESS_INFORMATION pinfo;
	STARTUPINFO sinfo;
	HANDLE inPipe = NULL, outPipe = NULL, errPipe = NULL, hRet = NULL;
	char inPipeName[256], outPipeName[256], errPipeName[256];

	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.cb = sizeof(sinfo);
	sinfo.dwFlags = STARTF_USESTDHANDLES;
	
	if(in)
		sinfo.hStdInput = (HANDLE)*in;
	if(out)
		sinfo.hStdOutput = (HANDLE)*out;
	if(err)
		sinfo.hStdError = (HANDLE)*err;

	if(!sinfo.hStdInput && in) {
		inPipe = getNewNamedPipe(inPipeName);
		sinfo.hStdInput = CreateFile( inPipeName, GENERIC_READ, 
						  FILE_SHARE_READ, &sec, OPEN_EXISTING,
						  0, NULL );
		*in = inPipe;
	}
	
	if(!sinfo.hStdOutput && out) {
		outPipe = getNewNamedPipe(outPipeName);
		sinfo.hStdOutput = CreateFile( outPipeName, GENERIC_WRITE, 
							FILE_SHARE_WRITE, &sec, OPEN_EXISTING,
							0, NULL );
		*out = outPipe;
	}
	
	if(!sinfo.hStdError && err) {
		errPipe = getNewNamedPipe(errPipeName);
		sinfo.hStdError = CreateFile( errPipeName, GENERIC_WRITE, 
						   FILE_SHARE_WRITE, &sec, OPEN_EXISTING,
						   0, NULL );
		*err = errPipe;
	}

	if(CreateProcess(NULL, (char*)file.c_str(), NULL, NULL, TRUE,
		CREATE_NO_WINDOW, // Use this flag to avoid popping up
		// a console window everytime. But
		// this means we can't pass our
		// stdin, out, or err handles.
		NULL, NULL, &sinfo, &pinfo)) {

		if(inPipe)
			CloseHandle(sinfo.hStdInput);
		if(outPipe)
			CloseHandle(sinfo.hStdOutput);
		if(errPipe)
			CloseHandle(sinfo.hStdError);

		CloseHandle(pinfo.hThread);
		hRet = pinfo.hProcess;
	} else {
		if(inPipe) {
			CloseHandle(sinfo.hStdInput);
			CloseHandle(inPipe);
			*in = NULL;
		}

		if(outPipe) {
			CloseHandle(sinfo.hStdOutput);
			CloseHandle(outPipe);
			*out = NULL;
		}

		if(errPipe) {
			CloseHandle(sinfo.hStdError);
			CloseHandle(errPipe);
			*err = NULL;
		}
	}
	
	return hRet;
}

// Uses the general UnixPOpen function above to simulate a popen call.

FILE*
UnixPOpen(const std::string& file, const std::string& mode)
{
	int tmpPipe = -1;
	FILE* pipe = NULL;
	int doRead = 0, doWrite = 0;
	HANDLE in, out, hProc;
   
	if(strchr(mode.c_str(), 'r'))
		doRead = 1;
	if(strchr(mode.c_str(), 'w'))
		doWrite = 1;

	if((!doRead && !doWrite) || (doRead && doWrite))
		return NULL;

	if(!pipeArray) {
		pipeArray = new PipeData[64];
	}
	
	// Because we start processes with the CREATE_NO_WINDOW flag,
	// we cannot legally pass in handles to our console.  We were
	// doing this before if doRead and doWrite were not set, and it
	// was causing crashes.  Note that this means it is not possible
	// to use this function to execute a program that takes input
	// from a pipe but puts output to a console window.  If you want
	// that, you're going to have to add a new parameter to UnixPOpen
	// to turn off the CREATE_NO_WINDOW flag.
	out = NULL;
	in  = NULL;

	hProc = UnixPOpen(file.c_str(), (const void**) &in, (const void**) &out,
			 NULL);
	if(hProc) {
		if(doRead) {
			tmpPipe = _open_osfhandle((intptr_t)out, _O_RDONLY|_O_BINARY);
		} else {
			tmpPipe = _open_osfhandle((intptr_t)in, _O_WRONLY|_O_BINARY);
		}
	}

	if(tmpPipe != -1) {
		int i = 0;
		pipe = fdopen(tmpPipe, doRead ? "rb" : "wb");
		while(pipeArray[i].fileptr)
			i++;
		pipeArray[i].fileptr = pipe;
		pipeArray[i].hProc = hProc;
		pipeArray[i].hPipe = doRead ? out : in;
	} else {
		if(doRead)
			CloseHandle(out);
		if(doWrite)
			CloseHandle(in);
	}
	if(doWrite)
		CloseHandle(out);
	if(doRead)
		CloseHandle(in);

	return pipe;
}

int
UnixPClose(const FILE* file)
{
	int i = 0;
	
	if(!pipeArray)
		return -1;
	while(i < 64 && pipeArray[i].fileptr != file)
		++i;
	if(i < 64) {
		unsigned long retval;

		fflush(const_cast<FILE*>(file));
		FlushFileBuffers(pipeArray[i].hPipe);
		DisconnectNamedPipe(pipeArray[i].hPipe);
		fclose(const_cast<FILE*>(file));
		WaitForSingleObject(pipeArray[i].hProc, INFINITE);
		GetExitCodeProcess(pipeArray[i].hProc, &retval);
		pipeArray[i].fileptr = NULL;
		CloseHandle(pipeArray[i].hProc);
		pipeArray[i].hProc = NULL;
		pipeArray[i].hPipe = NULL;
		return (int)retval;
	}

	return -1;
}

FILE* popen(const std::string& name, const std::string& mode) {
	return UnixPOpen(name, mode);
}

int pclose(FILE* f) {
	return UnixPClose(f);
}
#endif

#define MAGIC (('h'<<24)+('M'<<16)+('P'<<8)+('0'))
#define END_OF_IMAGE -2


//
// Class for holding the FILE* to imdisplay process. There is only
// one instance of this class but it instance of H_Image has a shared_ptr to
// it. When the last H_Image is closed (deleted) this class close the FILE*
//
class IMDisplay {
public:
    static IMDisplayPtr Singleton(const char* argv) {
        if (!s_IMD) {
            s_IMD = IMDisplayPtr(new IMDisplay(argv));
        }
        return s_IMD;
    }
    
    ~IMDisplay() {
        log(0, "closing IMDisplay. Closing pipe:%s\n", myPipe ? "True" :"False");
        if (myPipe) {
            int header[4];
            ::memset(header, 0, sizeof(header));
            header[0] = END_OF_IMAGE;
            fwrite(header, sizeof(int), 4, myPipe);

            ::pclose(myPipe);
            myPipe = (FILE*)0;
        }
    }
    
    bool
    IsValid(void) { return myPipe != (FILE*)0; }
    
    FILE*
    GetFile(void) { return myPipe; }
    
private:

    IMDisplay(const char* argv)
    {
        myPipe = ::popen(argv, "w");
    }
    
    
    FILE* myPipe;
    
    static IMDisplayPtr s_IMD;
};

/* static */
IMDisplayPtr IMDisplay::s_IMD;

class H_ChanDef {
public:
	H_ChanDef() {init("");};
	H_ChanDef(const std::string& name) {init(name);};
	~H_ChanDef() {log(0, "~H_ChanDef\n");}
	void init(const std::string& name) {
		myName = name;
		myOffset[0] = myOffset[1] = myOffset[2] = myOffset[3] = -1;
		mySize = 0;
		log(0, "H_ChanDef init\n");
	}

	void destroy() {
		myName.clear();
		log(0, "H_ChanDef destroy\n");
	}

#if D_HOUDINI_DEBUG_LEVEL > 0
	void dump(const int idx) {
		int i;
		log(0, "Channel[%d]: '%s'[%d] %d [", idx, myName.c_str(), mySize, myFormat);
		for (i = 0; i < mySize; ++i)
			log(0, " %d", myOffset[i]);
		log(0, "]\n");
	}
#else

	void dump(const int) {}

#endif
	std::string myName;
	int myFormat;
	int mySize;
	int myOffset[4];
	int isValid() const {
		int i;
		if (myName.empty() || mySize < 1 || mySize == 2 || mySize > 4)
			return 0;
		for (i = 0; i < mySize; i++)
			if (myOffset[i] < 0)
				return 0;
		return 1;
	}
};

static void
addChanDef(const PtDspyDevFormat& def,
           vector<h_shared_ptr< H_ChanDef > >& list, const int foff) {
	char* name;
	const char* dot;
	std::string prefix;
	int i, offset;
	int format;

	// Map the RIB types to the types expected by imdisplay
	switch (def.type & PkDspyMaskType) {
		case PkDspyFloat32:
			format = 0;
			break;
		case PkDspyUnsigned32:
		case PkDspySigned32:
			format = 4;
			break;
		case PkDspyUnsigned16:
		case PkDspySigned16:
			format = 2;
			break;
		case PkDspyUnsigned8:
		case PkDspySigned8:
			format = 1;
			break;
		default:
			return;
	}
	name = def.name;
	if(name)
		prefix = name;

	// Find the offset by extension
	dot = strrchr(prefix.c_str(), '.');
	if (dot)
		dot++;
	else
		dot = name;

	offset = -1;		// Put it in the next available slot
	if (!strcmp(dot, "r") || !strcmp(dot, "x"))
		offset = 0;
	else if (!strcmp(dot, "g") || !strcmp(dot, "y"))
		offset = 1;
	else if (!strcmp(dot, "b") || !strcmp(dot, "z"))
		offset = 2;
	else if (!strcmp(dot, "a"))
		offset = 3;

	dot = strrchr(prefix.c_str(), '.');
	if (!dot) {
		if (!strcmp(name, "r") || !strcmp(name, "g") || !strcmp(name, "b") ||
                                                        !strcmp(name, "a")) {
			prefix = "C";
        } else if (!strcmp(name, "z")) {
			offset = 0;
			prefix = "Z";
		}
		else
			return;
    } else {
        // look for lpe:diffuse.000.r names and turn that into lpe:diffuse.
        // Might need to be a bit more thorough about this determination, maybe
        // check for all digits between the dots?
        prefix.assign(name, dot - prefix.c_str());
        dot = strrchr(prefix.c_str(), '.');
        if (dot) {
            prefix.assign(name, dot - prefix.c_str());
        } else {
            // nothing sensible, revert
            if (name) {
                prefix = name;
            } else {
                prefix = "";
            }
        }
    }

	// See if we've started declaring the channel yet
	for (i = 0; i < list.size(); ++i) {
		if (list[i]->myName == prefix) {
			// If the format doesn't match, then we are in hot water.
			if (list[i]->myFormat != format)
				return;
			break;	// We've found our definition
		}
	}

	// If no channel is defined yet, we create one
	if (i == list.size()) {
		list.push_back(h_shared_ptr< H_ChanDef >(new H_ChanDef(prefix)));
	}

	// Now, continue setting the channel
	if (offset < 0) {
		offset = list[i]->mySize;
		if (offset > 3)
			return;
	}
	list[i]->myFormat = format;
	list[i]->myOffset[offset] = foff;
	if (offset >= list[i]->mySize)
		list[i]->mySize = offset+1;
}

static int
addImageChannels(ImagePtr img, const int nformats,
                    const PtDspyDevFormat* formats)
{
	int i, ok;
	vector< h_shared_ptr< H_ChanDef > > defs;

#if D_HOUDINI_DEBUG_LEVEL > 0
	log(0, "SCAN %d formats\n", nformats);
	for (i = 0; i < nformats; ++i) {
		unsigned int format_type = formats[i].type & PkDspyMaskType;
		const char* type;

		switch (format_type) {
			case PkDspyFloat32:
				type = "PkDspyFloat32";
				break;
			case PkDspySigned16:
				type = "PkDspySigned16";
				break;
			case PkDspyUnsigned16:
				type = "PkDspyUnsigned16";
				break;
			case PkDspyUnsigned8:
				type = "PkDspyUnsigned8";
				break;
			case PkDspySigned8:
				type = "PkDspySigned8";
				break;
			default:
				type = "Unknown";
				break;
		}
		log(0, "Format[%d] = '%s' type %s (%d)\n",
			i, formats[i].name, type, format_type );
	}
#endif

    if (nformats < 1) {
		return 0;
    }

    for (i = 0; i < nformats; ++i) {
		addChanDef(formats[i], defs, i);
    }

	log(0, "Found %d unique channels\n", defs.size());

	ok = 0;
	for (i = 0; i < defs.size(); ++i) {
		if (defs[i]->isValid()) {
			defs[i]->dump(i);
			img->addChannel(defs[i]->myName, defs[i]->myFormat, defs[i]->mySize,
                            defs[i]->myOffset);
			ok++;
		} else {
			log(0, "---------- INVALID CHANNEL ----------\n");
			defs[i]->dump(i);
		}
		defs[i]->destroy();
	}
	return ok;
}

// store a single resolution of a multires render
// allows storage also of a single lod
H_MultiRes::~H_MultiRes()
{
	log(0, "H_MultiRes destructor: level: %d, xres: %d, yres: %d\n",
        myLevel, myXres, myYres);
}

void
H_MultiRes::init(const int xres, const int yres, const int level)
{
	// record the current multires lod resolution
	myXres = xres;
	myYres = yres;

	// calculate the scaling factor to transform from this
	// tile into the target tile of the full res display
    myXscale = (float)myImg->getXres() / (float)xres;
    myYscale = (float)myImg->getYres() / (float)yres;

	// record the current multires lod level
	myLevel = level;

	log(0, "H_MultiRes init: level: %d, xres: %d, yres: %d\n",
            myLevel, myXres, myYres);
}

static int dbgCont = 0;

PtDspyError
DspyImageOpen(PtDspyImageHandle* pvImage,
	const char*, // drivername
	const char* filename,
	int width,
	int height,
	int paramCount,
	const UserParameter* parameters,
	int nformats,
	PtDspyDevFormat* formats,
	PtFlagStuff* flagstuff)
{

	PtDspyError ret;

	log(0, "DspyImageOpen() drivername(%s) filename(%s) width(%d) height(%d) paramCount(%d) nformats(%d)\n",
		"drivername", filename, width, height, paramCount, nformats);

#ifndef WIN32
    char *doWait = getenv("HOUDINI_DSPY_WAIT");
    if (doWait) {
        log(0, "HOUDINI_DSPY_WAIT is set, sleeping in wait for debugger...\n");
        while (dbgCont == 0) {
            usleep(5 * 1000000);
            log(0, "dbgCont sleeping...\n");
        }
    }
#endif
	// default image size
	if (!width)
		width = 512;
	if (!height)
		height = 512;

	flagstuff->flags &= ~PkDspyFlagsWantsScanLineOrder;
	flagstuff->flags |=  PkDspyFlagsWantsEmptyBuckets;

	ret = PkDspyErrorNone;

    // look to see if this particular Open is an LOD
    const char* lodKey = ":lod:";
    const char* findLod = strstr(filename, lodKey );
    
    int currLod = 0;
    if ( findLod != NULL ) {
        currLod = atoi( findLod + strlen(lodKey) );
    }
    

	// only alloacate the main image reference if this is isn't a LOD image.
    // If it is a LOD we must find a previously opened main image (by name) to
    // attached to
    ImagePtr img;
	if (!findLod) {
		// only do this init once when the first defining initialization is made
		// (it's always made before the lods are init'ed)
		img = ImagePtr(new H_Image(filename? filename : "", width, height));
        if (!img) {
            log(0, "Failed creaiting main image. Name=%s", filename);
            return PkDspyErrorNoResource;
        }

		log(0, "Create: %p\n", img.get());
        g_masterImages.push_back(img);
        
        if (g_masterImages.size() == 1) {
            // initialize the options we might send to mplay or Houdini. Only
            // the first H_Image gets to open mplay so don't bother with
            // subsequent ones
            img->parseOptions(paramCount, parameters);
        }
    } else {
        // a LOD image
        vector<ImagePtr>::const_iterator i;
        size_t nameLen = findLod - filename;
        for (i = g_masterImages.begin() ; i != g_masterImages.end() ; i++) {
            if (strncmp(filename, (*i)->getName(), nameLen) == 0) {
                img = *i;
                break;
            }
        }
        if (!img) {
            log(0, "no main image for LOD found. LOD name=%s", filename);
            return PkDspyErrorNoResource;
        }
    }

	// let this H_Image know that it represents a particular LOD level

    if (currLod == 0) {
		if (!::addImageChannels(img, nformats, formats)) {
			g_masterImages.clear();
			return PkDspyErrorNoResource;
		}
	}

	// allocate a multi-res image pointer - with a reference back to the
    // main image.
    MultiResPtr mimg(new H_MultiRes(img, width, height, currLod));
    g_multiRes.push_back(mimg);
	H_MultiRes* multiRes = g_multiRes.back().get();

	// return the current multires pointer, which contains a reference to the
    // main H_Image we are interested in each multires level refers to the same
    // H_Image
	*pvImage = (void*)multiRes;
	log(0, "Done Open %d\n", ret);

	return ret;
}

PtDspyError
DspyImageData(PtDspyImageHandle pvImage,
	int xmin,
	int xmax,
	int ymin,
	int ymax,
	int entrysize,
	const unsigned char* data)
{
	H_MultiRes* multiRes = static_cast<H_MultiRes*>(pvImage);
    ImagePtr img = multiRes->getImage();
    if (!img) {
        return PkDspyErrorNoResource;
    }
    
    if (H_Image::openPipe() != 1) { // only opens one time.
        return PkDspyErrorNoResource;
    }
    
	log(0, "ImageData: %d %d %d %d Size=%d 0x%08x\n", xmin, xmax, ymin, ymax, entrysize, data);
	if (img->writeData(xmin, xmax, ymin, ymax, (const char*)data, entrysize, multiRes->getXscale(), multiRes->getYscale()) ) {
		log(0, "Done Image Write\n");
		return PkDspyErrorNone;
	}
	
	return PkDspyErrorNoResource;
}

struct isImage {
    isImage(ImagePtr i) : myImg(i) {}
    bool operator () ( const ImagePtr &img ) {
        return img == myImg;
    }
    ImagePtr myImg;
};

struct isMultiRes {
    isMultiRes(H_MultiRes* m) : myMR(m) {}
    bool operator () (const MultiResPtr &mr) {
        return mr.get() == myMR;
    }
    H_MultiRes* myMR;
};

PtDspyError
DspyImageClose(PtDspyImageHandle pvImage) {
    
    H_MultiRes* multiRes = static_cast<H_MultiRes*>(pvImage);
    
   
    isImage pred(multiRes->getImage());
    std::remove_if(g_masterImages.begin(), g_masterImages.end(), pred);
    
    
    isMultiRes pred2(multiRes);
    std::remove_if(g_multiRes.begin(), g_multiRes.end(), pred2);

	return PkDspyErrorNone;
}

PtDspyError
DspyImageQuery(PtDspyImageHandle pvImage,
		PtDspyQueryType query,
		int datalen,
		void* data)
{
    H_MultiRes* mr = static_cast<H_MultiRes*>(pvImage);

	// get the current multi-res level
	PtDspyError		 ret = PkDspyErrorNone;
	PtDspySizeInfo	 size_info;
	PtDspyMultiResolutionQuery multiResolution_info;

	log(1, "Image Query: %d/%d\n", query, datalen);
	memset(data, 0, datalen);
	switch (query) {
		case PkOverwriteQuery: {
			PtDspyOverwriteInfo	overinfo;
			if (datalen > sizeof(overinfo))
				datalen = sizeof(overinfo);
			overinfo.overwrite = 0;
			overinfo.interactive = 1;
			memcpy(data, &overinfo, datalen);
			break;
		}
		case PkRenderingStartQuery:
			break;

        case PkRedrawQuery:
        {
            PtDspyRedrawInfo redrawInfo;
            if (datalen > sizeof(redrawInfo))
                datalen = sizeof(redrawInfo);
            redrawInfo.redraw = 1;
            memcpy(data, &redrawInfo, datalen);
            break;
        }

		case PkSizeQuery:
            if (datalen > sizeof(size_info)) {
				datalen = sizeof(size_info);
            }
            
            size_info.width = mr->getImage()->getXres();
            size_info.height = mr->getImage()->getYres();

            size_info.aspectRatio = 1.0F;
			memcpy(data, &size_info, datalen);
			break;

		case PkMultiResolutionQuery:
            if (datalen > sizeof(multiResolution_info)) {
				datalen = sizeof(multiResolution_info);
            }
			multiResolution_info.supportsMultiResolution = 1;
			memcpy(data, &multiResolution_info, datalen);
			break;

		case PkSupportsCheckpointing:
		default:
			ret = PkDspyErrorUnsupported;
	}
	log(1, "Done Query\n");
	return ret;
}

//
//  Class implementation
///

void
H_Channel::init()
{
	myData.clear();
	myDSize = 0;
	log(0, "H_Channel init\n");
}

void
H_Channel::destroy()
{
	log(0, "H_Channel destroy\n");
}

int
H_Channel::open(const std::string& channel_name, const int format,
                    const int count, const int off[4])
{
	myName = channel_name;
	mySize = (format == 0)? sizeof(float) : format;
	myFormat = format;
	myCount = count;
	myPixelSize = mySize*count;
	memcpy(myMap, off, 4*sizeof(int));
	myMap[0] *= mySize;
	myMap[1] *= mySize;
	myMap[2] *= mySize;
	myMap[3] *= mySize;

	return 1;
}

void
H_Channel::startTile(const int xres, const int yres)
{
	int dsize = xres*yres*myPixelSize;
	if (dsize > myDSize) {
		myData.resize(dsize);
		myDSize = dsize;
	}
	myOffset = 0;
}

int
H_Channel::writePixel(const char* data, const int pixelOffset)
{
	int ch = pixelOffset * myCount;
	switch (myCount) {
		case 4:
			memcpy(&(myData[0]) + (ch+3)*mySize, data+myMap[3], mySize);
		case 3:
			memcpy(&(myData[0]) + (ch+2)*mySize, data+myMap[2], mySize);
		case 2:
			memcpy(&(myData[0]) + (ch+1)*mySize, data+myMap[1], mySize);
		case 1:
			memcpy(&(myData[0]) + (ch+0)*mySize, data+myMap[0], mySize);
	}

	return myPixelSize;
}

int
H_Channel::closeTile(FILE* fp, const int id,
                     const int x0, const int x1, const int y0, const int y1)
{
	int tile_head[4];
	size_t size;

	// Switch to the appropriate channel
	tile_head[0] = -1;
	tile_head[1] = id;
	tile_head[2] = 0;
	tile_head[3] = 0;

	if (fwrite(tile_head, sizeof(int), 4, fp) != 4)
		return 0;

	tile_head[0] = x0;
	tile_head[1] = x1 - 1;
	tile_head[2] = y0;
	tile_head[3] = y1 - 1;

	size = (x0-x1) * (y0-y1);

	// write the header data for this tile
	if (fwrite(tile_head, sizeof(int), 4, fp) != 4)
		return 0;

	// write out the actual pixel data
	if (fwrite(&(myData[0]), myPixelSize, size, fp) != size)
		return 0;

	// flush through the data to ensure it's all there
	if (fflush(fp) != 0)
		return 0;

	return 1;
}


void
H_Image::init(const std::string& filename, int xres, int yres)
{
	myName = (!filename.empty())? filename : "lava";
	myXoff = 0;
	myYoff = 0;
	myOrigXres = 0;
	myOrigYres = 0;
	myXres = xres;
	myYres = yres;
	myPort = 0;
    myChannelOffset = 0;

	if (!strncmp(myName.c_str(), "socket:", 7)) {
		myPort = atoi(myName.c_str() + 7);
		if (myPort > 0) {
			myName = "lava";
		}
		else
			myPort = 0;
	}
	if (!strncmp(myName.c_str(), "iprsocket:", 10)) {
		myPort = atoi(myName.c_str() + 10);
		if (myPort > 0) {
			myName = "lava";
			myPort = -myPort;	// Negate port to indicate it's for IPR
		}
		else
			myPort = 0;
	}
	log(0, "H_Image init\n");
}

void
H_Image::destroy() {
	myChannels.clear();
	log(0, "H_Image destroy\n");
	log(-1, 0);
}

int
H_Image::addChannel(const std::string& name, const int format, const int count, const int off[4]) {
	log(0, "  ADD CHANNEL '%s' -- %d\n", name.c_str(), off[0]);
	myChannels.push_back(h_shared_ptr< H_Channel >(new H_Channel));
	h_shared_ptr< H_Channel >& chan = myChannels.back();
	if (!chan->open(name, format, count, off)) {
		// delete the invalid channel
		myChannels.pop_back();
		return 0;
	}

	return 1;
}


//
// openPipe must be called before any writes to the imdisplay program but after
// all of the DspyOpen's have occured. To ensure this it is called in
// DspyImageData. If the connection to imdisplay has been made then we do
// nothing and return success. Otherwise we open up the pipe and then hand
// out a shared pointer to a IMDisplay, the purpose of the shared_ptr is to
// simply reference count the H_Image's underway

/* static */
int
H_Image::openPipe(void)
{
    if (g_masterImages.size() == 0) {
        return 0;
    }

    //
    // base the image name and options off of the first Display call
    ImagePtr img = g_masterImages[0];
    
    if (img->getIMDisplay()) {
        return 1; // already open
    }
    
    std::string cmd;
    std::stringstream cmdss;
    int header[8];

    vector<ImagePtr>::iterator ip;
    int nChannels = 0;
    for (ip = g_masterImages.begin() ; ip != g_masterImages.end() ; ip++) {
        nChannels += (int)(*ip)->getChannelCount();
    }
    
    if (nChannels < 1) {
        return 0;
    }

    
    cmdss << "imdisplay -k -p -f -n \"" << img->getName() << "\"";
    
    // Port number is negative if this was opened with "iprsocket:<name>"
    if (img->getPort() > 0) {
        cmdss << " -s " << img->getPort();
    } else if (img->getPort() < 0) {
        cmdss << " -s " << -img->getPort();
    }

    cmd = cmdss.str();

    if (img->hasDisplayOptions()) {
        cmd += img->getDisplayOptions();
    }

    log(0, "popen: '%s'\n", cmd.c_str());
	
    IMDisplayPtr imp = IMDisplay::Singleton(cmd.c_str());
    if (!imp->IsValid()) {
    	log(0, "IMDisplayPtr is invalid !!!\n");
        return 0;
    }

    // hand a copy of the imp to each H_Image - when the last H_Image is deleted
    // that will cause the FILE* to be closed.
    // While we update time H_Images also adjust theier channel offsets. The
    // channel offset is the first id (an int) a H_Image's channels
    int totalChannels = 0;
    for (ip = g_masterImages.begin() ; ip != g_masterImages.end() ; ip++) {
        (*ip)->setIMDisplay(imp);
        (*ip)->setChannelOffset(totalChannels);
        totalChannels += (int)(*ip)->getChannelCount();
    }
    
    //
    // Now write the image header considering all H_Images and their channels
    //
    ::memset(header, 0, sizeof(header));
    header[0] = MAGIC;
    header[1] = img->getPort() < 0 ? img->getOrigXres() : img->getXres();
    header[2] = img->getPort() < 0 ? img->getOrigYres() : img->getYres();
    header[5] = totalChannels;
	
    FILE* fp = imp->GetFile();
    
    if (fwrite(header, sizeof(int), 8, fp) != 8) {
    	log(0, "Failed to write channel header !!!\n");
        return 0;
    }

    for (ip = g_masterImages.begin() ; ip != g_masterImages.end() ; ip++) {
        if ((*ip)->writeChannelHeader() != 1) {
            log(0, "Failed to write channel headers !!!\n");
            return 0;
        }
    }

    log(0, "openPipe done\n");
    return 1;
}

int
H_Image::writeChannelHeader()
{
    FILE* fp = myIMD->GetFile();

    int header[8];
    ::memset(header, 0, sizeof(header));

    for (int i = 0; i < myChannels.size(); ++i) {
        const H_Channel& chp = *myChannels[i];
        // Now, define each channel
        int namelen = strlen(chp.getName().c_str());
        header[0] = i;
        header[1] = namelen;
        header[2] = chp.getFormat();
        header[3] = chp.getArraySize();
        
        if (fwrite(header, sizeof(int), 8, fp) != 8)
            return 0;
        if (fwrite(chp.getName().c_str(), sizeof(char), namelen, fp) != namelen)
            return 0;
    }
    return 1; // succcess
}

int
H_Image::writeData(int a_x0, int a_x1, int a_y0, int a_y1,
                   const char* data_base, int bpp,
                   float tileScaleX, float tileScaleY) {
	int sx, sy;
	int xres, yres;
	int ch;

	// since we are going to move through the data based on per pixel information
	// we store a reference back to the original memory base address here....
	const char* data_curr = data_base;

	// scale into the current tile lod, assume tile scale of 1 to start with
	int x0 = a_x0;
	int x1 = a_x1;
	int y0 = a_y0;
	int y1 = a_y1;

	// for the special case of LOD we need to scale things up....
	if ((tileScaleX != 1) && (tileScaleY != 1)) {
		x0 = int(float(a_x0) * tileScaleX);
		x1 = int(float(a_x1) * tileScaleX);
		y0 = int(float(a_y0) * tileScaleY);
		y1 = int(float(a_y1) * tileScaleY);
	}

	// the source resolution
	int a_xres = a_x1 - a_x0;
	//int a_yres = a_y1 - a_y0;

	// the destination resolution
	xres = x1 - x0;
	yres = y1 - y0;

	// sanity check
	if (xres <= 0 || yres <= 0)
		return 1;

	//fprintf(stderr, "TILE: %d %d %d %d {%dx%d} -- %d %d\n", x0, x1, y0, y1, x1-x0, y1-y0, xres, yres);

	// begint the tile for all channels
	for (ch = 0; ch < myChannels.size(); ++ch)
		myChannels[ch]->startTile(xres, yres);

	// for each y pixel of the destination tile
	int destPixelOffset = 0;
	for (sy = 0; sy < yres; ++sy) {
		// for each x pixel of the destination tile
		for (sx = 0; sx < xres; ++sx) {
			// the destination pixel location
			destPixelOffset = sx + xres * sy;

			// the source pixel data location
			// to map to this, we use nearest neighbor interpolation to look into the source array
			int sourceX = int(float(sx) / tileScaleX);
			int sourceY = int(float(sy) / tileScaleY);

			data_curr = data_base + (sourceX + a_xres * sourceY) * bpp;

			// copy the data for each channel....
			for (ch = 0; ch < myChannels.size(); ++ch)
				myChannels[ch]->writePixel(data_curr, destPixelOffset);
		}
	}

    FILE* fp = myIMD->GetFile();
	for (ch = 0; ch < myChannels.size(); ++ch) {
        int chanId = ch + myChannelOffset;
		if (!myChannels[ch]->closeTile(fp, chanId, x0, x1, y0, y1))
			return 0;
	}

	fflush(fp);

	return 1;
}

void
H_Image::parseOptions(int paramCount, const UserParameter* parameters) {
	char options[4096] = {'\0'};  // initialized to empty string
	char tmp[4096];
	int int_buf[3];
	int original_size[2];
	char* str_ptr;
	int ret_count;
	PtDspyError err;

	ret_count = 3;
	err = DspyFindIntsInParamList("frange", &ret_count, int_buf, paramCount, parameters);
	if (err == PkDspyErrorNone && ret_count == 3) {
		// int_buf[0] = current frame
		// int_buf[1] = start frame
		// int_buf[2] = end frame
		if (int_buf[1] <= int_buf[2] && int_buf[0] >= int_buf[1] && int_buf[0] <= int_buf[2]) {
			sprintf(tmp, " -F %d %d %d", int_buf[0], int_buf[1], int_buf[2]);
			strcat(options, tmp);
		}
	}

	err = DspyFindIntInParamList("houdiniportnum", &int_buf[0], paramCount, parameters);
	if (err == PkDspyErrorNone) {
		sprintf(tmp, " -P %d", int_buf[0]);
		strcat(options, tmp);
	}

	err = DspyFindStringInParamList("sequence", &str_ptr, paramCount, parameters);
	if (err == PkDspyErrorNone) {
		strcat(options, " -S ");
		strcat(options, str_ptr);
	}

	err = DspyFindStringInParamList("remotedisplay", &str_ptr, paramCount, parameters);
	if (err == PkDspyErrorNone) {
		strcat(options, " -s ");
		strcat(options, str_ptr);
	}

	err = DspyFindStringInParamList("label", &str_ptr, paramCount, parameters);
	if (err == PkDspyErrorNone) {
		strcat(options, " -Y ");
		strcat(options, str_ptr);
	}

	err = DspyFindStringInParamList("numbering", &str_ptr, paramCount, parameters);
	if (err == PkDspyErrorNone) {
		strcat(options, " -N ");
		strcat(options, str_ptr);
	}

	ret_count = 2;
	err = DspyFindIntsInParamList("OriginalSize", &ret_count, original_size, paramCount, parameters);
	if (err == PkDspyErrorNone && ret_count == 2) {
		myOrigXres = original_size[0];
		myOrigYres = original_size[1];
		sprintf(tmp, " -Z %d %d", original_size[0], original_size[1]);
		strcat(options, tmp);
	}

	ret_count = 2;
	err = DspyFindIntsInParamList("origin", &ret_count, int_buf, paramCount, parameters);
	if (err == PkDspyErrorNone && ret_count == 2) {
		myXoff = int_buf[0];
		myYoff = int_buf[1];
		sprintf(tmp, " -o %d %d", int_buf[0], int_buf[1]);
		strcat(options, tmp);
	}

	myIMDisplayOptions.clear();

	if (*options != '\0')
		myIMDisplayOptions = ::strdup(options);
}

