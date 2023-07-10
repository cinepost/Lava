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

#include "blosc.h"

#include "stdafx.h"
#include "Bitmap.h"
#include "LTX_Bitmap.h"
#include "BitmapUtils.h"
#include "LTX_BitmapAlgo.h"
#include "LTX_BitmapUtils.h"

#include "Falcor/Core/API/Texture.h"
#include "Falcor/Utils/Debug/debug.h"

#include "lava_utils_lib/logging.h"


namespace Falcor {

	using uint = uint32_t;

namespace oiio = OIIO;

static const size_t kLtxHeaderOffset = sizeof(LTX_Header);

static bool isPowerOfTwo(int x) {
	return x > 0 && !(x & (x-1));
}

static struct {
	bool operator()(size_t a, size_t b) const {   
		return a < b;
	}   
} pageSort;

LTX_Header::TopLevelCompression LTX_Bitmap::getTLCFromString(const std::string& name) {
	if(name == "lz4") return LTX_Header::TopLevelCompression::LZ4;
	if(name == "lz4hc") return LTX_Header::TopLevelCompression::LZ4HC;
	if(name == "snappy") return LTX_Header::TopLevelCompression::SNAPPY;
	if(name == "zlib") return LTX_Header::TopLevelCompression::ZLIB;
	if(name == "zstd") return LTX_Header::TopLevelCompression::ZSTD;
	if(name == "blosclz") return LTX_Header::TopLevelCompression::BLOSC_LZ;
	return LTX_Header::TopLevelCompression::NONE;
} 

const char* getBloscCompressionName(LTX_Header::TopLevelCompression tlc) {
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
			return "zstd";
		default:
			return "none";
	}
} 

static bool checkMagic(const unsigned char* magic, bool strict) {
	int match = 0;
	match += memcmp(&gLtxFileMagic[0], &magic[0], 4);
	match += memcmp(&gLtxFileMagic[7], &magic[7], 5);
	if(match != 0) return false;

	if (strict) {
		if( magic[4] != 48 + static_cast<unsigned char>(kLtxVersionMajor)) {
			LLOG_ERR << "LTX major version mismatch.";
			return false;
		}
		if( magic[5] != 48 + static_cast<unsigned char>(kLtxVersionMinor)) {
			LLOG_ERR << "LTX minor version mismatch.";
			return false;
		}
		if( magic[6] != 48 + static_cast<unsigned char>(kLtxVersionBuild)) {
			LLOG_ERR << "LTX build version mismatch.";
			return false;
		}
	}

	if(	48 <= magic[4] && magic[4] <= 57 && 
		48 <= magic[5] && magic[5] <= 57 && 
		48 <= magic[6] && magic[6] <= 57 ) return true;

	return false;
}

std::string LTX_Header::versionString() const {
	std::ostringstream stringStream;
  	stringStream << magic[4] << "." << magic[5] << "." << magic[6];
  	return stringStream.str();
}

bool LTX_Bitmap::checkFileMagic(const fs::path& path, bool strict) {
	return checkFileMagic(path.string(), strict);
}

bool LTX_Bitmap::checkFileMagic(const std::string& filename, bool strict) {
	std::ifstream file(filename, std::ios::in | std::ios::binary);
	
	unsigned char magic[12];
	file.read((char *)&magic, 12);
	file.seekg(0, file.beg);
	if(!file) {
		LLOG_ERR << "Error reading LTX header magic from file " << filename;
		file.close();
		return false;
	}

	file.close();
	return checkMagic(magic, strict);
}

static void makeMagic(unsigned char *magic) {
	static_assert((0 <= kLtxVersionMajor) && (kLtxVersionMajor <= 9));
	static_assert((0 <= kLtxVersionMinor) && (kLtxVersionMinor <= 9));
	static_assert((0 <= kLtxVersionBuild) && (kLtxVersionBuild <= 9));

	memcpy(magic, gLtxFileMagic, 12);
	magic[4] = 48 + static_cast<unsigned char>(kLtxVersionMajor);
	magic[5] = 48 + static_cast<unsigned char>(kLtxVersionMinor);
	magic[6] = 48 + static_cast<unsigned char>(kLtxVersionBuild);
}

