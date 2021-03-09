#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Formats.h"
#include "LTX_BitmapAlgo.h"

namespace Falcor {

bool ltxCpuGenerateAndWriteMIPTilesHQSlow(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile) {
    assert(pFile);

    // image dimesions
    uint32_t img_width  = header.width;
    uint32_t img_height = header.height;
    uint32_t img_depth  = header.depth;

    // image memory page dimensions
    uint32_t page_width  = header.pageDims.width;
    uint32_t page_height = header.pageDims.height;
    uint32_t page_depth  = header.pageDims.depth;

    // source image buffer spec and pixel format
    auto const& spec = srcBuff.spec();
    auto format = header.format;

    uint32_t pagesCount = 0;
    uint32_t dstChannelCount = getFormatChannelCount(format);
    uint32_t dstChannelBits = getNumChannelBits(format, 0);

    size_t srcBytesPerPixel = spec.pixel_bytes();
    size_t dstBytesPerPixel = dstChannelCount * dstChannelBits / 8;

    size_t tileWidthStride = page_width * dstBytesPerPixel;
    size_t bufferWidthStride = img_width * dstBytesPerPixel;

    uint32_t pagesCountX = img_width / page_width;
    uint32_t pagesCountY = img_height / page_height;
    uint32_t pagesCountZ = img_depth / page_depth;

    uint3 partialPageDims = {
        (img_width % page_width == 0) ? 0 : img_width - page_width * pagesCountX,
        (img_height % page_height == 0) ? 0 : img_height - page_height * pagesCountY,
        (img_depth % page_depth == 0) ? 0 : img_depth - page_depth * pagesCountZ
    };

    size_t partialTileWidthStride = partialPageDims.x * dstBytesPerPixel;

    if( partialPageDims.x != 0) pagesCountX++;
    if( partialPageDims.y != 0) pagesCountY++;
    if( partialPageDims.z != 0) pagesCountZ++;

    std::vector<unsigned char> zero_buff(65536, 0);
    std::vector<unsigned char> tiles_buffer(img_width * page_height * dstBytesPerPixel);

    // write mip level 0
    LOG_WARN("Writing mip level 0 tiles %u %u ...", pagesCountX, pagesCountY);
    LOG_WARN("Partial page dims: %u %u %u", partialPageDims.x, partialPageDims.y, partialPageDims.z);

    pagesCount += pagesCountX * pagesCountY * pagesCountZ;
    for(uint32_t z = 0; z < pagesCountZ; z++) {
        for(uint32_t tileIdxY = 0; tileIdxY < pagesCountY; tileIdxY++) {
            int y_begin = tileIdxY * page_height;

            bool partialRow = false;
            uint writeLinesCount = page_height;
            if ( tileIdxY == (pagesCountY-1) && partialPageDims.y != 0) {
                writeLinesCount = partialPageDims.y;
                partialRow = true;
            }

            oiio::ROI roi(0, img_width, y_begin, y_begin + writeLinesCount, z, z + 1, /*chans:*/ 0, dstChannelCount);
            srcBuff.get_pixels(roi, spec.format, tiles_buffer.data(), oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);

            unsigned char *pBufferData = tiles_buffer.data();

            for(uint32_t tileIdxX = 0; tileIdxX < pagesCountX; tileIdxX++) {

                bool partialColumn = false;
                if(tileIdxX == (pagesCountX-1) && partialPageDims.x != 0) partialColumn = true;
                
                unsigned char *pTileData = pBufferData + tileIdxX * tileWidthStride;

                if( partialRow && partialColumn) {
                    // write partial corner tile
                    for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
                        fwrite(pTileData, sizeof(uint8_t), partialTileWidthStride, pFile);
                        pTileData += bufferWidthStride;
                    }
                    // write zero padding
                    fwrite(zero_buff.data(), sizeof(uint8_t), (tileWidthStride - partialTileWidthStride) * writeLinesCount, pFile);
                    fwrite(zero_buff.data(), sizeof(uint8_t), tileWidthStride * (page_height - writeLinesCount), pFile);
                } else if( partialRow ) {
                    // write partial bottom tile
                    for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
                        fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                        pTileData += bufferWidthStride;
                    }
                    // write zero padding
                    fwrite(zero_buff.data(), sizeof(uint8_t), tileWidthStride * (page_height - writeLinesCount), pFile);
                } else if( partialColumn ) { 
                    // write partial column tile
                    for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
                        fwrite(pTileData, sizeof(uint8_t), partialTileWidthStride, pFile);
                        pTileData += bufferWidthStride;
                    }
                    // write zero padding
                    fwrite(zero_buff.data(), sizeof(uint8_t), (tileWidthStride - partialTileWidthStride) * page_height, pFile);
                } else {
                    // write full tile
                    for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
                        fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                        pTileData += bufferWidthStride;
                    }
                }
            }
        }
    }

        // Write 1+ MIP tiles
    for(uint8_t mipLevel = 1; mipLevel < mipInfo.mipTailStart; mipLevel++) {
        uint32_t mipLevelWidth = mipInfo.mipLevelsDims[mipLevel].x;
        uint32_t mipLevelHeight = mipInfo.mipLevelsDims[mipLevel].y;
        uint32_t mipLevelDepth = mipInfo.mipLevelsDims[mipLevel].z;

        pagesCountX = mipLevelWidth / page_width;
        pagesCountY = mipLevelHeight / page_height;
        pagesCountZ = mipLevelDepth / page_depth;
        pagesCount += pagesCountX * pagesCountY * pagesCountZ;

        partialPageDims = {
            (mipLevelWidth % page_width == 0) ? 0 : mipLevelWidth - page_width * pagesCountX,
            (mipLevelHeight % page_height == 0) ? 0 : mipLevelHeight - page_height * pagesCountY,
            (mipLevelDepth % page_depth == 0) ? 0 : mipLevelDepth - page_depth * pagesCountZ
        };

        if( partialPageDims.x != 0) pagesCountX++;
        if( partialPageDims.y != 0) pagesCountY++;
        if( partialPageDims.z != 0) pagesCountZ++;

        LOG_WARN("Writing mip level %u tiles %u %u ...", mipLevel, pagesCountX, pagesCountY);
        LOG_WARN("Mip level width %u height %u ...", mipLevelWidth, mipLevelHeight);
        LOG_WARN("Partial page dims: %u %u %u", partialPageDims.x, partialPageDims.y, partialPageDims.z);

        partialTileWidthStride = partialPageDims.x * dstBytesPerPixel;

        tiles_buffer.resize(mipLevelWidth * mipLevelHeight * dstBytesPerPixel);
        bufferWidthStride = mipLevelWidth * dstBytesPerPixel;

        oiio::ROI roi(0, mipLevelWidth, 0, mipLevelHeight, 0, 1, 0, dstChannelCount);
        oiio::ImageBufAlgo::resize(srcBuff, "", 0, roi).get_pixels(roi, spec.format, tiles_buffer.data(), oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);

        for(uint32_t z = 0; z < pagesCountZ; z++) {
            for(uint32_t tileIdxY = 0; tileIdxY < pagesCountY; tileIdxY++) {
                
                bool partialRow = false;
                uint writeLinesCount = page_height;
                if ( tileIdxY == (pagesCountY-1) && partialPageDims.y != 0) {
                    writeLinesCount = partialPageDims.y;
                    partialRow = true;
                }

                unsigned char *pTilesLineData = tiles_buffer.data() + bufferWidthStride * page_height * tileIdxY;

                for(uint32_t tileIdxX = 0; tileIdxX < pagesCountX; tileIdxX++) {
                    
                    bool partialColumn = false;
                    if(tileIdxX == (pagesCountX-1) && partialPageDims.x != 0) partialColumn = true;

                    unsigned char *pTileData = pTilesLineData + tileIdxX * tileWidthStride;
                   
                    if( partialRow && partialColumn) {
                        // write partial corner tile
                        for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
                            fwrite(pTileData, sizeof(uint8_t), partialTileWidthStride, pFile);
                            pTileData += bufferWidthStride;
                        }
                        // write zero padding
                        fwrite(zero_buff.data(), sizeof(uint8_t), (tileWidthStride - partialTileWidthStride) * writeLinesCount, pFile);
                        fwrite(zero_buff.data(), sizeof(uint8_t), tileWidthStride * (page_height - writeLinesCount), pFile);
                    } else if( partialRow ) {
                        // write partial bottom tile
                        for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
                            fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                            pTileData += bufferWidthStride;
                        }
                        // write zero padding
                        fwrite(zero_buff.data(), sizeof(uint8_t), tileWidthStride * (page_height - writeLinesCount), pFile);
                    } else if( partialColumn ) {
                        // write partial column tile
                        for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
                            fwrite(pTileData, sizeof(uint8_t), partialTileWidthStride, pFile);
                            pTileData += bufferWidthStride;
                        }
                        // write zero padding
                        fwrite(zero_buff.data(), sizeof(uint8_t), (tileWidthStride - partialTileWidthStride) * page_height, pFile);
                    } else {
                        // write full tile
                        for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
                            fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                            pTileData += bufferWidthStride;
                        }
                    }
                }
            }
        }
    }

    header.pagesCount = pagesCount;

    return true;
}

