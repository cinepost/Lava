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
#include <stdlib.h>
#include <algorithm>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include "stdafx.h"
#include "Bitmap.h"
#include "LTX_Bitmap.h"
#include "BitmapUtils.h"
#include "LTX_BitmapAlgo.h"
#include "LTX_BitmapUtils.h"

#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/ResourceManager.h"
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

LTX_Bitmap::SharedConstPtr LTX_Bitmap::createFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool isTopDown) {
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
    
    auto pFile = fopen(filename.c_str(), "rb");
    fread(&pLtxBitmap->mHeader, sizeof(LTX_Header), 1, pFile );

    fseek(pFile, 0L, SEEK_END);
    size_t mDataSize = ftell(pFile) - sizeof(LTX_Header);
    fclose(pFile);

    return SharedConstPtr(pLtxBitmap);
}

LTX_Bitmap::LTX_Bitmap() {

}

LTX_Bitmap::~LTX_Bitmap() {

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

static ResourceFormat getDestFormat(const ResourceFormat &format) {
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

static uint3 getPageDims(const ResourceFormat &format) {
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

static LTX_MipInfo calcMipInfo(const uint3& imgDims, const ResourceFormat &format) {
    LTX_MipInfo info;

    info.mipLevelsCount = Texture::getMaxMipCount(imgDims);
    info.mipTailStart = info.mipLevelsCount;
    info.pageDims = getPageDims(format);
    info.mipLevelsDims = std::vector<uint3>(info.mipLevelsCount);

    // pre calculate image dimensions for each mip level (mipLevelDims)
    for( uint mipLevel = 0; mipLevel < info.mipLevelsCount; mipLevel++) {
        info.mipLevelsDims[mipLevel].x = imgDims.x / pow(2, mipLevel);
        info.mipLevelsDims[mipLevel].y = imgDims.y / pow(2, mipLevel);
        info.mipLevelsDims[mipLevel].z = 1;//imgDims.z / pow(2, mipLevel);
    }

    // find mip tail starting mip level
    uint8_t cMipLevel = 0;
    for( auto const& mipDims: info.mipLevelsDims) {
        if( (info.pageDims.x > mipDims.x) && (info.pageDims.y > mipDims.y)) {
            info.mipTailStart = cMipLevel;
            break;
        }
        cMipLevel += 1;
    }

    return info;
}

void LTX_Bitmap::convertToKtxFile(std::shared_ptr<Device> pDevice, const std::string& srcFilename, const std::string& dstFilename, bool isTopDown) {
    auto in = oiio::ImageInput::open(srcFilename);
    if (!in) {
        LOG_ERR("Error reading image file %s", srcFilename.c_str());
    }
    const oiio::ImageSpec &spec = in->spec();

    LOG_DBG("OIIO channel formats size: %zu", spec.channelformats.size());
    LOG_DBG("OIIO is signed: %s", spec.format.is_signed() ? "YES" : "NO");
    LOG_DBG("OIIO is float: %s", spec.format.is_floating_point() ? "YES" : "NO");
    LOG_DBG("OIIO basetype %u", oiio::TypeDesc::BASETYPE(spec.format.basetype));
    LOG_DBG("OIIO bytes per pixel %zu", spec.pixel_bytes());
    LOG_DBG("OIIO nchannels %i", spec.nchannels);
    
    auto srcFormat = getFormatOIIO(spec.format.basetype, spec.nchannels);
    LOG_WARN("Source ResourceFormat from OIIO: %s", to_string(srcFormat).c_str());
    auto dstFormat = getDestFormat(srcFormat);

    oiio::ImageBuf srcBuff;

    if(spec.nchannels == 3 ) {
        // This is an RGB image. Convert it to RGBA image with full-on alpha channel 
        
        int channelorder[] = { 0, 1, 2, -1 /*use a float value*/ };
        float channelvalues[] = { 0 /*ignore*/, 0 /*ignore*/, 0 /*ignore*/, 1.0 };
        std::string channelnames[] = { "", "", "", "A" };
        oiio::ImageBuf tmpBuffRGB(srcFilename);
        srcBuff = oiio::ImageBufAlgo::channels(tmpBuffRGB, 4, channelorder, channelvalues, channelnames);
    } else {
        srcBuff = oiio::ImageBuf(srcFilename);
    }

    // TODO: make image analysis (pre scale down source with blurry data) and reflect that in dstDims
    uint3 srcDims = {spec.width, spec.height, spec.depth};
    uint3 dstDims = srcDims;

    // calc mip related info
    auto mipInfo = calcMipInfo(dstDims, dstFormat);
    
    // make header
    LTX_Header header;
    unsigned char magic[12];
    makeMagic(9, 8, &header.magic[0]);

    header.srcLastWriteTime = fs::last_write_time(srcFilename);
    header.width = dstDims.x;
    header.height = dstDims.y;
    header.depth = dstDims.z;
    header.pageDims = {mipInfo.pageDims.x, mipInfo.pageDims.y, mipInfo.pageDims.z};
    header.pageDataSize = 65536;
    header.pagesCount = 0;
    header.arrayLayersCount = 1;
    header.mipLevelsCount = mipInfo.mipLevelsCount;
    header.mipTailStart   = mipInfo.mipTailStart;
    header.format = dstFormat;//pBitmap->getFormat();

    // open file and write header
    FILE *pFile = fopen(dstFilename.c_str(), "wb");
    fwrite(&header, sizeof(unsigned char), sizeof(LTX_Header), pFile);

    LOG_WARN("LTX Mip page dims %u %u %u ...", mipInfo.pageDims.x, mipInfo.pageDims.y, mipInfo.pageDims.z);

    if(ltxCpuGenerateAndWriteMIPTilesHQSlow(header, mipInfo, srcBuff, pFile)) {
    //if(ltxCpuGenerateDebugMIPTiles(header, mipInfo, srcBuff, pFile)) {
        // re-write header as it might get modified ... 
        // TODO: increment pagesCount ONLY upon successfull fwrite !
        fseek(pFile, 0, SEEK_SET);
        fwrite(&header, sizeof(unsigned char), sizeof(LTX_Header), pFile);
    }
    fclose(pFile);
}

void LTX_Bitmap::readPageData(size_t pageNum, void *pData) const {
    if (pageNum >= mHeader.pagesCount ) {
        logError("LTX_Bitmap::readPageData pageNum exceeds pages count !!!");
        return;
    }

    auto pFile = fopen(mFilename.c_str(), "rb");
    fseek(pFile, kLtxHeaderOffset + pageNum * mHeader.pageDataSize, SEEK_SET);
    fread(pData, 1, mHeader.pageDataSize, pFile);
    fclose(pFile);
}

// This version uses previously opened file. On large scenes this saves us at least 50% time
void LTX_Bitmap::readPageData(size_t pageNum, void *pData, FILE *pFile) const {
    assert(pFile);

    if (pageNum >= mHeader.pagesCount ) {
        logError("LTX_Bitmap::readPageData pageNum exceeds pages count !!!");
        return;
    }

    fseek(pFile, kLtxHeaderOffset + pageNum * mHeader.pageDataSize, SEEK_SET);
    fread(pData, 1, mHeader.pageDataSize, pFile);
}

void LTX_Bitmap::readPagesData(std::vector<std::pair<size_t, void*>>& pages, bool unsorted) const {
    auto pFile = fopen(mFilename.c_str(), "rb");

    if (unsorted) {
        std::sort (pages.begin(), pages.end(), [](std::pair<size_t, void*> a, std::pair<size_t, void*> b) {
            return a.first < b.first;   
        });
    }

    for(auto& page: pages) {
        fseek(pFile, kLtxHeaderOffset + page.first * mHeader.pageDataSize, SEEK_SET);
    }
    
    fclose(pFile);
}

}  // namespace Falcor
