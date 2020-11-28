#ifndef SRC_FALCOR_UTILS_IMAGE_BITMAP_UTILS_H_
#define SRC_FALCOR_UTILS_IMAGE_BITMAP_UTILS_H_

#include "stdafx.h"
#include "LTX_Bitmap.h"
#include "FreeImage.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Utils/StringUtils.h"

namespace Falcor {

bool isRGB32fSupported(std::shared_ptr<Device> pDevice);
bool isConvertibleToRGBA32Float(ResourceFormat format);


/** Converts half float image to RGBA float image.
*/
std::vector<float> convertHalfToRGBA32Float(uint32_t width, uint32_t height, uint32_t channelCount, const void* pData);


/** Converts integer image to RGBA float image.
    Unsigned integers are normalized to [0,1], signed integers to [-1,1].
*/
template<typename SrcT>
std::vector<float> convertIntToRGBA32Float(uint32_t width, uint32_t height, uint32_t channelCount, const void* pData);


/** Converts an image of the given format to an RGBA float image.
*/
std::vector<float> convertToRGBA32Float(ResourceFormat format, uint32_t width, uint32_t height, const void* pData);


/** Converts 96bpp to 128bpp RGBA without clamping.
    Note that we can't use FreeImage_ConvertToRGBAF() as it clamps to [0,1].
*/
FIBITMAP* convertToRGBAF(FIBITMAP* pDib);


FREE_IMAGE_FORMAT toFreeImageFormat(Bitmap::FileFormat fmt);
FREE_IMAGE_TYPE getImageType(uint32_t bytesPerPixel);

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_IMAGE_BITMAP_UTILS_H_