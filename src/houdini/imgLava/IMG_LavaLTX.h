#ifndef __IMG_SAMPLE__
#define __IMG_SAMPLE__

#include <IMG/IMG_File.h>
#include "Falcor/Utils/Image/LTX_Bitmap.h"

namespace HDK_Sample {

/// Custom image file format.  This class handles reading/writing the image.
/// @see IMG_SampleFormat

class IMG_LavaLTX : public IMG_File {
  public:
    IMG_LavaLTX();
    ~IMG_LavaLTX() override;

    int    open() override;
    int    openFile(const char *filename) override;

    int    readScanline(int y, void *buf) override;

    int    create(const IMG_Stat &stat) override;
    int    writeScanline(int scan, const void *buf) override;

    int    closeFile() override;

  private:
    int		 readHeader();
    int		 writeHeader();

    int		 myByteSwap;	// If reading on a different architecture

    Falcor::LTX_Bitmap::SharedConstPtr mpLtxBitmap;
};

}	// End of HDK_Sample namespace

#endif

