#include "LTX_BitmapUtils.h"

namespace Falcor {

/** Converts (in place) an RGB image data of an srcFormat to RGBA dstFormat image.
*/
void convertToRGBA(ResourceFormat srcFormat, ResourceFormat dstFormat, uint32_t width, uint32_t height, std::vector<unsigned char>& data) {
    uint8_t srcChannelCount = getFormatChannelCount(srcFormat);
    uint8_t dstChannelCount = getFormatChannelCount(dstFormat);

    assert((srcChannelCount == 3) && "Only 3 channel source image conversion supported !!!");
    assert((dstChannelCount == 4) && "Only 4 channel destination image conversion supported !!!");

    uint8_t srcChannelBitsCount = getNumChannelBits(srcFormat, 0);
    uint8_t dstChannelBitsCount = getNumChannelBits(dstFormat, 0);

    assert((srcChannelBitsCount == dstChannelBitsCount) && "Different bits per channel !!!");

    size_t srcDataSize = data.size();
    size_t dstDataSize = srcDataSize + srcDataSize / 3;
    size_t estimatedDstDataSize = width * height * dstChannelCount * dstChannelBitsCount / 8;

    if(data.size() == estimatedDstDataSize) {
        // target data vector already resized. probably by the previous run ...
        srcDataSize = width * height * srcChannelCount * srcChannelBitsCount / 8;
        dstDataSize = estimatedDstDataSize;
    } else {
        data.resize(dstDataSize);
    }

    uint8_t srcPixelBytesStride = srcChannelCount * srcChannelBitsCount / 8;
    uint8_t dstPixelBytesStride = dstChannelCount * dstChannelBitsCount / 8;

    size_t numPixels = srcDataSize / srcPixelBytesStride;

    unsigned char* lastSrcPixelAddr = data.data() + srcDataSize - srcPixelBytesStride;
    unsigned char* lastDstPixelAddr = data.data() + dstDataSize - dstPixelBytesStride;
    
    for(size_t i = 0; i < numPixels; i++) {
        memcpy(lastDstPixelAddr, lastSrcPixelAddr, srcPixelBytesStride);
        lastSrcPixelAddr -= srcPixelBytesStride;
        lastDstPixelAddr -= dstPixelBytesStride;
    }

}
    

}  // namespace Falcor
