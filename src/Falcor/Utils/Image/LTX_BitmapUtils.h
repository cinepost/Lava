#ifndef SRC_FALCOR_UTILS_IMAGE_LTX_BITMAP_UTILS_H_
#define SRC_FALCOR_UTILS_IMAGE_LTX_BITMAP_UTILS_H_

#include "stdafx.h"
#include "LTX_Bitmap.h"
#include "FreeImage.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Utils/StringUtils.h"

namespace Falcor {

/** Converts (in place) an RGB image data of an srcFormat to RGBA dstFormat image.
*/
void convertToRGBA(ResourceFormat srcFormat, ResourceFormat dstFormat, uint32_t width, uint32_t height, std::vector<unsigned char>& data);

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_IMAGE_LTX_BITMAP_UTILS_H_