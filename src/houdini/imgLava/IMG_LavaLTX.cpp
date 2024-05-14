#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <iostream>
#include <fstream>  
#include <string>

#include <UT/UT_Endian.h>		// For byte swapping
#include <UT/UT_DSOVersion.h>
#include <UT/UT_SysClone.h>
#include <IMG/IMG_Format.h>
#include "IMG_LavaLTX.h"

#define MAGIC		0x1234567a
#define MAGIC_SWAP	0xa7654321		// Swapped magic number

static const size_t LTX_MAX_RES = 32767;

namespace HDK_Sample {
typedef struct {
	unsigned int	magic;		// Magic number
	unsigned int	xres;		// Width of image
	unsigned int	yres;		// Height of image
	unsigned int	model;		// My color model
	unsigned int	data;		// Data type
} IMG_SampleHeader;

/// Custom image file format definition.  This class defines the properties of
/// the custom image format.
class IMG_LavaLTXFormat : public IMG_Format {
	public:
		IMG_LavaLTXFormat() {}
		~IMG_LavaLTXFormat() override {}

		const char          *getFormatName() const override;
		const char          *getFormatLabel() const override;
		const char          *getFormatDescription() const override;
		const char          *getDefaultExtension() const override;
		IMG_File            *createFile()    const override;

		// Methods to determine if this is one of our recognized files.
		// The extension is the first try.  If there are multiple matches,
		// then we resort to the magic number (when reading)
		int                  checkExtension(const char *filename) const override;
		int                  checkMagic(unsigned int) const override;
		IMG_DataType         getSupportedTypes() const override { return IMG_DT_ALL; }
		IMG_ColorModel       getSupportedColorModels() const override{ return IMG_CM_ALL; }

		// Configuration information for the format
		void                 getMaxResolution(unsigned &x, unsigned &y) const override;

		int                  isReadRandomAccess() const override  { return 0; }
		int                  isWriteRandomAccess() const override { return 1; }
		bool                 isReadable() const override { return true; }
		bool                 isWritable() const override { return false; }
};

}	// End HDK_Sample namespace

using namespace HDK_Sample;

const char* IMG_LavaLTXFormat::getFormatName() const {
	// Very brief label (no spaces)
	return "LavaLTX";
}

const char* IMG_LavaLTXFormat::getFormatLabel() const {
	// A simple description of the format
	return "Lava LTX texture image format.";
}

const char* IMG_LavaLTXFormat::getFormatDescription() const {
	// A more verbose description of the image format.  Things you might put in
	// here are the version of the format, etc.
	return "Lava LTX texture image format.";
}

const char* IMG_LavaLTXFormat::getDefaultExtension() const {
	// The default extension for the format files.  If there is no default
	// extension, the format won't appear in the menus to choose image format
	// types.
	return "ltx";
}

IMG_File* IMG_LavaLTXFormat::createFile() const {
	return new IMG_LavaLTX;
}

int IMG_LavaLTXFormat::checkExtension(const char *filename) const {
	static const char	*extensions[] = { ".ltx", ".LTX", 0 };
	return matchExtensions(filename, extensions);
}

int IMG_LavaLTXFormat::checkMagic(unsigned int magic) const {
	// Check if we hit our magic number
	return (magic == MAGIC || magic == MAGIC_SWAP);
}

void IMG_LavaLTXFormat::getMaxResolution(unsigned &x, unsigned &y) const {
	x = static_cast<unsigned>(LTX_MAX_RES);		// Stored as shorts
	y = static_cast<unsigned>(LTX_MAX_RES);
}


//////////////////////////////////////////////////////////////////
//
//  Sample file loader/saver
//
//////////////////////////////////////////////////////////////////

IMG_LavaLTX::IMG_LavaLTX() {
	myByteSwap = 0;
	mpLtxBitmap = nullptr;
}

IMG_LavaLTX::~IMG_LavaLTX() {
	close();
}

int IMG_LavaLTX::open() {
	return readHeader();
}

