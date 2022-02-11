#ifndef SRC_FALCOR_UTILS_IMAGE_LTX_BITMAPALGO_H_
#define SRC_FALCOR_UTILS_IMAGE_LTX_BITMAPALGO_H_

#include <stdio.h>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "Falcor/Core/Framework.h"

#include "LTX_Bitmap.h"

namespace Falcor {

namespace oiio = OIIO;


struct TLCInfo {
	LTX_Header::TopLevelCompression topLevelCompression = LTX_Header::TopLevelCompression::NONE;
	int compressionLevel = 1;
	uint32_t *pPageOffsets = nullptr;
	uint16_t *pCompressedPageSizes = nullptr;
};

/* Slow highest possible quility algorithm suitable for textures of any dimensions
 */
bool ltxCpuGenerateAndWriteMIPTilesHQSlow(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile, TLCInfo& compressionInfo);

/* Faster highest possible quility algorithm with a higher memory footprint suitable for textures of any dimensions
 */
bool ltxCpuGenerateAndWriteMIPTilesHQFast(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile, TLCInfo& compressionInfo);

/* Fastest low quility algorithm suitable for textures of any dimensions
 */
bool ltxCpuGenerateAndWriteMIPTilesLQ(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile, TLCInfo& compressionInfo);

/* Fast algorithm for textures with "power of two" side dimesions 
 */
bool ltxCpuGenerateAndWriteMIPTilesPOT(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile, TLCInfo& compressionInfo);

/* Debug ltx tiles generation
 */
bool ltxCpuGenerateDebugMIPTiles(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile, TLCInfo& compressionInfo);

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_IMAGE_LTX_BITMAPALGO_H_
