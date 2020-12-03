/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#include <algorithm>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "stdafx.h"
#include "Bitmap.h"
#include "LTX_Bitmap.h"
#include "BitmapUtils.h"
#include "LTX_BitmapUtils.h"

#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/TextureManager.h"
#include "Falcor/Utils/Debug/debug.h"

namespace Falcor {

namespace oiio = OpenImageIO_v2_3;

static const size_t kLtxHeaderOffset = sizeof(LTX_Header);

static struct {
    bool operator()(size_t a, size_t b) const {   
        return a < b;
    }   
} pageSort;

bool LTX_Bitmap::checkMagic(const unsigned char* magic) {
    int match = 0;
    match += memcmp(&gLtxFileMagic[0], &magic[0], 4);
    match += memcmp(&gLtxFileMagic[7], &magic[7], 5);
    if(match == 0 && 48 <= magic[5] && magic[5] <= 57 && 48 <= magic[6] && magic[6] <= 57)
        return true;

    return false;
}

void LTX_Bitmap::makeMagic(uint8_t minor, uint8_t major, unsigned char *magic) {
    if (minor > 9 || major > 9) {
        LOG_ERR("Major and minor versions should be less than 10 !!!");
        return;
    }

    memcpy(magic, gLtxFileMagic, 12);
    magic[5] = 48 + static_cast<unsigned char>(major);
    magic[6] = 48 + static_cast<unsigned char>(minor);
}

LTX_Bitmap::UniquePtr LTX_Bitmap::createFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool isTopDown) {
    std::ifstream file (filename, std::ios::in | std::ios::binary);
    if (file.is_open()) {
        unsigned char magic[12];
        file.read((char *)&magic, 12);
        
        if (!checkMagic(magic)) {
            LOG_ERR("Non LTX file provided (magic test failed) !!!");
            return nullptr;
        }

        file.seekg(0, file.beg);
    } else {
        LOG_ERR("Error opening file: %s !!!", filename.c_str());
        return nullptr;
    }

    auto pLtxBitmap = new LTX_Bitmap();
    pLtxBitmap->mFilename = filename;
    pLtxBitmap->mpFile = fopen(filename.c_str(), "rb");
    fread(&pLtxBitmap->mHeader, sizeof(LTX_Header), 1, pLtxBitmap->mpFile );

    fseek(pLtxBitmap->mpFile, 0L, SEEK_END);
    size_t mDataSize = ftell(pLtxBitmap->mpFile) - sizeof(LTX_Header);
    fclose(pLtxBitmap->mpFile);
    pLtxBitmap->mpFile = nullptr;

    return UniquePtr(pLtxBitmap);
}

LTX_Bitmap::LTX_Bitmap() {

}

LTX_Bitmap::~LTX_Bitmap() {
    if(mpFile != nullptr)
        fclose(mpFile);
}

static ResourceFormat getFormatOIIO(unsigned char baseType, int nchannels) {
    using BASETYPE = oiio::TypeDesc::BASETYPE;
    switch (nchannels) {
        case 1:
            switch (BASETYPE(baseType)) {
                case BASETYPE::UINT8:
                    return ResourceFormat::R8Unorm;
                case BASETYPE::UINT32:
                    return ResourceFormat::R32Uint;
                case BASETYPE::FLOAT:
                    return ResourceFormat::R32Float;
                default:
                    should_not_get_here();
                    break;
            }
            break;
        case 2:
            switch (BASETYPE(baseType)) {
                case BASETYPE::UINT8:
                    return ResourceFormat::RG8Unorm;
                case BASETYPE::UINT32:
                    return ResourceFormat::RG32Uint;
                case BASETYPE::FLOAT:
                    return ResourceFormat::RG32Float;
                default:
                    should_not_get_here();
                    break;
            }
            break;
        case 3:
            switch (BASETYPE(baseType)) {
                case BASETYPE::UINT8:
                    return ResourceFormat::RGB8Unorm;
                case BASETYPE::UINT32:
                    return ResourceFormat::RGB32Uint;
                case BASETYPE::FLOAT:
                    return ResourceFormat::RGB32Float;
                default:
                    should_not_get_here();
                    break;
            }
            break;

        case 4:
            switch (BASETYPE(baseType)) {
                case BASETYPE::UINT8:
                    return ResourceFormat::RGBA8Unorm;
                case BASETYPE::UINT32:
                    return ResourceFormat::RGBA32Uint;
                case BASETYPE::FLOAT:
                    return ResourceFormat::RGBA32Float;
                default:
                    should_not_get_here();
                    break;
            }
            break;
        default:
            should_not_get_here();
            break;
    }
    return ResourceFormat::RGBA8Unorm;
}