bool ltxCpuGenerateAndWriteMIPTilesHQFast(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile) {
    return true;
}

bool ltxCpuGenerateAndWriteMIPTilesLQ(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile) {
    return true;
}

bool ltxCpuGenerateAndWriteMIPTilesPOT(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile) {
    assert(pFile);

    // image dimesions
    uint32_t img_width  = header.width;
    uint32_t img_height = header.height;
    uint32_t img_depth  = header.depth;

    // image memory page dimensions
    uint32_t page_width  = header.pageDims.width;
    uint32_t page_height = header.pageDims.height;
    uint32_t page_depth  = header.pageDims.depth;

    // source image buffer spec and pixel format
    auto const& spec = srcBuff.spec();
    auto format = header.format;

    uint32_t pagesCount = 0;
    uint32_t dstChannelCount = getFormatChannelCount(format);
    uint32_t dstChannelBits = getNumChannelBits(format, 0);

    size_t srcBytesPerPixel = spec.pixel_bytes();
    size_t dstBytesPerPixel = dstChannelCount * dstChannelBits / 8;

    size_t tileWidthStride = page_width * dstBytesPerPixel;
    size_t bufferWidthStride = img_width * dstBytesPerPixel;

    uint32_t pagesCountX = img_width / page_width;
    uint32_t pagesCountY = img_height / page_height;
    uint32_t pagesCountZ = img_depth / page_depth;

    std::vector<unsigned char> zero_buff(65536, 0);
    std::vector<unsigned char> tiles_buffer(img_width * page_height * dstBytesPerPixel);

    // write mip level 0
    LOG_WARN("Writing POT texture mip level 0 tiles %u %u ...", pagesCountX, pagesCountY);

    pagesCount += pagesCountX * pagesCountY * pagesCountZ;
    for(uint32_t z = 0; z < pagesCountZ; z++) {
        for(uint32_t tileIdxY = 0; tileIdxY < pagesCountY; tileIdxY++) {
            int y_begin = tileIdxY * page_height;

            uint writeLinesCount = page_height;

            oiio::ROI roi(0, img_width, y_begin, y_begin + writeLinesCount, z, z + 1, /*chans:*/ 0, dstChannelCount);
            srcBuff.get_pixels(roi, spec.format, tiles_buffer.data(), oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);

            unsigned char *pBufferData = tiles_buffer.data();

            for(uint32_t tileIdxX = 0; tileIdxX < pagesCountX; tileIdxX++) {
                unsigned char *pTileData = pBufferData + tileIdxX * tileWidthStride;


                // write full tile
                for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
                    fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                    pTileData += bufferWidthStride;
                }
            }
        }
    }

    // Write 1+ MIP tiles
    oiio::ImageBuf prevBuff;
    oiio::ImageBuf scaledBuff;

    for(uint8_t mipLevel = 1; mipLevel < mipInfo.mipTailStart; mipLevel++) {
        uint32_t mipLevelWidth = mipInfo.mipLevelsDims[mipLevel].x;
        uint32_t mipLevelHeight = mipInfo.mipLevelsDims[mipLevel].y;
        uint32_t mipLevelDepth = mipInfo.mipLevelsDims[mipLevel].z;

        pagesCountX = mipLevelWidth / page_width;
        pagesCountY = mipLevelHeight / page_height;
        pagesCountZ = mipLevelDepth / page_depth;
        pagesCount += pagesCountX * pagesCountY * pagesCountZ;

        LOG_WARN("Writing mip level %u tiles %u %u ...", mipLevel, pagesCountX, pagesCountY);
        LOG_WARN("Mip level width %u height %u ...", mipLevelWidth, mipLevelHeight);

        tiles_buffer.resize(mipLevelWidth * mipLevelHeight * dstBytesPerPixel);
        bufferWidthStride = mipLevelWidth * dstBytesPerPixel;

        oiio::ROI roi(0, mipLevelWidth, 0, mipLevelHeight, 0, 1, 0, dstChannelCount);

        if(mipLevel == 1) {
            prevBuff.copy(srcBuff, spec.format);
        } else {
            prevBuff.copy(scaledBuff, spec.format);
        }

        scaledBuff = oiio::ImageBuf(oiio::ImageSpec(mipLevelWidth, mipLevelHeight, spec.nchannels, spec.format), oiio::InitializePixels::No);

        oiio::ImageBufAlgo::resample(scaledBuff, prevBuff, true, roi);
        scaledBuff.get_pixels(roi, spec.format, tiles_buffer.data(), oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);
        
        for(uint32_t z = 0; z < pagesCountZ; z++) {
            for(uint32_t tileIdxY = 0; tileIdxY < pagesCountY; tileIdxY++) {

                unsigned char *pTilesLineData = tiles_buffer.data() + bufferWidthStride * page_height * tileIdxY;

                for(uint32_t tileIdxX = 0; tileIdxX < pagesCountX; tileIdxX++) {
                    
                    unsigned char *pTileData = pTilesLineData + tileIdxX * tileWidthStride;
                   
                    // write tile
                    for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
                        fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                        pTileData += bufferWidthStride;
                    }
                }
            }
        }
    }

    header.pagesCount = pagesCount;

    return true;
}