static inline int _readUncompressedPageData(FILE * pFile, size_t dataOffset, size_t readDataSize, void *pData) {
	fseek(pFile, dataOffset, SEEK_SET);
	return fread(pData, 1, readDataSize, pFile);
} 

static inline int _readCompressedPageData(FILE * pFile, size_t dataOffset, size_t readDataSize, void *pData, uint8_t *pScratchBuffer) {
	assert(pScratchBuffer);
	fseek(pFile, dataOffset, SEEK_SET);
	fread(pScratchBuffer, 1, readDataSize, pFile);
	return blosc_decompress_ctx(pScratchBuffer, pData, kLtxPageSize, 1);
}

LTX_Bitmap::SharedConstPtr LTX_Bitmap::createFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool isTopDown) {
	return createFromFile(pDevice, fs::path(filename), isTopDown);
}

LTX_Bitmap::SharedConstPtr LTX_Bitmap::createFromFile(std::shared_ptr<Device> pDevice, const fs::path& path, bool isTopDown) {
	if(!checkFileMagic(path, true)) {
		LLOG_ERR << "Wrong LTX texture file magic!";
		return nullptr;
	}

	auto pLtxBitmap = new LTX_Bitmap();
	pLtxBitmap->mFilePath = path;
	
	auto pFile = fopen(path.string().c_str(), "rb");
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
				//case BASETYPE::UCHAR:
				case BASETYPE::UINT8:
					return ResourceFormat::R8Unorm;
				case BASETYPE::INT8:
					return ResourceFormat::R8Snorm;
				case BASETYPE::UINT16:
					return ResourceFormat::R16Unorm;
				case BASETYPE::INT16:
					return ResourceFormat::R16Snorm;
				case BASETYPE::UINT32:
					return ResourceFormat::R32Uint;
				case BASETYPE::INT32:
					return ResourceFormat::R32Int;
				case BASETYPE::HALF:
					return ResourceFormat::R16Float;
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
				case BASETYPE::INT8:
					return ResourceFormat::RG8Snorm;
				case BASETYPE::UINT16:
					return ResourceFormat::RG16Uint;
				case BASETYPE::INT16:
					return ResourceFormat::RG16Snorm;
				case BASETYPE::UINT32:
					return ResourceFormat::RG32Uint;
				case BASETYPE::INT32:
					return ResourceFormat::RG32Int;
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
				case BASETYPE::INT8:
					return ResourceFormat::RGB8Snorm;
				case BASETYPE::UINT16:
					return ResourceFormat::RGB16Unorm;
				case BASETYPE::INT16:
					return ResourceFormat::RGB16Snorm;
				case BASETYPE::UINT32:
					return ResourceFormat::RGB32Uint;
				case BASETYPE::INT32:
					return ResourceFormat::RGB32Int;
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
				case BASETYPE::INT8:
					return ResourceFormat::RGBA8Snorm;
				case BASETYPE::UINT16:
					return ResourceFormat::RGBA16Uint;
				case BASETYPE::INT16:
					return ResourceFormat::RGBA16Snorm;
				case BASETYPE::UINT32:
					return ResourceFormat::RGBA32Uint;
				case BASETYPE::INT32:
					return ResourceFormat::RGBA32Int;
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
		case ResourceFormat::RGB8Snorm:
			return ResourceFormat::RGBA8Snorm;  // this should force 24bit to 32bit conversion
		case ResourceFormat::RGB16Uint:
			return ResourceFormat::RGBA16Uint;  // this should force 48bit to 64bit conversion
		case ResourceFormat::RGB16Int:
			return ResourceFormat::RGBA16Int;  // this should force 48bit to 64bit conversion
		case ResourceFormat::RGB32Uint:
			return ResourceFormat::RGBA32Uint;  // this should force 96bit to 128bit conversion
		case ResourceFormat::RGB32Int:
			return ResourceFormat::RGBA32Int;  // this should force 96bit to 128bit conversion
		case ResourceFormat::RGB16Unorm:
			return ResourceFormat::RGBA16Unorm; // this should force 48bit to 64bit conversion
		case ResourceFormat::RGB16Snorm:
			return ResourceFormat::RGBA16Snorm; // this should force 48bit to 64bit conversion
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

	uint32_t bytesPerPixel = getFormatBytesPerBlock(format);

	info.mipLevelsCount = Texture::getMaxMipCount(imgDims);
	info.mipTailStart = info.mipLevelsCount - 1; // Default is last mip level
	info.pageDims = getPageDims(format);
	info.mipLevelsDims = std::vector<uint3>(info.mipLevelsCount);

	// Pre calculate image dimensions for each mip level (mipLevelDims)
	for( uint mipLevel = 0; mipLevel < info.mipLevelsCount; mipLevel++) {
		info.mipLevelsDims[mipLevel].x = std::max(uint32_t(imgDims.x / pow(2, mipLevel)), 1u);
		info.mipLevelsDims[mipLevel].y = std::max(uint32_t(imgDims.y / pow(2, mipLevel)), 1u);
		info.mipLevelsDims[mipLevel].z = 1u;//std::max(imgDims.z / pow(2, mipLevel), 1);
	}

	// Find mip tail starting mip level. This and all smaller layers (higher indices) combined memory footprint should be equal or less than kLtxPageSize 
	uint8_t mipTailStart = info.mipLevelsCount - 1;
	for( uint8_t currentMipLevel = 0; currentMipLevel < info.mipLevelsCount; ++currentMipLevel) {
		// Find cumulative mip levels footprint
		uint32_t currentMemCumulativeFootprint = 0;
		for(uint8_t i = currentMipLevel; i < info.mipLevelsCount; ++i) {
			auto const& mipDims = info.mipLevelsDims[i];
			currentMemCumulativeFootprint += mipDims.x * mipDims.y * bytesPerPixel;
		}

		if (currentMemCumulativeFootprint <= kLtxPageSize) {
			// Mip tail start level found
			info.mipTailStart = currentMipLevel;
			return info;
		}
	}

	return info;
}

