#include "BitmapUtils.h"

namespace Falcor {

#ifdef FALCOR_VK

bool isRGB32fSupported(std::shared_ptr<Device> pDevice) {
    VkFormatProperties p;
    vkGetPhysicalDeviceFormatProperties(pDevice->getApiHandle(), VK_FORMAT_R32G32B32_SFLOAT, &p);
    return p.optimalTilingFeatures != 0;
}

#else

bool isRGB32fSupported(std::shared_ptr<Device> pDevice) { return false; } // FIX THIS

#endif


bool isConvertibleToRGBA32Float(ResourceFormat format) {
    FormatType type = getFormatType(format);
    bool isRGBFloatFormat = (type == FormatType::Float && getNumChannelBits(format, 0) == 32);
    bool isHalfFormat = (type == FormatType::Float && getNumChannelBits(format, 0) == 16);
    bool isLargeIntFormat = ((type == FormatType::Uint || type == FormatType::Sint) && getNumChannelBits(format, 0) >= 16);
    return isHalfFormat || isLargeIntFormat || isRGBFloatFormat;
}

/** Converts half float image to RGBA float image.
*/
std::vector<float> convertHalfToRGBA32Float(uint32_t width, uint32_t height, uint32_t channelCount, const void* pData) {
    std::vector<float> newData(width * height * 4u, 0.f);
    const glm::detail::hdata* pSrc = reinterpret_cast<const glm::detail::hdata*>(pData);
    float* pDst = newData.data();

    for (uint32_t i = 0; i < width * height; ++i) {
        for (uint32_t c = 0; c < channelCount; ++c) {
            *pDst++ = glm::detail::toFloat32(*pSrc++);
        }
        pDst += (4 - channelCount);
    }

    return newData;
}

/** Converts RGB float image to RGBA float image.
*/
std::vector<float> convertRGB32FloatToRGBA32Float(uint32_t width, uint32_t height, uint32_t channelCount, const void* pData) {
    std::vector<float> newData(width * height * 4u, 1.f);
    const float* pSrc = static_cast<const float*>(pData);
    float* pDst = newData.data();

    for (uint32_t i = 0; i < width * height; ++i) {
        for (uint32_t c = 0; c < channelCount; ++c) {
            *pDst++ = *pSrc++;
        }
        pDst += (4 - channelCount);
    }

    return newData;
}

/** Converts integer image to RGBA float image.
    Unsigned integers are normalized to [0,1], signed integers to [-1,1].
*/
template<typename SrcT>
std::vector<float> convertIntToRGBA32Float(uint32_t width, uint32_t height, uint32_t channelCount, const void* pData) {
    std::vector<float> newData(width * height * 4u, 0.f);
    const SrcT* pSrc = reinterpret_cast<const SrcT*>(pData);
    float* pDst = newData.data();

    for (uint32_t i = 0; i < width * height; ++i) {
        for (uint32_t c = 0; c < channelCount; ++c) {
            *pDst++ = float(*pSrc++) / float(std::numeric_limits<SrcT>::max());
        }
        pDst += (4 - channelCount);
    }

    return newData;
}

/** Converts an image of the given format to an RGBA float image.
*/
std::vector<float> convertToRGBA32Float(ResourceFormat format, uint32_t width, uint32_t height, const void* pData) {
    assert(isConvertibleToRGBA32Float(format));

    FormatType type = getFormatType(format);
    uint32_t channelCount = getFormatChannelCount(format);
    uint32_t channelBits = getNumChannelBits(format, 0);

    std::vector<float> floatData;

    if (type == FormatType::Float && channelBits == 16)
    {
        floatData = convertHalfToRGBA32Float(width, height, channelCount, pData);
    }
    else if (type == FormatType::Float && channelBits == 32 && channelCount == 3)
    {
        floatData = convertRGB32FloatToRGBA32Float(width, height, channelCount, pData);
        return floatData;
    }
    else if (type == FormatType::Uint && channelBits == 16)
    {
        floatData = convertIntToRGBA32Float<uint16_t>(width, height, channelCount, pData);
    }
    else if (type == FormatType::Uint && channelBits == 32)
    {
        floatData = convertIntToRGBA32Float<uint32_t>(width, height, channelCount, pData);
    }
    else if (type == FormatType::Sint && channelBits == 16)
    {
        floatData = convertIntToRGBA32Float<int16_t>(width, height, channelCount, pData);
    }
    else if (type == FormatType::Sint && channelBits == 32)
    {
        floatData = convertIntToRGBA32Float<int32_t>(width, height, channelCount, pData);
    } else {
        should_not_get_here();
    }

    // Default alpha channel to 1.
    if (channelCount < 4) {
        for (uint32_t i = 0; i < width * height; ++i) floatData[i * 4 + 3] = 1.f;
    }

    return floatData;
}

/** Converts 96bpp to 128bpp RGBA without clamping.
    Note that we can't use FreeImage_ConvertToRGBAF() as it clamps to [0,1].
*/
FIBITMAP* convertToRGBAF(FIBITMAP* pDib) {
    const unsigned width = FreeImage_GetWidth(pDib);
    const unsigned height = FreeImage_GetHeight(pDib);

    auto pNew = FreeImage_AllocateT(FIT_RGBAF, width, height);
    FreeImage_CloneMetadata(pNew, pDib);

    const unsigned src_pitch = FreeImage_GetPitch(pDib);
    const unsigned dst_pitch = FreeImage_GetPitch(pNew);

    const BYTE *src_bits = (BYTE*)FreeImage_GetBits(pDib);
    BYTE* dst_bits = (BYTE*)FreeImage_GetBits(pNew);

    for (unsigned y = 0; y < height; y++)
    {
        const FIRGBF *src_pixel = (FIRGBF*)src_bits;
        FIRGBAF* dst_pixel = (FIRGBAF*)dst_bits;

        for (unsigned x = 0; x < width; x++)
        {
            // Convert pixels directly, while adding a "dummy" alpha of 1.0
            dst_pixel[x].red = src_pixel[x].red;
            dst_pixel[x].green = src_pixel[x].green;
            dst_pixel[x].blue = src_pixel[x].blue;
            dst_pixel[x].alpha = 1.0F;

        }
        src_bits += src_pitch;
        dst_bits += dst_pitch;
    }
    return pNew;
}



FREE_IMAGE_FORMAT toFreeImageFormat(Bitmap::FileFormat fmt) {
    switch(fmt) {
        case Bitmap::FileFormat::PngFile:
            return FIF_PNG;
        case Bitmap::FileFormat::JpegFile:
            return FIF_JPEG;
        case Bitmap::FileFormat::TgaFile:
            return FIF_TARGA;
        case Bitmap::FileFormat::BmpFile:
            return FIF_BMP;
        case Bitmap::FileFormat::PfmFile:
            return FIF_PFM;
        case Bitmap::FileFormat::ExrFile:
            return FIF_EXR;
        default:
            should_not_get_here();
    }
    return FIF_PNG;
}

FREE_IMAGE_TYPE getImageType(uint32_t bytesPerPixel) {
    switch(bytesPerPixel) {
        case 4:
            return FIT_BITMAP;
        case 12:
            return FIT_RGBF;
        case 16:
            return FIT_RGBAF;
        default:
            should_not_get_here();
    }
    return FIT_BITMAP;
}

}  // namespace Falcor
