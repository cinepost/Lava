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

#include "c-blosc2/blosc/blosc2.h"

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
static const size_t kLtxPageSize = 65536;

static bool isPowerOfTwo(int x) {
    return x > 0 && !(x & (x-1));
}

static struct {
    bool operator()(size_t a, size_t b) const {   
        return a < b;
    }   
} pageSort;

static LTX_Header::TopLevelCompression getTLCFromString(const std::string & name) {
    if(name == "lz4") return LTX_Header::TopLevelCompression::LZ4;
    if(name == "lz4hc") return LTX_Header::TopLevelCompression::LZ4HC;
    if(name == "snappy") return LTX_Header::TopLevelCompression::SNAPPY;
    if(name == "zlib") return LTX_Header::TopLevelCompression::ZLIB;
    if(name == "ztsd") return LTX_Header::TopLevelCompression::ZSTD;
    if(name == "blosclz") return LTX_Header::TopLevelCompression::BLOSC_LZ;
    return LTX_Header::TopLevelCompression::NONE;
} 

static const char* getBloscCompressionName(LTX_Header::TopLevelCompression tlc) {
    switch(tlc) {
        case LTX_Header::TopLevelCompression::BLOSC_LZ:
            return "blosclz";
        case LTX_Header::TopLevelCompression::LZ4:
            return "lz4";
        case LTX_Header::TopLevelCompression::LZ4HC:
            return "lz4hc";
        case LTX_Header::TopLevelCompression::SNAPPY:
            return "snappy";
        case LTX_Header::TopLevelCompression::ZLIB:
            return "zlib";
        case LTX_Header::TopLevelCompression::ZSTD:
            return "ztsd";
        default:
            return "none";
    }
} 

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

    pLtxBitmap->mTopLevelCompression = pLtxBitmap->mHeader.topLevelCompression;

    if( pLtxBitmap->mTopLevelCompression != LTX_Header::TopLevelCompression::NONE ) {
        pLtxBitmap->mCompressedPageDataOffset.resize(pLtxBitmap->mHeader.pagesCount);
        pLtxBitmap->mCompressedPageDataSize.resize(pLtxBitmap->mHeader.pagesCount);

        fread(pLtxBitmap->mCompressedPageDataOffset.data(), sizeof(uint32_t), pLtxBitmap->mHeader.pagesCount, pFile );
        fread(pLtxBitmap->mCompressedPageDataSize.data(), sizeof(uint16_t), pLtxBitmap->mHeader.pagesCount, pFile );
    }

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
                case BASETYPE::UINT16:
                    return ResourceFormat::R16Unorm;
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
                case BASETYPE::UINT16:
                    return ResourceFormat::RG16Uint;
                case BASETYPE::UINT32:
                    return ResourceFormat::RG32Uint;
                case BASETYPE::HALF:
                    return ResourceFormat::RG16Float;
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
                case BASETYPE::UINT16:
                    return ResourceFormat::RGB16Unorm;
                case BASETYPE::UINT32:
                    return ResourceFormat::RGB32Uint;
                case BASETYPE::HALF:
                    return ResourceFormat::RGB16Float;
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
                case BASETYPE::UINT16:
                    return ResourceFormat::RGBA16Uint;
                case BASETYPE::UINT32:
                    return ResourceFormat::RGBA32Uint;
                case BASETYPE::HALF:
                    return ResourceFormat::RGBA16Float;
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
        case ResourceFormat::RGB16Unorm:
            return ResourceFormat::RGBA16Unorm; // this should force 48bit to 64bit conversion
        case ResourceFormat::RGB16Float:
            return ResourceFormat::RGBA16Float; // this should force 48bit to 64bit float conversion
        case ResourceFormat::RGB32Float:
            return ResourceFormat::RGBA32Float; // this should force 96bit to 128bit float conversion
        default:
            break;
    }

    return format;
}

static uint3 getPageDims(const ResourceFormat &format) {
    uint32_t channelCount = getFormatChannelCount(format);
    uint32_t totalBits = 0u;

    for(uint i = 0; i < channelCount; i++) totalBits += getNumChannelBits(format, i);

    switch(totalBits) {
        case 8:
            return {256u, 256u, 1u};
        case 16:
            return {256u, 128u, 1u};
        case 32:
            return {128u, 128u, 1u};
        case 64:
            return {128u, 64u, 1u};
        case 128:
            return {64u, 64u, 1u};
        default:
            should_not_get_here();
            break;
    } 

    return {0u, 0u, 0u};
}