bool LTX_Bitmap::convertToLtxFile(std::shared_ptr<Device> pDevice, const std::string& srcFilename, const std::string& dstFilename, const TLCParms& compParms, bool isTopDown) {
	auto in = oiio::ImageInput::open(srcFilename);
	if (!in) {
		LLOG_ERR << "Error reading image file: " << srcFilename;
		return false;
	}
	const oiio::ImageSpec &spec = in->spec();

	LLOG_INF << "Converting texture \"" << srcFilename << "\" to LTX format using " << to_string(getTLCFromString(compParms.compressorName)) << " compressor.";

	LLOG_DBG << "OIIO channel formats size: " << std::to_string(spec.channelformats.size());
	LLOG_DBG << "OIIO is signed: " << (spec.format.is_signed() ? "YES" : "NO");
	LLOG_DBG << "OIIO is float: " << (spec.format.is_floating_point() ? "YES" : "NO");
	LLOG_DBG << "OIIO basetype: " << std::to_string(oiio::TypeDesc::BASETYPE(spec.format.basetype));
	LLOG_DBG << "OIIO bytes per pixel: " << std::to_string(spec.pixel_bytes());
	LLOG_DBG << "OIIO nchannels: " << std::to_string(spec.nchannels);
	
	auto srcFormat = getFormatOIIO(spec.format.basetype, spec.nchannels);
	LLOG_DBG << "Source ResourceFormat from OIIO: " << to_string(srcFormat);
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
	LLOG_DBG << "Texture " << srcFilename << " mip tail starts at level " << std::to_string(mipInfo.mipTailStart);

	// make header
	LTX_Header header;
	makeMagic(&header.magic[0]);

	header.srcLastWriteTime = fs::last_write_time(srcFilename);
	header.width = dstDims.x;
	header.height = dstDims.y;
	header.depth = dstDims.z;
	header.pageDims = {static_cast<uint16_t>(mipInfo.pageDims.x), static_cast<uint16_t>(mipInfo.pageDims.y), static_cast<uint16_t>(mipInfo.pageDims.z)};
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

	LLOG_DBG << "LTX calc pages count is: " << std::to_string(header.pagesCount);

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
			LLOG_ERR << "Error setting Blosc compressor type to " << to_string(header.topLevelCompression);
			fclose(pFile);
			return false;
		}

		LLOG_DBG << "LTX Tlc: " << compname << " clevel: " << std::to_string(compressionInfo.compressionLevel);

		// write initial values. these valuse are going to rewriten after all pages compressed and written to disk
		fwrite(pageOffsets.data(), sizeof(uint32_t), header.pagesCount, pFile);
		fwrite(compressedPageSizes.data(), sizeof(uint16_t), header.pagesCount, pFile);
	}

	LLOG_DBG << "LTX Mip page dims " << mipInfo.pageDims.x << " " <<  mipInfo.pageDims.y << " " << mipInfo.pageDims.z;

	bool result = false;

	if( 1 == 1) {
		if( isPowerOfTwo(srcDims.x) && isPowerOfTwo(srcDims.y) ) {
			if(ltxCpuGenerateAndWriteMIPTilesPOT(header, mipInfo, srcBuff, pFile, compressionInfo)) {
				// re-write header as it might get modified ... 
				// TODO: increment pagesCount ONLY upon successfull fwrite !
				fseek(pFile, 0, SEEK_SET);
				fwrite(&header, sizeof(uint8_t), sizeof(LTX_Header), pFile);
				result = true;
			}
		} else {
			if(ltxCpuGenerateAndWriteMIPTilesHQSlow(header, mipInfo, srcBuff, pFile, compressionInfo)) {
				// re-write header as it might get modified ... 
				// TODO: increment pagesCount ONLY upon successfull fwrite !
				fseek(pFile, 0, SEEK_SET);
				fwrite(&header, sizeof(uint8_t), sizeof(LTX_Header), pFile);
				result = true;
			}
		}
	} else {
		// debug tiles texture
		if(ltxCpuGenerateDebugMIPTiles(header, mipInfo, srcBuff, pFile, compressionInfo)) {
			fseek(pFile, 0, SEEK_SET);
			fwrite(&header, sizeof(uint8_t), sizeof(LTX_Header), pFile);
			result = true;
		}
	}

	if( header.topLevelCompression != LTX_Header::TopLevelCompression::NONE) {
		// write updated compressed blocks table balesues. these valuse are going to be rewriten after all pages compressed and written to disk
		fseek(pFile, kLtxHeaderOffset, SEEK_SET);
		fwrite(pageOffsets.data(), sizeof(uint32_t), header.pagesCount, pFile);
		fwrite(compressedPageSizes.data(), sizeof(uint16_t), header.pagesCount, pFile);
	}
	
	LLOG_DBG << "LTX post write pages count is: " << std::to_string(header.pagesCount);

	fclose(pFile);
	return result;
}