static oiio::cspan<float> dbg_cls_light[8] = {
    {1.0, 0.0, 0.0, 1.0},
    {0.0, 1.0, 0.0, 1.0},
    {0.0, 0.0, 1.0, 1.0},
    
    {1.0, 1.0, 0.0, 1.0},
    {0.0, 1.0, 1.0, 1.0},
    {1.0, 0.0, 1.0, 1.0},

    {1.0, 1.0, 1.0, 1.0},
    {1.0, 0.5, 0.0, 1.0},
};

static oiio::cspan<float> dbg_cls_dark[8] = {
    {0.5, 0.0, 0.0, 1.0},
    {0.0, 0.5, 0.0, 1.0},
    {0.0, 0.0, 0.5, 1.0},
    
    {0.5, 0.5, 0.0, 1.0},
    {0.0, 0.5, 0.5, 1.0},
    {0.5, 0.0, 0.5, 1.0},

    {0.5, 0.5, 0.5, 1.0},
    {0.5, 0.25,0.0, 1.0},
};

bool ltxCpuGenerateDebugMIPTiles(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile) {
    assert(pFile);

    // image dimesions
    uint32_t img_width  = header.width;
    uint32_t img_height = header.height;
    uint32_t img_depth  = header.depth;

    // image memory page dimensions
    uint32_t page_width  = header.pageDims.width;
    uint32_t page_height = header.pageDims.height;
    uint32_t page_depth  = header.pageDims.depth;

    // source image buffer spec and pixel format
    auto const& spec = srcBuff.spec();
    auto format = header.format;

    uint32_t pagesCount = 0;
    uint32_t dstChannelCount = getFormatChannelCount(format);
    uint32_t dstChannelBits = getNumChannelBits(format, 0);

    size_t srcBytesPerPixel = spec.pixel_bytes();
    size_t dstBytesPerPixel = dstChannelCount * dstChannelBits / 8;

    size_t tileWidthStride = page_width * dstBytesPerPixel;
    size_t bufferWidthStride = img_width * dstBytesPerPixel;

    uint32_t pagesCountX = img_width / page_width;
    uint32_t pagesCountY = img_height / page_height;
    uint32_t pagesCountZ = img_depth / page_depth;

    uint3 partialPageDims = {
        (img_width % page_width == 0) ? 0 : img_width - page_width * pagesCountX,
        (img_height % page_height == 0) ? 0 : img_height - page_height * pagesCountY,
        (img_depth % page_depth == 0) ? 0 : img_depth - page_depth * pagesCountZ
    };

    size_t partialTileWidthStride = partialPageDims.x * dstBytesPerPixel;

    if( partialPageDims.x != 0) pagesCountX++;
    if( partialPageDims.y != 0) pagesCountY++;
    if( partialPageDims.z != 0) pagesCountZ++;

    std::vector<unsigned char> zero_buff(65536, 0);
    std::vector<unsigned char> tiles_buffer(img_width * page_height * dstBytesPerPixel);

    // write mip level 0
    LOG_WARN("Writing mip level 0 tiles %u %u ...", pagesCountX, pagesCountY);
    LOG_WARN("Partial page dims: %u %u %u", partialPageDims.x, partialPageDims.y, partialPageDims.z);

    oiio::ImageBuf A(oiio::ImageSpec(img_width, img_height, 4, oiio::TypeDesc::UINT8));
    oiio::ImageBufAlgo::checker( A, 128, 128,1, dbg_cls_dark[0], dbg_cls_light[0], 0, 0, 0);

    pagesCount += pagesCountX * pagesCountY * pagesCountZ;
    for(uint32_t z = 0; z < pagesCountZ; z++) {
        for(uint32_t tileIdxY = 0; tileIdxY < pagesCountY; tileIdxY++) {
            int y_begin = tileIdxY * page_height;

            bool partialRow = false;
            uint writeLinesCount = page_height;
            if ( tileIdxY == (pagesCountY-1) && partialPageDims.y != 0) {
                writeLinesCount = partialPageDims.y;
                partialRow = true;
            }

            oiio::ROI roi(0, img_width, y_begin, y_begin + writeLinesCount, z, z + 1, /*chans:*/ 0, dstChannelCount);
            A.get_pixels(roi, spec.format, tiles_buffer.data(), oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);

            unsigned char *pBufferData = tiles_buffer.data();

            for(uint32_t tileIdxX = 0; tileIdxX < pagesCountX; tileIdxX++) {

                bool partialColumn = false;
                if(tileIdxX == (pagesCountX-1) && partialPageDims.x != 0) partialColumn = true;
                
                unsigned char *pTileData = pBufferData + tileIdxX * tileWidthStride;

                if( partialRow && partialColumn) {
                    // write partial corner tile
                    for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
                        fwrite(pTileData, sizeof(uint8_t), partialTileWidthStride, pFile);
                        pTileData += bufferWidthStride;
                    }
                    // write zero padding
                    fwrite(zero_buff.data(), sizeof(uint8_t), (tileWidthStride - partialTileWidthStride) * writeLinesCount, pFile);
                    fwrite(zero_buff.data(), sizeof(uint8_t), tileWidthStride * (page_height - writeLinesCount), pFile);
                } else if( partialRow ) {
                    // write partial bottom tile
                    for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
                        fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                        pTileData += bufferWidthStride;
                    }
                    // write zero padding
                    fwrite(zero_buff.data(), sizeof(uint8_t), tileWidthStride * (page_height - writeLinesCount), pFile);
                } else if( partialColumn ) { 
                    // write partial column tile
                    for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
                        fwrite(pTileData, sizeof(uint8_t), partialTileWidthStride, pFile);
                        pTileData += bufferWidthStride;
                    }
                    // write zero padding
                    fwrite(zero_buff.data(), sizeof(uint8_t), (tileWidthStride - partialTileWidthStride) * page_height, pFile);
                } else {
                    // write full tile
                    for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
                        fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                        pTileData += bufferWidthStride;
                    }
                }
            }
        }
    }

        // Write 1+ MIP tiles
    for(uint8_t mipLevel = 1; mipLevel < mipInfo.mipTailStart; mipLevel++) {
        uint32_t mipLevelWidth = mipInfo.mipLevelsDims[mipLevel].x;
        uint32_t mipLevelHeight = mipInfo.mipLevelsDims[mipLevel].y;
        uint32_t mipLevelDepth = mipInfo.mipLevelsDims[mipLevel].z;

        pagesCountX = mipLevelWidth / page_width;
        pagesCountY = mipLevelHeight / page_height;
        pagesCountZ = mipLevelDepth / page_depth;
        pagesCount += pagesCountX * pagesCountY * pagesCountZ;

        partialPageDims = {
            (mipLevelWidth % page_width == 0) ? 0 : mipLevelWidth - page_width * pagesCountX,
            (mipLevelHeight % page_height == 0) ? 0 : mipLevelHeight - page_height * pagesCountY,
            (mipLevelDepth % page_depth == 0) ? 0 : mipLevelDepth - page_depth * pagesCountZ
        };

        if( partialPageDims.x != 0) pagesCountX++;
        if( partialPageDims.y != 0) pagesCountY++;
        if( partialPageDims.z != 0) pagesCountZ++;

        LOG_WARN("Writing mip level %u tiles %u %u ...", mipLevel, pagesCountX, pagesCountY);
        LOG_WARN("Mip level width %u height %u ...", mipLevelWidth, mipLevelHeight);
        LOG_WARN("Partial page dims: %u %u %u", partialPageDims.x, partialPageDims.y, partialPageDims.z);

        partialTileWidthStride = partialPageDims.x * dstBytesPerPixel;

        tiles_buffer.resize(mipLevelWidth * mipLevelHeight * dstBytesPerPixel);
        bufferWidthStride = mipLevelWidth * dstBytesPerPixel;

        oiio::ROI roi(0, mipLevelWidth, 0, mipLevelHeight, 0, 1, 0, dstChannelCount);
        //oiio::ImageBufAlgo::resize(srcBuff, "", 0, roi).get_pixels(roi, spec.format, tiles_buffer.data(), oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);
        oiio::ImageBuf A(oiio::ImageSpec(mipLevelWidth, mipLevelHeight, 4, oiio::TypeDesc::UINT8));
        oiio::ImageBufAlgo::checker( A, 128/ pow(2, mipLevel), 128/ pow(2, mipLevel),1, dbg_cls_dark[mipLevel], dbg_cls_light[mipLevel], 0, 0, 0);
        A.get_pixels(roi, spec.format, tiles_buffer.data(), oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);

        for(uint32_t z = 0; z < pagesCountZ; z++) {
            for(uint32_t tileIdxY = 0; tileIdxY < pagesCountY; tileIdxY++) {
                
                bool partialRow = false;
                uint writeLinesCount = page_height;
                if ( tileIdxY == (pagesCountY-1) && partialPageDims.y != 0) {
                    writeLinesCount = partialPageDims.y;
                    partialRow = true;
                }

                unsigned char *pTilesLineData = tiles_buffer.data() + bufferWidthStride * page_height * tileIdxY;

                for(uint32_t tileIdxX = 0; tileIdxX < pagesCountX; tileIdxX++) {
                    
                    bool partialColumn = false;
                    if(tileIdxX == (pagesCountX-1) && partialPageDims.x != 0) partialColumn = true;

                    unsigned char *pTileData = pTilesLineData + tileIdxX * tileWidthStride;
                   
                    if( partialRow && partialColumn) {
                        // write partial corner tile
                        for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
                            fwrite(pTileData, sizeof(uint8_t), partialTileWidthStride, pFile);
                            pTileData += bufferWidthStride;
                        }
                        // write zero padding
                        fwrite(zero_buff.data(), sizeof(uint8_t), (tileWidthStride - partialTileWidthStride) * writeLinesCount, pFile);
                        fwrite(zero_buff.data(), sizeof(uint8_t), tileWidthStride * (page_height - writeLinesCount), pFile);
                    } else if( partialRow ) {
                        // write partial bottom tile
                        for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
                            fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                            pTileData += bufferWidthStride;
                        }
                        // write zero padding
                        fwrite(zero_buff.data(), sizeof(uint8_t), tileWidthStride * (page_height - writeLinesCount), pFile);
                    } else if( partialColumn ) {
                        // write partial column tile
                        for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
                            fwrite(pTileData, sizeof(uint8_t), partialTileWidthStride, pFile);
                            pTileData += bufferWidthStride;
                        }
                        // write zero padding
                        fwrite(zero_buff.data(), sizeof(uint8_t), (tileWidthStride - partialTileWidthStride) * page_height, pFile);
                    } else {
                        // write full tile
                        for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
                            fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                            pTileData += bufferWidthStride;
                        }
                    }
                }
            }
        }
    }

    // write debug tail
    std::vector<unsigned char> tail_buff(65536, 255);
    fwrite(tail_buff.data(), sizeof(uint8_t), 65536, pFile);
    pagesCount++;

    fwrite(tail_buff.data(), sizeof(uint8_t), 65536, pFile);
    pagesCount++;

    fwrite(tail_buff.data(), sizeof(uint8_t), 65536, pFile);
    pagesCount++;

    fwrite(tail_buff.data(), sizeof(uint8_t), 65536, pFile);
    pagesCount++;

    fwrite(tail_buff.data(), sizeof(uint8_t), 65536, pFile);
    pagesCount++;

    header.pagesCount = pagesCount;

    return true;
}


}  // namespace Falcor