int IMG_LavaLTX::openFile(const char *filename) {
	mpLtxBitmap = Falcor::LTX_Bitmap::createFromFile(nullptr, std::string(filename));
	return readHeader();
}

/// Default texture options passed down by mantra.
/// See also the vm_saveoption SOHO setting.
static const char	*theTextureOptions[] = {
	"camera:orthowidth",	// Orthographic camera width
	"camera:zoom",		// Perspective camera zoom
	"camera:projection",	// 0 = perspective, 1 = orthographic, etc.
	"image:crop",		// Crop window (xmin, xmax, ymin, ymax)
	"image:window",		// Screen window (x0, x1, y0, y1)
	"image:pixelaspect",	// Pixel aspect ratio (not frame aspect)
	"image:samples",		// Sampling information
	"space:world",		// World space transform of camera
	NULL
};

static void writeTextureOption(const char *token, const char *value) {
	//cout << "Sample: " << token << " := " << value << endl;
}

int IMG_LavaLTX::create(const IMG_Stat &stat) {
	// Store the image stats and write out the header.
	myStat = stat;
	if (!writeHeader())
	return 0;

	// When mantra renders to this format, options set in the vm_saveoption
	// string will be passed down to the image format.  This allows you to
	// query information about the renderer settings.  This is optional of
	// course.
	for (int i = 0; theTextureOptions[i]; ++i) {
		const char	*value;
		value = getOption(theTextureOptions[i]);
		if (value) writeTextureOption(theTextureOptions[i], value);
	}
	return true;
}

int IMG_LavaLTX::closeFile() {
	// If we're writing data, flush out the stream
	if (myOS) myOS->flush();	// Flush out the data

	return 1;	// return success
}

static inline void swapHeader(IMG_SampleHeader &header) {
	UTswapBytes((int *)&header, sizeof(header));
}

int IMG_LavaLTX::readHeader() {
	if(!mpLtxBitmap) return 0;

	const auto& header = mpLtxBitmap->header();

	IMG_Plane		*plane;

	myStat.setResolution(header.width, header.height);
	myStat.setNumFrames(1);

	plane = myStat.addDefaultPlane();
	if(!plane) return 0;

	plane->setColorModel(IMG_RGBA);
	plane->setDataType(IMG_UCHAR);

	// Now, we're ready to read the data.
	return 1;
}

int IMG_LavaLTX::writeHeader() {

	// Now, we're ready to write the scanlines...
	return 1;
}

int IMG_LavaLTX::readScanline(int y, void *buf) {
	int	nbytes;

	//test
	for(size_t i = 0; i < 128; i++) *((char*)buf + i) = 0;

	if (y >= myStat.getYres()) return 0;

	nbytes = myStat.bytesPerScanline();
	if (!readBytes((char *)buf, nbytes))
	return 0;

	// If the file was written on a different architecture, we might need to
	// swap the data.
	if (myByteSwap) {
	switch (myStat.getPlane()->getDataType()) {
		case IMG_UCHAR:	break;		// Nope
		case IMG_FLOAT16:
		case IMG_USHORT:
		UTswapBytes((short *)buf, nbytes/sizeof(short));
		break;
		case IMG_UINT:
		UTswapBytes((int *)buf, nbytes/sizeof(int));
		break;
		case IMG_FLOAT:
		UTswapBytes((float *)buf, nbytes/sizeof(float));
		break;
		default:
		break;
	}
	}

	return 1;
}

int IMG_LavaLTX::writeScanline(int /*y*/, const void *buf) {
	// If we specified a translator in creation, the buf passed in will be in
	// the format we want, that is, the translator will make sure the data is
	// in the correct format.

	// Since we always write in native format, we don't have to swap
	return (!myOS->write((char *)buf, myStat.bytesPerScanline())) ? 0 : 1;
}

////////////////////////////////////////////////////////////////////
//
//  Now, we load the format
//
////////////////////////////////////////////////////////////////////
void newIMGFormat(void *) {
	new IMG_LavaLTXFormat();
}