bool LTX_Bitmap::readPageData(size_t pageNum, void *pData) const {
	bool result = false;
	auto pFile = fopen(mFilePath.string().c_str(), "rb");
	std::array<uint8_t, kLtxPageSize> scratchBuffer;
	result = readPageData(pageNum, pData, pFile, scratchBuffer.data());
	fclose(pFile);
	return result;
}

// This version uses previously opened file. On large scenes this saves us approx. 50% of texture pages data loading time
bool LTX_Bitmap::readPageData(size_t pageNum, void *pData, FILE *pFile, uint8_t *pScratchBuffer) const {
	assert(pFile);
	assert(pData);

	if (pageNum >= mHeader.pagesCount ) {
		LLOG_ERR << "LTX_Bitmap::readPageData page " << std::to_string(pageNum) << " exceeds pages count " << std::to_string(mHeader.pagesCount) << " !!!";
		return false;
	}

	if (mTopLevelCompression == LTX_Header::TopLevelCompression::NONE) {
		// read uncompressed page data
		size_t page_data_offset = mHeader.dataOffset + pageNum * mHeader.pageDataSize;
		if(_readUncompressedPageData(pFile, page_data_offset, kLtxPageSize, pData) != kLtxPageSize) {
			LLOG_ERR << "Error reading texture page " << std::to_string(pageNum) << " data!";
			return false;
		}
	} else {
		// read compressed page data
		size_t page_data_offset = mHeader.dataOffset + mCompressedPageDataOffset[pageNum];
		const int nbytes = _readCompressedPageData(pFile, page_data_offset, mCompressedPageDataSize[pageNum], pData, pScratchBuffer);
		if(nbytes < 0) {
			LLOG_ERR << "Error decompressing page " << std::to_string(pageNum) << "!";
			return false;
		} else if( nbytes > kLtxPageSize) {
			LLOG_ERR << "Error decompressing page " << std::to_string(pageNum) << "! " << std::to_string(nbytes) << " bytes decompressed !!!";
			return false;
		} else if (nbytes == 0) {
			LLOG_ERR << "Error reading texture page " << std::to_string(pageNum) << " data! Source buffer is empty!";
			return false;
		}
		LLOG_TRC << "Compressed page (read) " << std::to_string(pageNum) << " size is " << std::to_string(mCompressedPageDataSize[pageNum]) 
				 << " offset " <<std::to_string(page_data_offset) << " decomp size: " << std::to_string(nbytes);
	}
	return true;
}

