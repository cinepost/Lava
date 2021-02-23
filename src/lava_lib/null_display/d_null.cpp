#include <limits>
#include <memory>
#include <chrono>
#include <array>
#include <cstring>

#include "prman/ndspy.h"

#ifdef __cplusplus
extern "C" {
#endif

static std::chrono::time_point<std::chrono::system_clock> g_start, g_end;

static int g_channels;
static PtDspyUnsigned32 g_width;
static PtDspyUnsigned32 g_height;


PtDspyError processEvents();

PtDspyError DspyImageOpen(PtDspyImageHandle *image_h,
  const char *,const char *filename,
  int width,int height,
  int ,const UserParameter *,
  int formatCount,PtDspyDevFormat *format, PtFlagStuff *flagstuff)
{
  PtDspyError ret;
  
  // MyImageType image;
  
  flagstuff->flags &= ~PkDspyFlagsWantsScanLineOrder;
  flagstuff->flags |= PkDspyBucketOrderSpaceFill;
  flagstuff->flags |= PkDspyFlagsWantsNullEmptyBuckets;
  
  // stupidity checking
  if (0 == width) g_width = 640;
  if (0 == height) g_height = 480;

  ret = PkDspyErrorNone;

  g_width = width;
  g_height = height;
  g_channels = formatCount;

  // shuffle format so we always write out RGB[A]
  std::array<std::string,4> chan = { {"r", "g", "b", "a"} };
  for ( int i=0; i<formatCount; i++ )
    for( int j=0; j<4; ++j )
      if( std::string(format[i].name)==chan[size_t(j)] && i!=j )
        std::swap(format[j],format[i]);

  g_start = std::chrono::system_clock::now();

  *image_h = (void *)new float(1);

  return ret;
}

PtDspyError DspyImageQuery(PtDspyImageHandle,PtDspyQueryType querytype, int datalen,void *data) {
  PtDspyError ret;

  ret = PkDspyErrorNone;

  if (datalen > 0 &&  data!=nullptr) {
    switch (querytype) {
      case PkOverwriteQuery: {
        PtDspyOverwriteInfo overwriteInfo;
        size_t size = sizeof(overwriteInfo);
        overwriteInfo.overwrite = 1;
        overwriteInfo.interactive = 0;
        memcpy(data, &overwriteInfo, size);
        break; }

      case PkSizeQuery : {
        PtDspySizeInfo overwriteInfo;
        size_t size = sizeof(overwriteInfo);
        overwriteInfo.width = g_width;
        overwriteInfo.height = g_height;
        overwriteInfo.aspectRatio=1.0f;
        memcpy(data, &overwriteInfo, size);
        break; }

      case PkRedrawQuery : {
        PtDspyRedrawInfo overwriteInfo;

        size_t size = sizeof(overwriteInfo);
        overwriteInfo.redraw = 0;
        memcpy(data, &overwriteInfo, size);
        break; }

      case PkRenderingStartQuery :
        std::cerr<<"Start rendering\n";
        break;

      case PkSupportsCheckpointing:
      case PkNextDataQuery:
      case PkGridQuery:
      case PkMultiResolutionQuery:
      case PkQuantizationQuery :
      case PkMemoryUsageQuery :
      case PkElapsedTimeQuery :
      case PkPointCloudQuery :
        ret = PkDspyErrorUnsupported;
        break;
    }
  }
  else
  {
   ret = PkDspyErrorBadParams;
  }
  return ret;
}

PtDspyError DspyImageData(PtDspyImageHandle ,int xmin,int xmax,int ymin,int ymax,int entrysize,const unsigned char *data) {
  return PkDspyErrorNone;
}

PtDspyError DspyImageClose(PtDspyImageHandle image_h) {
  g_end = std::chrono::system_clock::now();

  std::chrono::duration<double> elapsed_seconds = g_end-g_start;
  std::time_t end_time = std::chrono::system_clock::to_time_t(g_end);

  std::cout << "finished rendering at " << std::ctime(&end_time) << "elapsed time: " << elapsed_seconds.count() << "s\n";

  delete image_h;

  return PkDspyErrorNone;
}


#ifdef __cplusplus
}
#endif