static LTX_MipInfo calcMipInfo(const uint3& imgDims, const ResourceFormat &format) {
    LTX_MipInfo info;

    info.mipLevelsCount = Texture::getMaxMipCount(imgDims);
    info.mipTailStart = info.mipLevelsCount;
    info.pageDims = getPageDims(format);
    info.mipLevelsDims = std::vector<uint3>(info.mipLevelsCount);

    // pre calculate image dimensions for each mip level (mipLevelDims)
    for( uint mipLevel = 0; mipLevel < info.mipLevelsCount; mipLevel++) {
        info.mipLevelsDims[mipLevel].x = std::max(uint32_t(imgDims.x / pow(2, mipLevel)), 1u);
        info.mipLevelsDims[mipLevel].y = std::max(uint32_t(imgDims.y / pow(2, mipLevel)), 1u);
        info.mipLevelsDims[mipLevel].z = 1u;//std::max(imgDims.z / pow(2, mipLevel), 1);
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

void LTX_Bitmap::convertToKtxFile(std::shared_ptr<Device> pDevice, const std::string& srcFilename, const std::string& dstFilename, bool isTopDown, const TLCParms& compParms) {
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
    header.pageDataSize = kLtxPageSize;
    header.pagesCount = 0;
    header.arrayLayersCount = 1;
    header.mipLevelsCount = mipInfo.mipLevelsCount;
    header.mipTailStart   = mipInfo.mipTailStart;
    header.format = dstFormat;
    header.dataOffset = sizeof(LTX_Header);

    for( auto const& mipSize: mipInfo.mipLevelsDims) {
        uint32_t pagesCountX = std::max((uint32_t)std::ceil((float)mipSize[0] / (float)mipInfo.pageDims.x), 1u);
        uint32_t pagesCountY = std::max((uint32_t)std::ceil((float)mipSize[1] / (float)mipInfo.pageDims.y), 1u);
        uint32_t pagesCountZ = std::max((uint32_t)std::ceil((float)mipSize[2] / (float)mipInfo.pageDims.z), 1u);

        header.pagesCount += pagesCountX * pagesCountY * pagesCountZ;
    }

    LOG_WARN("LTX calc pages count is: %u", header.pagesCount);

    // test
    header.topLevelCompression = getTLCFromString(compParms.compressorName);//BLOSC_LZ;
    header.topLevelCompressionLevel = compParms.compressionLevel;

    if( header.topLevelCompression != LTX_Header::TopLevelCompression::NONE) {
        // if top level compression used, data begins after two additional tables 
        header.dataOffset += (sizeof(uint32_t) + sizeof(uint16_t)) * header.pagesCount; 
    }

    // open file and write header
    FILE *pFile = fopen(dstFilename.c_str(), "wb");
    fwrite(&header, sizeof(unsigned char), sizeof(LTX_Header), pFile);

    // if some compression being used we have to write additional tables before actual pages data begins.
    // first table is individual page offset values
    // second table is individual compressed page size values

    TLCInfo compressionInfo;

    std::vector<uint32_t> pageOffsets;
    std::vector<uint16_t> compressedPageSizes;

    if( header.topLevelCompression != LTX_Header::TopLevelCompression::NONE) {
        pageOffsets.resize(header.pagesCount);
        compressedPageSizes.resize(header.pagesCount);

        compressionInfo.topLevelCompression = header.topLevelCompression;
        compressionInfo.compressionLevel = header.topLevelCompressionLevel;
        compressionInfo.pPageOffsets = pageOffsets.data();
        compressionInfo.pCompressedPageSizes = compressedPageSizes.data();

        const char *compname = getBloscCompressionName(header.topLevelCompression);
        if(blosc_set_compressor(compname) == -1) {
            LOG_ERR("Error setting Blosc compressor type to %s", compname);
            fclose(pFile);
            return;
        }

        LOG_WARN("LTX Tlc: %s clevel: %u", compname, compressionInfo.compressionLevel);

        // write initial values. these valuse are going to rewriten after all pages compressed and written to disk
        fwrite(pageOffsets.data(), sizeof(uint32_t), header.pagesCount, pFile);
        fwrite(compressedPageSizes.data(), sizeof(uint16_t), header.pagesCount, pFile);
    }

    LOG_WARN("LTX Mip page dims %u %u %u ...", mipInfo.pageDims.x, mipInfo.pageDims.y, mipInfo.pageDims.z);



if( 1 == 1) {
    if( isPowerOfTwo(srcDims.x) && isPowerOfTwo(srcDims.y) ) {
        if(ltxCpuGenerateAndWriteMIPTilesPOT(header, mipInfo, srcBuff, pFile, compressionInfo)) {
            // re-write header as it might get modified ... 
            // TODO: increment pagesCount ONLY upon successfull fwrite !
            fseek(pFile, 0, SEEK_SET);
            fwrite(&header, sizeof(unsigned char), sizeof(LTX_Header), pFile);
        }
    } else {
        if(ltxCpuGenerateAndWriteMIPTilesHQSlow(header, mipInfo, srcBuff, pFile, compressionInfo)) {
            // re-write header as it might get modified ... 
            // TODO: increment pagesCount ONLY upon successfull fwrite !
            fseek(pFile, 0, SEEK_SET);
            fwrite(&header, sizeof(unsigned char), sizeof(LTX_Header), pFile);
        }
    }
} else {
    // debug tiles texture
    ltxCpuGenerateDebugMIPTiles(header, mipInfo, srcBuff, pFile, compressionInfo);
    fseek(pFile, 0, SEEK_SET);
    fwrite(&header, sizeof(unsigned char), sizeof(LTX_Header), pFile);
}

    if( header.topLevelCompression != LTX_Header::TopLevelCompression::NONE) {
        // write updated compressed blocks table balesues. these valuse are going to rewriten after all pages compressed and written to disk
        fseek(pFile, kLtxHeaderOffset, SEEK_SET);
        fwrite(pageOffsets.data(), sizeof(uint32_t), header.pagesCount, pFile);
        fwrite(compressedPageSizes.data(), sizeof(uint16_t), header.pagesCount, pFile);
    }
    
    LOG_WARN("LTX post write pages count is: %u", header.pagesCount);

    fclose(pFile);
}

void LTX_Bitmap::readPageData(size_t pageNum, void *pData) const {
    if (pageNum >= mHeader.pagesCount ) {
        logError("LTX_Bitmap::readPageData pageNum exceeds pages count !!!");
        return;
    }

    auto pFile = fopen(mFilename.c_str(), "rb");
    readPageData(pageNum, pData, pFile);
    fclose(pFile);
}

// This version uses previously opened file. On large scenes this saves us at least 50% time
void LTX_Bitmap::readPageData(size_t pageNum, void *pData, FILE *pFile) const {
    assert(pFile);

    if (pageNum >= mHeader.pagesCount ) {
        logError("LTX_Bitmap::readPageData pageNum exceeds pages count !!!");
        return;
    }

    if (mTopLevelCompression == LTX_Header::TopLevelCompression::NONE) {
        // read uncompressed page data
        fseek(pFile, mHeader.dataOffset + pageNum * mHeader.pageDataSize, SEEK_SET);
        fread(pData, 1, mHeader.pageDataSize, pFile);
    } else {
        // read compressed page fata
        std::vector<unsigned char> tmp(65536);

        size_t page_data_offset = mHeader.dataOffset + mCompressedPageDataOffset[pageNum];

        fseek(pFile, page_data_offset, SEEK_SET);
        fread(tmp.data(), 1, mCompressedPageDataSize[pageNum], pFile);

        auto nbytes = blosc_decompress(tmp.data(), pData, 65536);
        LOG_WARN("Compressed page (read) %zu size is %u offset %u decomp size %u", pageNum, mCompressedPageDataSize[pageNum], page_data_offset, nbytes);
    }

}

void LTX_Bitmap::readPagesData(std::vector<std::pair<size_t, void*>>& pages, bool unsorted) const {
    auto pFile = fopen(mFilename.c_str(), "rb");

    if (unsorted) {
        std::sort (pages.begin(), pages.end(), [](std::pair<size_t, void*> a, std::pair<size_t, void*> b) {
            return a.first < b.first;   
        });
    }

    for(auto& page: pages) {
        fseek(pFile, mHeader.dataOffset + page.first * mHeader.pageDataSize, SEEK_SET);
    }
    
    fclose(pFile);
}

}  // namespace Falcor