static ResourceFormat getDestFormat(ResourceFormat format) {
    switch (format) {
        case ResourceFormat::RGB8Unorm:
            return ResourceFormat::RGBA8Unorm;  // this should force 24bit to 32bit conversion
        case ResourceFormat::RGB32Uint:
            return ResourceFormat::RGBA32Uint;  // this should force 96bit to 128bit conversion
        case ResourceFormat::RGB32Float:
            return ResourceFormat::RGBA32Float; // this should force 96bit to 128bit conversion
        default:
            break;
    }

    return format;
}

static uint3 getPageDims(ResourceFormat format) {
    uint32_t channelCount = getFormatChannelCount(format);
    uint32_t totalBits = 0;

    for(uint i = 0; i < channelCount; i++) totalBits += getNumChannelBits(format, i);

    switch(totalBits) {
        case 8:
            return {256, 256, 1};
        case 16:
            return {256, 128, 1};
        case 32:
            return {128, 128, 1};
        case 64:
            return {128, 64, 1};
        case 128:
            return {64, 64, 1};
        default:
            should_not_get_here();
            break;
    } 

    return {0, 0, 0};
}

void LTX_Bitmap::convertToKtxFile(std::shared_ptr<Device> pDevice, const std::string& srcFilename, const std::string& dstFilename, bool isTopDown) {
    auto in = oiio::ImageInput::open(srcFilename);
    if (!in) {
        LOG_ERR("Error reading image file %s", srcFilename.c_str());
    }
    const oiio::ImageSpec &spec = in->spec();

    LOG_WARN("OIIO channel formats size: %zu", spec.channelformats.size());
    LOG_WARN("OIIO is signed: %s", spec.format.is_signed() ? "YES" : "NO");
    LOG_WARN("OIIO is float: %s", spec.format.is_floating_point() ? "YES" : "NO");
    LOG_WARN("OIIO basetype %u", oiio::TypeDesc::BASETYPE(spec.format.basetype));
    LOG_WARN("OIIO bytes per pixel %zu", spec.pixel_bytes());
    LOG_WARN("OIIO nchannels %i", spec.nchannels);
    
    auto srcFormat = getFormatOIIO(spec.format.basetype, spec.nchannels);
    LOG_WARN("Source ResourceFormat from OIIO: %s", to_string(srcFormat).c_str());
    auto dstFormat = getDestFormat(srcFormat);

    uint3 pageDims = getPageDims(dstFormat);
    uint3 srcDims = {spec.width, spec.height, spec.depth};
    uint3 dstDims = srcDims;

    // check for image size is able to fin integer number of pages along each axis.
    // if not we have to load the whole image and resize it to the appropriate size
    int cx = (int)spec.width / pageDims.x;
    int cy = (int)spec.height / pageDims.y;

    int dx = spec.width % pageDims.x;
    int dy = spec.height % pageDims.y;

    oiio::ImageBuf srcBuff; // used when source resize/resample required. we use this to compute secont mip level and avoid double insterpolation
    oiio::ImageBuf dstBuff; // used when source resize/resample required
    bool doSourceResize = false;
    //if (dx != 0) dstDims.x = (cx + 1) * pageDims.x;
    //if (dy != 0) dstDims.y = (cy + 1) * pageDims.y;
    
    if (srcDims != dstDims) {
        doSourceResize = true;
        LOG_WARN("Source image have to be resized !!!");

        srcBuff = oiio::ImageBuf(srcFilename); 
        oiio::ROI roi (0, dstDims.x, 0, dstDims.y, 0, 1, /*chans:*/ 0, srcBuff.nchannels());
        dstBuff = oiio::ImageBufAlgo::resize(srcBuff, "", 0, roi);
    }
    
    // check if we can read tiles natively
    bool canReadTiles = (spec.tile_width != 0 && spec.tile_height != 0) ? true : false; 
    LOG_WARN("OIIO can read tiles %s", canReadTiles ? "YES" : "NO");

    // make header
    LTX_Header header;
    unsigned char magic[12];
    makeMagic(9, 8, &header.magic[0]);

    header.width = dstDims.x;
    header.height = dstDims.y;
    header.depth = dstDims.z;
    header.pageDims = {pageDims.x, pageDims.y, pageDims.z};
    header.pageDataSize = 65536;
    header.arrayLayersCount = 1;
    header.mipLevelsCount = Texture::getMaxMipCount({dstDims.x, dstDims.y, dstDims.z});
    header.format = dstFormat;//pBitmap->getFormat();

    // open file
    FILE *pFile = fopen(dstFilename.c_str(), "wb");

    // write header
    uint mipLevelsCount = header.mipLevelsCount;
    fwrite(&header, sizeof(unsigned char), sizeof(LTX_Header), pFile);

    // 
    uint32_t dstChannelCount = getFormatChannelCount(dstFormat);
    uint32_t dstChannelBits = getNumChannelBits(dstFormat, 0);

    size_t srcBytesPerPixel = spec.pixel_bytes();
    size_t dstBytesPerPixel = dstChannelCount * dstChannelBits / 8;

    size_t tileWidthStride = pageDims.x * dstBytesPerPixel;
    size_t bufferWidthStride = dstDims.x * srcBytesPerPixel;

    std::vector<unsigned char> mip_tiles_buffer;

    uint32_t pagesNumX = dstDims.x / pageDims.x;
    uint32_t pagesNumY = dstDims.y / pageDims.y;
    uint32_t pagesNumZ = dstDims.z / pageDims.z;

    uint3 lastPageDims = {
        (srcDims.x % pageDims.x == 0) ? 0 : srcDims.x - pageDims.x * pagesNumX,
        (srcDims.y % pageDims.y == 0) ? 0 : srcDims.y - pageDims.y * pagesNumY,
        (srcDims.z % pageDims.z == 0) ? 0 : srcDims.z - pageDims.z * pagesNumZ
    };

    size_t partialTileWidthStride = lastPageDims.x * dstBytesPerPixel;

    LOG_WARN("LTX last page dims: %u %u %u", lastPageDims.x, lastPageDims.y, lastPageDims.z);

    if( lastPageDims.x != 0) pagesNumX++;
    if( lastPageDims.y != 0) pagesNumY++;
    if( lastPageDims.z != 0) pagesNumZ++;

    std::vector<unsigned char> zero_buff(65536, 0);
    std::vector<unsigned char> tiles_buffer(srcDims.x * pageDims.y * srcBytesPerPixel);

    if (mipLevelsCount > 1) {
        mip_tiles_buffer.resize((pagesNumX / 2 + pagesNumX % 2) * pageDims.x * (pagesNumY / 2 + pagesNumY % 2) * pageDims.y * dstBytesPerPixel);
    }

    // write mip level 0
    LOG_WARN("Writing mip level 0 tiles...");

    for(uint32_t z = 0; z < pagesNumZ; z++) {
        for(uint32_t tileIdxY = 0; tileIdxY < pagesNumY; tileIdxY++) {
            int y_begin = tileIdxY * pageDims.y;
            
            in->read_scanlines(0, 0, y_begin, y_begin + pageDims.y, z, 0, spec.nchannels, spec.format, tiles_buffer.data());

            if(srcFormat != dstFormat) {
                convertToRGBA(srcFormat, dstFormat, dstDims.x, pageDims.y, tiles_buffer);
                bufferWidthStride = dstDims.x * dstBytesPerPixel;
            }
            unsigned char *pBufferData = tiles_buffer.data();

            for(uint32_t tileIdxX = 0; tileIdxX < pagesNumX; tileIdxX++) {
                
                unsigned char *pTileData = pBufferData + tileIdxX * tileWidthStride;

                if( tileIdxX == (pagesNumX-1) && lastPageDims.x != 0) {
                    // write partial tile
                    for(uint32_t lineNum = 0; lineNum < pageDims.y; lineNum++) {
                        fwrite(pTileData, sizeof(uint8_t), partialTileWidthStride, pFile);
                        pTileData += bufferWidthStride;
                    }
                    // write zero padding
                    fwrite(zero_buff.data(), sizeof(uint8_t) * pageDims.y, tileWidthStride - partialTileWidthStride, pFile);
                } else {
                    // write normal tile
                    for(uint32_t lineNum = 0; lineNum < pageDims.y; lineNum++) {
                        fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                        pTileData += bufferWidthStride;
                    }
                }
            }
        }
    }

    // free resized buffer data
    if(doSourceResize) dstBuff.reset();

    // write mip levels 
    pagesNumX = pagesNumX / 2 + pagesNumX % 2;
    pagesNumY = pagesNumY / 2 + pagesNumY % 2;

    if(srcFormat != dstFormat && doSourceResize) {
        LOG_WARN("Downsampling mip level 1 tiles...");
        // If we did 0 mip level using resized buffer wa have to build 1 mip level from srcBuff to avoid double resampling
        oiio::ROI roi(0, pagesNumX * pageDims.x, 0, pagesNumY * pageDims.y, 0, 1, /*chans:*/ 0, spec.nchannels);
        oiio::ImageBufAlgo::resize(srcBuff, "", 0, roi).get_pixels(roi, spec.format, mip_tiles_buffer.data(), oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);
        
        // free source buffer data
        srcBuff.reset();
        // convert mip tiles buffer to RGBA as we reused original srcBuff and thus source channels count
        convertToRGBA(srcFormat, dstFormat, pagesNumX * pageDims.x, pagesNumY * pageDims.y, mip_tiles_buffer);
    }

    
    for(uint8_t mipLevel = 1; mipLevel < mipLevelsCount; mipLevel++) {
        LOG_WARN("Writing mip level %u tiles %u %u ...", mipLevel, pagesNumX, pagesNumY);

        size_t mipTilesBufferWidthStride = pagesNumX * pageDims.x * dstBytesPerPixel;

        for(uint32_t z = 0; z < pagesNumZ; z++) {
            for(uint32_t tileIdxY = 0; tileIdxY < pagesNumY; tileIdxY++) {

                unsigned char *pBufferData = mip_tiles_buffer.data() + mipTilesBufferWidthStride * pageDims.y * tileIdxY;

                for(uint32_t tileIdxX = 0; tileIdxX < pagesNumX; tileIdxX++) {
                    
                    unsigned char *pTileData = pBufferData + tileIdxX * tileWidthStride;

                    for(uint32_t lineNum = 0; lineNum < pageDims.y; lineNum++) {
                        //LOG_WARN("write tile %u line %u", tileIdxX, lineNum);
                        fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                        pTileData += mipTilesBufferWidthStride;
                    }
                }
            }
        }
    
        oiio::ImageSpec tmpSpec(pagesNumX * pageDims.x, pagesNumY * pageDims.y, dstChannelCount, spec.format);
        dstBuff.reset(spec, mip_tiles_buffer.data());
        
        pagesNumX = pagesNumX / 2 + pagesNumX % 2;
        pagesNumY = pagesNumY / 2 + pagesNumY % 2;
        //std::fill(mip_tiles_buffer.begin(),mip_tiles_buffer.end(),64*mipLevel);
        mip_tiles_buffer.resize(pagesNumX * pageDims.x * pagesNumY * pageDims.y * dstBytesPerPixel);

        oiio::ROI roi(0, pagesNumX * pageDims.x, 0, pagesNumY * pageDims.y, 0, 1, /*chans:*/ 0, dstChannelCount);
        oiio::ImageBufAlgo::resize(dstBuff, "", 0, roi);
        dstBuff.get_pixels(roi, spec.format, mip_tiles_buffer.data(), oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);

    }

    fclose(pFile);
}

void LTX_Bitmap::readPageData(size_t pageNum, void *pData) {
    mpFile = fopen(mFilename.c_str(), "rb");
    fseek(mpFile, kLtxHeaderOffset + pageNum * mHeader.pageDataSize, SEEK_SET);
    fread(pData, 1, mHeader.pageDataSize, mpFile);
    fclose(mpFile);
    mpFile = nullptr;
}

void LTX_Bitmap::readPagesData(std::vector<std::pair<size_t, void*>>& pages, bool unsorted) {
    mpFile = fopen(mFilename.c_str(), "rb");

    if (unsorted) {
        std::sort (pages.begin(), pages.end(), [](std::pair<size_t, void*> a, std::pair<size_t, void*> b) {
            return a.first < b.first;   
        });
    }

    for(auto& page: pages) {
        fseek(mpFile, kLtxHeaderOffset + page.first * mHeader.pageDataSize, SEEK_SET);
    }
    
    fclose(mpFile);
    mpFile = nullptr;
}

}  // namespace Falcor
