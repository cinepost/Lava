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

#include "stdafx.h"
#include "Bitmap.h"
#include "LTX_Bitmap.h"
#include "BitmapUtils.h"

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

    uint3 pageDims = {64, 64, 1};  // test hardcode
    uint3 srcDims = {spec.width, spec.height, spec.depth};

    bool canReadTiles = (spec.tile_width != 0 && spec.tile_height != 0) ? true : false; 
    LOG_WARN("OIIO can read tiles %s", canReadTiles ? "YES" : "NO");

    // make header
    LTX_Header header;
    unsigned char magic[12];
    makeMagic(9, 8, &header.magic[0]);

    header.width = srcDims[0];
    header.height = srcDims[1];
    header.depth = srcDims[2];
    header.pageDims = {pageDims[0], pageDims[1], pageDims[2]};
    header.pageDataSize = 65536;
    header.arrayLayersCount = 1;
    header.mipLevelsCount = Texture::getMaxMipCount({srcDims[0], srcDims[1], srcDims[2]});
    header.format = dstFormat;//pBitmap->getFormat();

    // open file
    FILE *pFile = fopen(dstFilename.c_str(), "wb");

    // write header
    fwrite(&header, sizeof(unsigned char), sizeof(LTX_Header), pFile);

    // first mip level naive copy
    uint32_t pagesNumX = srcDims[0] / pageDims[0];
    uint32_t pagesNumY = srcDims[1] / pageDims[1];
    uint32_t pagesNumZ = srcDims[2] / pageDims[2];

    size_t srcBytesPerPixel = spec.pixel_bytes(); // hardcode 4 bytes per pixel

    uint32_t dstChannelCount = getFormatChannelCount(dstFormat);
    uint32_t dstChannelBits = getNumChannelBits(dstFormat, 0);

    size_t tileWidthStride = pageDims[0] * dstChannelCount * dstChannelBits / 8;
    size_t bufferWidthStride = srcDims[0] * srcBytesPerPixel;

    std::vector<unsigned char> tiles_buffer (spec.width * pageDims[1] * srcBytesPerPixel);

    for(uint32_t z = 0; z < pagesNumZ; z++) {
        for(uint32_t tileIdxY = 0; tileIdxY < pagesNumY; tileIdxY++) {
            int y_begin = tileIdxY * pageDims[1];
            
            in->read_scanlines(0, 0, y_begin, y_begin + pageDims[1], z, 0, spec.nchannels, spec.format, tiles_buffer.data());

            unsigned char *pBufferData = tiles_buffer.data();
            if(srcFormat != dstFormat) {
                auto convertedData = convertToRGBA32Float(srcFormat, spec.width, pageDims[1], pBufferData);
                bufferWidthStride = srcDims[0] * dstChannelCount * dstChannelBits / 8;

                LOG_WARN("Converted buffer size (bytes): %zu", convertedData.size() * 4);
                LOG_WARN("Tile width stride: %zu", tileWidthStride);
                LOG_WARN("Buffer width stride: %zu", bufferWidthStride);
                pBufferData = static_cast<unsigned char*>(static_cast<void*>(convertedData.data()));
            }

            for(uint32_t tileIdxX = 0; tileIdxX < pagesNumX; tileIdxX++) {
                
                unsigned char *pTileData = pBufferData + tileIdxX * tileWidthStride;

                for(uint32_t lineNum = 0; lineNum < pageDims[1]; lineNum++) {
                    LOG_WARN("write tile %u line %u", tileIdxX, lineNum);
                    fwrite(pTileData, sizeof(uint8_t), tileWidthStride, pFile);
                    pTileData += bufferWidthStride;
                }
            
            }
        }
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

void LTX_Bitmap::readPagesData(std::vector<std::pair<size_t, void*>>& pages) {
    mpFile = fopen(mFilename.c_str(), "rb");

    std::sort (pages.begin(), pages.end(), [](std::pair<size_t, void*> a, std::pair<size_t, void*> b) {
        return a.first < b.first;   
    });

    for(auto& page: pages) {
        fseek(mpFile, kLtxHeaderOffset + page.first * mHeader.pageDataSize, SEEK_SET);
    }
    
    fclose(mpFile);
    mpFile = nullptr;
}

}  // namespace Falcor