void LTX_Bitmap::readTailData(std::vector<uint8_t>& data) const {
	auto pFile = fopen(mFilePath.string().c_str(), "rb");
	std::array<uint8_t, kLtxPageSize> scratchBuffer;
	readTailData(pFile, data, scratchBuffer.data());
	fclose(pFile);
}

void LTX_Bitmap::readTailData(FILE *pFile, std::vector<uint8_t>& data, uint8_t *pScratchBuffer) const {
	if(mHeader.mipTailStart >= 16) {
		LLOG_ERR << "Mip tail start is " << mHeader.mipTailStart << " for LTX bitmap " << mFilePath.string(); 
		return;
	}

	std::array<uint8_t, kLtxPageSize> pageDataBuffer; 

	if(is_set(mHeader.flags, LTX_Header::Flags::ONE_PAGE_MIP_TAIL)) {
		// All tail data stored in one page
		LLOG_TRC << "Reading tail data for LTX_Bitmap " << mFilePath.string();
		
		if(data.size() < kLtxPageSize)data.resize(kLtxPageSize);
		int nbytes = 0;
		if(mTopLevelCompression == LTX_Header::TopLevelCompression::NONE) {
			nbytes = _readUncompressedPageData(pFile, mHeader.dataOffset + mHeader.tailDataOffset, kLtxPageSize, data.data());
		} else {
			nbytes = _readCompressedPageData(pFile, mHeader.dataOffset + mHeader.tailDataOffset, mHeader.tailDataSize, data.data(), pScratchBuffer);
		}

		if( nbytes != kLtxPageSize) {
			LLOG_ERR << "Error decompressing tail data !";
			data.clear();
		}
	} else {
		data.clear();
		data.reserve(kLtxPageSize);
		// Tail data stores as one mip level per page
		uint32_t tailStartPageNum = mHeader.mipBases[mHeader.mipTailStart];
		LLOG_TRC << "Reading tail data for " << mFilePath.string() << " starting from page " << tailStartPageNum << " total pages count is " <<  mHeader.pagesCount;
		for(uint32_t pageNum = tailStartPageNum; pageNum < mHeader.pagesCount; ++pageNum) {
			if(!readPageData(pageNum, pageDataBuffer.data(), pFile, pScratchBuffer)) {
				LLOG_ERR << "Error reading tail data page " << std::to_string(pageNum) << " for texture " << mFilePath.string();
				return;
			}
		}
		data.insert(data.end(), pageDataBuffer.begin(), pageDataBuffer.end());
	}
}

const std::string to_string(LTX_Header::TopLevelCompression type) {
	#define type_2_string(a) case LTX_Header::TopLevelCompression::a: return #a;
	switch (type) {
			type_2_string(NONE);
			type_2_string(BLOSC_LZ);
			type_2_string(LZ4);
			type_2_string(LZ4HC);
			type_2_string(SNAPPY);
			type_2_string(ZLIB);
			type_2_string(ZSTD);
		default:
			should_not_get_here();
			return "";
	}
#undef type_2_string
}

}  // namespace Falcor
