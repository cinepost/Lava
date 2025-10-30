/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "stdafx.h"
#include "ImageIO.h"


#include "Falcor/Core/Framework.h"

#include "NvidiaTextureTools/linux/include/nvtt/nvtt.h"


namespace Falcor {

namespace {
    
struct ImportData {
    // Commonly used values converted or casted for cleaner access
    ResourceFormat format;
    Resource::Type type;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t arraySize;
    uint32_t mipLevels;
    bool hasDX10Header = false;

    // Data to be imported
    std::vector<uint8_t> imageData;
};

struct ExportData {
    // Commonly used values converted or casted for cleaner access
    nvtt::TextureType type;
    ResourceFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t faceCount;
    uint32_t mipLevels;

    // Data to be exported
    std::vector<nvtt::Surface> images;
};

#ifdef _NVTT 

ImageIO::CompressionMode convertFormatToMode(ResourceFormat format) {
    switch (format) {
        case ResourceFormat::BC1Unorm:
        case ResourceFormat::BC1UnormSrgb:
            return ImageIO::CompressionMode::BC1;
        case ResourceFormat::BC2Unorm:
        case ResourceFormat::BC2UnormSrgb:
            return ImageIO::CompressionMode::BC2;
        case ResourceFormat::BC3Unorm:
        case ResourceFormat::BC3UnormSrgb:
            return ImageIO::CompressionMode::BC3;
        case ResourceFormat::BC4Unorm:
            return ImageIO::CompressionMode::BC4;
        case ResourceFormat::BC5Snorm:
        case ResourceFormat::BC5Unorm:
            return ImageIO::CompressionMode::BC5;
        case ResourceFormat::BC6HS16:
            return ImageIO::CompressionMode::BC6;
        case ResourceFormat::BC7Unorm:
        case ResourceFormat::BC7UnormSrgb:
            return ImageIO::CompressionMode::BC7;
        default:
            throw std::runtime_error("No corresponding compression mode for the provided ResourceFormat.");
    }
}

// Returns the corresponding NVTT compression format for the provided compression mode.
nvtt::Format convertModeToNvttFormat(ImageIO::CompressionMode mode) {
    switch (mode) {
        case ImageIO::CompressionMode::None:
            return nvtt::Format::Format_RGBA;
        case ImageIO::CompressionMode::BC1:
            return nvtt::Format::Format_BC1;
        case ImageIO::CompressionMode::BC2:
            return nvtt::Format::Format_BC2;
        case ImageIO::CompressionMode::BC3:
            return nvtt::Format::Format_BC3;
        case ImageIO::CompressionMode::BC4:
            return nvtt::Format::Format_BC4;
        case ImageIO::CompressionMode::BC5:
            return nvtt::Format::Format_BC5;
        case ImageIO::CompressionMode::BC6:
            return nvtt::Format::Format_BC6S;
        case ImageIO::CompressionMode::BC7:
            return nvtt::Format::Format_BC7;
        default:
            throw std::runtime_error("Invalid compression mode.");
    }
}

// Returns the corresponding NVTT compression format for the provided ResourceFormat. This conversion function should be used to convert formats for compressed textures.
nvtt::Format convertFormatToNvttFormat(ResourceFormat format) {
    switch (format)
    {
    case ResourceFormat::BC1Unorm:
    case ResourceFormat::BC1UnormSrgb:
        return nvtt::Format::Format_BC1;
    case ResourceFormat::BC2Unorm:
    case ResourceFormat::BC2UnormSrgb:
        return nvtt::Format::Format_BC2;
    case ResourceFormat::BC3Unorm:
    case ResourceFormat::BC3UnormSrgb:
        return nvtt::Format::Format_BC3;
    case ResourceFormat::BC4Unorm:
        return nvtt::Format::Format_BC4;
    case ResourceFormat::BC5Snorm:
    case ResourceFormat::BC5Unorm:
        return nvtt::Format::Format_BC5;
    case ResourceFormat::BC6HS16:
        return nvtt::Format::Format_BC6S;
    case ResourceFormat::BC7Unorm:
    case ResourceFormat::BC7UnormSrgb:
        return nvtt::Format::Format_BC7;
    default:
        throw std::runtime_error("No corresponding NVTT compression format for the specified ResourceFormat.");
    }
}

// Returns the corresponding NVTT input format for the provided ResourceFormat. Should only be used to convert formats for non-compressed textures.
nvtt::InputFormat convertToNvttInputFormat(ResourceFormat format) {
    // Special case for R32FloatX32 as this is otherwise indistinguishable from RG32Float
    if (format == ResourceFormat::R32FloatX32) {
        throw std::runtime_error("Image is in an unsupported ResourceFormat.");
    }

    uint32_t channelCount = getFormatChannelCount(format);
    uint32_t xBits = getNumChannelBits(format, 0);
    uint32_t yBits = getNumChannelBits(format, 1);
    uint32_t zBits = getNumChannelBits(format, 2);
    uint32_t wBits = getNumChannelBits(format, 3);

    bool isR32Float = channelCount == 1 && xBits == 32;
    bool isSupportedTwoChannel = channelCount == 2 && xBits == yBits; // all RG formats
    bool isSupportedThreeChannel = channelCount == 3 && xBits == yBits && yBits == zBits; // all RGB formats
    bool isSupportedFourChannel = xBits == yBits && yBits == zBits && zBits == wBits;

    // These are fairly broadly sorted into the five NVTT input formats. Most resource formats will require
    // modifications to the data before being passed to NVTT for exporting; this is done later on in setImage().
    if (isR32Float || isSupportedTwoChannel || isSupportedThreeChannel || isSupportedFourChannel) {
        if (isSupportedThreeChannel) {
            LLOG_WRN << "NVTT is incompatible with three channel images. This image will be padded with a solid alpha channel.";
        }

        if (isR32Float) {
            return nvtt::InputFormat::InputFormat_R_32F;
        }

        if (xBits == 8) {
            if (getFormatType(format) == FormatType::Uint || getFormatType(format) == FormatType::Unorm) {
                return nvtt::InputFormat::InputFormat_BGRA_8UB;
            }
            else return nvtt::InputFormat::InputFormat_BGRA_8SB;
        }
        else if (xBits == 16) return nvtt::InputFormat::InputFormat_RGBA_16F;
        else if (xBits == 32) return nvtt::InputFormat::InputFormat_RGBA_32F;
    }

    throw std::runtime_error("Image is in an unsupported ResourceFormat.");
}

// Check if any of base image dimensions need to be clamped to a multiple of 4.
// This function should only be called if the image is being compressed and mipmaps are being automatically generated.
bool clampIfNeeded(ExportData& image) {
    bool clamped = false;
    if (image.width > 1u && image.width % 4 != 0) {
        image.width = std::max(1u, image.width - image.width % 4);
        clamped = true;
    }
    if (image.height > 1u && image.height % 4 != 0) {
        image.height = std::max(1u, image.height - image.height % 4);
        clamped = true;
    }
    if (image.depth > 1u && image.depth % 4 != 0) {
        image.depth = std::max(1u, image.depth - image.depth % 4);
        clamped = true;
    }

    return clamped;
}

// Fill the alpha channel with 1's.
void fillAlphaChannel(nvtt::Surface& image) {
    // Create a dummy Surface and fill with 1's then copy the alpha channel. DirectXTex fills the alpha channel
    // with 0's for images that do not have an alpha, but NVTT does not have an equivalent alpha-less InputFormat.
    // The alpha channel must thus be manually filled with 1's otherwise the resulting image may not display
    // properly. BGRX8 is a unique case that it is a four channel format with no alpha.
    nvtt::Surface alpha(image);
    alpha.fill(1.0, 1.0, 1.0, 1.0);
    image.copyChannel(alpha, 3, 3);
}

// Prepare the original image data for being passed to NVTT for exporting. Certain image formats will also need
// the data to be modified to include empty blue and/or solid alpha channels. This is because NVTT only supports
// five specific input formats: 8-bit unsigned BGRA, 8-bit signed BGRA, 16-bit floating point RGBA,
// 32-bit floating point RGBA, and single channel 32-bit floating point.
//
// NVTT's Surface always holds a single image's worth of UNCOMPRESSED data. Re-compression is necessary
// if image compression needs to be maintained.
template <typename T>
void setImage(const void* subresourceData, nvtt::Surface& surface, ExportData image, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcDepth) {
    std::vector<T> modified;
    uint32_t pixelCount = srcWidth * srcHeight * srcDepth;
    uint32_t channelCount = getFormatChannelCount(image.format);
    T alpha = 0;

    // Need to flip red and blue channels for all 8 bit formats that aren't BGRA/BGRX as NVTT only supports BGRA inputs for these cases
    bool reverseRB = getNumChannelBits(image.format, 0) == 8 && image.format != ResourceFormat::BGRA8Unorm && image.format != ResourceFormat::BGRA8UnormSrgb
        && image.format != ResourceFormat::BGRX8Unorm && image.format != ResourceFormat::BGRX8UnormSrgb;
    // Need to fill the alpha channel with 1's for all formats that do not have an alpha channel
    bool fillAlpha = channelCount == 2 || channelCount == 3 || image.format == ResourceFormat::BGRX8Unorm || image.format == ResourceFormat::BGRX8UnormSrgb;

    modified.resize(4 * pixelCount);

    T* src = (T*)subresourceData;
    T* dst = (T*)modified.data();
    for (uint32_t h = 0; h < image.height; ++h) {
        for (uint32_t w = 0; w < image.width; ++w) {
            uint32_t i = h * srcWidth + w; // Source data index
            uint32_t j = h * image.width + w; // Destination data index - Same as source index if no clamping is involved
            if (channelCount == 1) {
                dst[j] = src[i];
            }
            else if (channelCount == 2)
            {
                dst[4 * j] = reverseRB ? 0 : src[2 * i];
                dst[4 * j + 1] = src[2 * i + 1];
                dst[4 * j + 2] = reverseRB ? src[2 * i] : 0;
                dst[4 * j + 3] = 0;
            }
            else if (channelCount == 3)
            {
                dst[4 * j] = reverseRB ? src[3 * i + 2] : src[3 * i];
                dst[4 * j + 1] = src[3 * i + 1];
                dst[4 * j + 2] = reverseRB ? src[3 * i] : src[3 * i + 2];
                dst[4 * j + 3] = 0;
            }
            else if (channelCount == 4)
            {
                dst[4 * j] = reverseRB ? src[4 * i + 2] : src[4 * i];
                dst[4 * j + 1] = src[4 * i + 1];
                dst[4 * j + 2] = reverseRB ? src[4 * i] : src[4 * i + 2];
                dst[4 * j + 3] = src[4 * i + 3];
            }
        }
    }

    if (isCompressedFormat(image.format)) {
        nvtt::Format compressionFormat = convertFormatToNvttFormat(image.format);
        if (!surface.setImage3D(compressionFormat, (int)image.width, (int)image.height, (int)image.depth, modified.data())) {
            throw std::runtime_error("Failed to set image data.");
        }
    } else {
        nvtt::InputFormat inputFormat = convertToNvttInputFormat(image.format);
        if (!surface.setImage(inputFormat, (int)image.width, (int)image.height, (int)image.depth, modified.data()))
        {
            throw std::runtime_error("Failed to set image data.");
        }
    }

    if (fillAlpha) fillAlphaChannel(surface);
}

#endif  // _NVTT

}  // namespace



} // namespace Falcor
