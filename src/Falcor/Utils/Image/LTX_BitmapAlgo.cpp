#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Formats.h"
#include "LTX_BitmapAlgo.h"

#include <OpenImageIO/filter.h>
#include <OpenImageIO/color.h>

#include "blosc.h"

namespace Falcor {

static int32_t gBloscForceBlocksize = 0;

static const bool kOnePageTailData = true;

static const int kDoBloscShuffle = BLOSC_NOSHUFFLE;; //BLOSC_SHUFFLE;

/*
int blosc_compress_ctx(int clevel, int doshuffle, size_t typesize,
                       size_t nbytes, const void* src, void* dest,
                       size_t destsize, const char* compressor,
                       size_t blocksize, int numinternalthreads)
*/

static uint32_t writePageData(FILE *pFile, uint32_t pageId, uint32_t pageOffset, TLCInfo& compressionInfo, const std::vector<uint8_t>& page_data, std::vector<uint8_t>* pScratchBuffer) {
	if (page_data.size() > kLtxPageSize) {
		LLOG_ERR << "Page data size exceeds maximum " << std::to_string(kLtxPageSize) << " bytes !!!";
		return 0;
	}

	size_t bytes_written = 0;
	if(compressionInfo.topLevelCompression != LTX_Header::TopLevelCompression::NONE) {
		if(!pScratchBuffer) {
			LLOG_ERR << "No scratch buffer provided for compressed page write !!!";
			return 0;
		}

		assert(pScratchBuffer);
		if(pScratchBuffer->size() < (kLtxPageSize + BLOSC_MAX_OVERHEAD)) pScratchBuffer->resize(kLtxPageSize + BLOSC_MAX_OVERHEAD);
		
		// write compressed page date
		int cbytes = blosc_compress_ctx(compressionInfo.compressionLevel, kDoBloscShuffle, compressionInfo.compressionTypeSize, 
			kLtxPageSize, page_data.data(), pScratchBuffer->data(), 
			pScratchBuffer->size(), getBloscCompressionName(compressionInfo.topLevelCompression),
			gBloscForceBlocksize, 4);

		if (cbytes == 0 ) {
			throw std::runtime_error("Compression error! Data cannot be copied without overrun destination.");
		} else if (cbytes < 0) {
			throw std::runtime_error("Compression error!");
		}

		LLOG_TRC << "Compressed page: " << pageId << " size is: " << cbytes << " offset: " << pageOffset;// << " ftell: " << ftell(pFile);

		bytes_written = fwrite(pScratchBuffer->data(), sizeof(uint8_t), cbytes, pFile);
		compressionInfo.pPageOffsets[pageId] = pageOffset;
		compressionInfo.pCompressedPageSizes[pageId] = cbytes;

	} else {
		// write uncompressed page data
		bytes_written = fwrite(page_data.data(), sizeof(uint8_t), page_data.size(), pFile);
	}
	return (uint32_t)bytes_written;
}

static uint32_t writeTailData(FILE *pFile, TLCInfo& compressionInfo, const std::vector<uint8_t>& tail_data) {
	size_t bytes_written = 0;
	if(compressionInfo.topLevelCompression != LTX_Header::TopLevelCompression::NONE) {
		std::vector<uint8_t> tmp_compressed_tail_data(tail_data.size() + BLOSC_MAX_OVERHEAD);
		// write compressed page date
		int cbytes = blosc_compress_ctx(compressionInfo.compressionLevel, kDoBloscShuffle, compressionInfo.compressionTypeSize, 
			tail_data.size(), tail_data.data(), tmp_compressed_tail_data.data(), 
			tmp_compressed_tail_data.size(), getBloscCompressionName(compressionInfo.topLevelCompression),
			gBloscForceBlocksize, 4);

		if (cbytes == 0 ) {
			throw std::runtime_error("Compression error! Data cannot be copied without overrun destination.");
		} else if (cbytes < 0) {
			throw std::runtime_error("Compression error!");
		}

		LLOG_TRC << "Compressed tail data size is: " << cbytes << " bytes.";

		bytes_written = fwrite(tmp_compressed_tail_data.data(), sizeof(uint8_t), cbytes, pFile);
		//compressionInfo.pPageOffsets[pageId] = pageOffset;
		//compressionInfo.pCompressedPageSizes[pageId] = cbytes;
	} else {
		// write uncompressed page data
		bytes_written = fwrite(tail_data.data(), sizeof(uint8_t), tail_data.size(), pFile);
	}
	return (uint32_t)bytes_written;
}

bool ltxCpuGenerateAndWriteMIPTilesHQSlow(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile, TLCInfo& compressionInfo) {
	assert(pFile);

	if(compressionInfo.topLevelCompression != LTX_Header::TopLevelCompression::NONE) {
		assert(compressionInfo.pPageOffsets);
		assert(compressionInfo.pCompressedPageSizes);
	}

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

	compressionInfo.compressionTypeSize = dstChannelBits / 8; // compressor shuffle preconditioner

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

	std::vector<uint8_t> page_data(kLtxPageSize); // tile data to be compressed and written on disk 
	std::vector<uint8_t> compressed_page_data(kLtxPageSize + BLOSC_MAX_OVERHEAD); // temporary compressed page data

	std::vector<uint8_t> zero_buff(kLtxPageSize, 0);
	std::vector<uint8_t> tiles_buffer(img_width * page_height * dstBytesPerPixel);

	uint32_t currentPageId = 0;
	uint32_t currentPageOffset = 0;

	// write mip level 0
	LLOG_TRC << "Writing mip level 0 tiles " << pagesCountX << " " << pagesCountY;
	LLOG_TRC << "Partial page dims: " << partialPageDims.x << " " << partialPageDims.y << " " << partialPageDims.z;

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
				
				unsigned char *pPageData = page_data.data();
				unsigned char *pTileData = pBufferData + tileIdxX * tileWidthStride;

				if( partialRow && partialColumn) {
					// fill page data from partial corner tile
					::memset(pPageData, 0, kLtxPageSize);
					for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
						::memcpy(pPageData, pTileData, partialTileWidthStride );
						pPageData += partialTileWidthStride;
						pTileData += bufferWidthStride;
					}
				} else if( partialRow ) {
					// fill page data from partial bottom tile
					::memset(pPageData, 0, kLtxPageSize);
					for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
						::memcpy(pPageData, pTileData, tileWidthStride );
						pPageData += tileWidthStride;
						pTileData += bufferWidthStride;
					}
				} else if( partialColumn ) { 
					// fill page data from partial column tile
					::memset(pPageData, 0, kLtxPageSize);
					for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
						::memcpy(pPageData, pTileData, partialTileWidthStride );
						pPageData += partialTileWidthStride;
						pTileData += bufferWidthStride;
					}
				} else {
					// fill page data from full tile
					for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
						::memcpy(pPageData, pTileData, tileWidthStride );
						pPageData += tileWidthStride;
						pTileData += bufferWidthStride;
					}
				}

				currentPageOffset += writePageData(pFile, currentPageId, currentPageOffset, compressionInfo, page_data, &compressed_page_data);
				currentPageId++;
			}
		}
	}

	// color space converions processors
	oiio::ColorConfig cc;
	auto processor_tolin = cc.createColorProcessor("sRGB", "linear");
	auto processor_tosrgb = cc.createColorProcessor("linear", "sRGB");

	// Write 1+ MIP tiles
	for(uint8_t mipLevel = 1; mipLevel < mipInfo.mipTailStart; mipLevel++) {
		header.mipBases[mipLevel] = currentPageId;

		uint32_t mipLevelWidth = mipInfo.mipLevelsDims[mipLevel].x;
		uint32_t mipLevelHeight = mipInfo.mipLevelsDims[mipLevel].y;
		uint32_t mipLevelDepth = mipInfo.mipLevelsDims[mipLevel].z;

		pagesCountX = mipLevelWidth / page_width;
		pagesCountY = mipLevelHeight / page_height;
		pagesCountZ = mipLevelDepth / page_depth;

		partialPageDims = {
			(mipLevelWidth % page_width == 0) ? 0 : mipLevelWidth - page_width * pagesCountX,
			(mipLevelHeight % page_height == 0) ? 0 : mipLevelHeight - page_height * pagesCountY,
			(mipLevelDepth % page_depth == 0) ? 0 : mipLevelDepth - page_depth * pagesCountZ
		};

		if( partialPageDims.x != 0) pagesCountX++;
		if( partialPageDims.y != 0) pagesCountY++;
		if( partialPageDims.z != 0) pagesCountZ++;

		pagesCount += pagesCountX * pagesCountY * pagesCountZ;

		LLOG_TRC << "Writing mip level " << std::to_string(mipLevel) << " tiles " << pagesCountX << " " << pagesCountY;
		LLOG_TRC << "Mip level width " << mipLevelWidth << " height " << mipLevelHeight;
		LLOG_TRC << "Partial page dims: " << partialPageDims.x << " " << partialPageDims.y << " " << partialPageDims.z;

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

					unsigned char *pPageData = page_data.data();
					unsigned char *pTileData = pTilesLineData + tileIdxX * tileWidthStride;
				   
					if( partialRow && partialColumn) {
						// fill page data from partial corner tile
						::memset(pPageData, 0, kLtxPageSize);
						for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
							::memcpy(pPageData, pTileData, partialTileWidthStride );
							pPageData += partialTileWidthStride;
							pTileData += bufferWidthStride;
						}
					} else if( partialRow ) {
						// fill page data from partial bottom tile
						::memset(pPageData, 0, kLtxPageSize);
						for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
							::memcpy(pPageData, pTileData, tileWidthStride );
							pPageData += tileWidthStride;
							pTileData += bufferWidthStride;
						}
					} else if( partialColumn ) {
						// fill page data from partial column tile
						::memset(pPageData, 0, kLtxPageSize);
						for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
							::memcpy(pPageData, pTileData, partialTileWidthStride );
							pPageData += partialTileWidthStride;
							pTileData += bufferWidthStride;
						}
					} else {
						// //  fill page data from full tile
						for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
							::memcpy(pPageData, pTileData, tileWidthStride );
							pPageData += tileWidthStride;
							pTileData += bufferWidthStride;
						}
					}

					currentPageOffset += writePageData(pFile, currentPageId, currentPageOffset, compressionInfo, page_data, &compressed_page_data);
					currentPageId++;
				}
			}
		}
	}

	header.pagesCount = pagesCount;

	// Write mip tail pages

	header.tailDataOffset = currentPageOffset;

	size_t tailDataSize = 0;
	
	auto *pTailData = page_data.data(); 

	for(uint8_t mipTailLevel = mipInfo.mipTailStart; mipTailLevel < mipInfo.mipLevelsCount; mipTailLevel++) {
		header.mipBases[mipTailLevel] = currentPageId;
		uint32_t mipLevelWidth = std::max(1u, mipInfo.mipLevelsDims[mipTailLevel].x);
		uint32_t mipLevelHeight = std::max(1u, mipInfo.mipLevelsDims[mipTailLevel].y);
		uint32_t mipLevelDepth = std::max(1u, mipInfo.mipLevelsDims[mipTailLevel].z);

		auto tailLevelByteSize = mipLevelWidth * mipLevelHeight * dstBytesPerPixel; 
		
		if (tailLevelByteSize > kLtxPageSize) {
			LLOG_ERR << "Mip tail level " << std::to_string(mipTailLevel) << " data size is greater than " << std::to_string(kLtxPageSize);
			continue;
		}

		oiio::ROI roi(0, mipLevelWidth, 0, mipLevelHeight, 0, 1, 0, dstChannelCount);
		
		/*
		oiio::ImageBufAlgo::resize(srcBuff, "", 0, roi).get_pixels(roi, spec.format, page_data.data(), oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);
		LLOG_TRC << "Writing tail page " << std::to_string(currentPageId) << " while " << std::to_string(header.pagesCount) << " calculated";
		
		currentPageOffset += writePageData(pFile, currentPageId, currentPageOffset, compressionInfo, page_data, &compressed_page_data);
		currentPageId++;
		header.pagesCount += 1;
		*/

		if(kOnePageTailData) {
			oiio::ImageBufAlgo::resize(srcBuff, "", 0, roi).get_pixels(roi, spec.format, pTailData + tailDataSize, oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);
		} else {
			oiio::ImageBufAlgo::resize(srcBuff, "", 0, roi).get_pixels(roi, spec.format, pTailData, oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);	
			LLOG_TRC << "Writing tail level " << std::to_string(mipTailLevel) << " page " << std::to_string(currentPageId) << " while " << std::to_string(header.pagesCount) << " calculated";
			currentPageOffset += writePageData(pFile, currentPageId, currentPageOffset, compressionInfo, page_data, &compressed_page_data);
			currentPageId++;
			header.pagesCount += 1;
		}

		tailDataSize += tailLevelByteSize;
	}

	if(kOnePageTailData) {
		LLOG_WRN << "Writing one tail data page at offset " << header.tailDataOffset;
		header.tailDataSize = writePageData(pFile, currentPageId, currentPageOffset, compressionInfo, page_data, &compressed_page_data);
		LLOG_WRN << "Written one tail data size is " << header.tailDataSize;
		header.flags |= LTX_Header::Flags::ONE_PAGE_MIP_TAIL;
	}
	
	LLOG_TRC << "Tail data size " << std::to_string(tailDataSize);

	return true;
}

bool ltxCpuGenerateAndWriteMIPTilesHQFast(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile, TLCInfo& compressionInfo) {
	return true;
}

bool ltxCpuGenerateAndWriteMIPTilesLQ(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile, TLCInfo& compressionInfo) {
	return true;
}

bool ltxCpuGenerateAndWriteMIPTilesPOT(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile, TLCInfo& compressionInfo) {
	assert(pFile);

	if(compressionInfo.topLevelCompression != LTX_Header::TopLevelCompression::NONE) {
		assert(compressionInfo.pPageOffsets);
		assert(compressionInfo.pCompressedPageSizes);
	}

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

	compressionInfo.compressionTypeSize = dstChannelBits / 8; // compressor shuffle preconditioner

	size_t srcBytesPerPixel = spec.pixel_bytes();
	size_t dstBytesPerPixel = dstChannelCount * dstChannelBits / 8;

	size_t tileWidthStride = page_width * dstBytesPerPixel;
	size_t bufferWidthStride = img_width * dstBytesPerPixel;

	uint32_t pagesCountX = img_width / page_width;
	uint32_t pagesCountY = img_height / page_height;
	uint32_t pagesCountZ = img_depth / page_depth;

	std::vector<uint8_t> page_data(kLtxPageSize);
	std::vector<uint8_t> compressed_page_data(kLtxPageSize + BLOSC_MAX_OVERHEAD); // temporary compressed page data

	std::vector<uint8_t> zero_buff(kLtxPageSize, 0);
	std::vector<uint8_t> tiles_buffer(img_width * page_height * dstBytesPerPixel);

	uint32_t currentPageId = 0;
	uint32_t currentPageOffset = 0;

	// write mip level 0
	LLOG_DBG << "Writing POT texture mip level 0 tiles " << pagesCountX << " " << pagesCountY;

	pagesCount += pagesCountX * pagesCountY * pagesCountZ;
	for(uint32_t z = 0; z < pagesCountZ; z++) {
		for(uint32_t tileIdxY = 0; tileIdxY < pagesCountY; tileIdxY++) {
			int y_begin = tileIdxY * page_height;

			uint writeLinesCount = page_height;

			oiio::ROI roi(0, img_width, y_begin, y_begin + writeLinesCount, z, z + 1, /*chans:*/ 0, dstChannelCount);
			srcBuff.get_pixels(roi, spec.format, tiles_buffer.data(), oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);

			unsigned char *pBufferData = tiles_buffer.data();

			for(uint32_t tileIdxX = 0; tileIdxX < pagesCountX; tileIdxX++) {

				unsigned char *pPageData = page_data.data();
				unsigned char *pTileData = pBufferData + tileIdxX * tileWidthStride;

				//  fill page data from tile
				for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
					::memcpy(pPageData, pTileData, tileWidthStride );
					pPageData += tileWidthStride;
					pTileData += bufferWidthStride;
				}

				currentPageOffset += writePageData(pFile, currentPageId, currentPageOffset, compressionInfo, page_data, &compressed_page_data);
				currentPageId++;
			}
		}
	}

	// color space converions processors
	oiio::ColorConfig cc;
	auto processor_tolin = cc.createColorProcessor("sRGB", "linear");
	auto processor_tosrgb = cc.createColorProcessor("linear", "sRGB");

	// Write 1+ MIP tiles
	oiio::ImageBuf prevBuff;
	oiio::ImageBuf scaledBuff;

	for(uint8_t mipLevel = 1; mipLevel < mipInfo.mipTailStart; mipLevel++) {
		header.mipBases[mipLevel] = currentPageId;
		uint32_t mipLevelWidth = mipInfo.mipLevelsDims[mipLevel].x;
		uint32_t mipLevelHeight = mipInfo.mipLevelsDims[mipLevel].y;
		uint32_t mipLevelDepth = mipInfo.mipLevelsDims[mipLevel].z;

		pagesCountX = mipLevelWidth / page_width;
		pagesCountY = mipLevelHeight / page_height;
		pagesCountZ = mipLevelDepth / page_depth;
		pagesCount += pagesCountX * pagesCountY * pagesCountZ;

		LLOG_TRC << "Writing mip level " << std::to_string(mipLevel) << " tiles " << pagesCountX << " " << pagesCountY;
		LLOG_TRC << "Mip level width " << mipLevelWidth << " height " << mipLevelHeight;

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
					
					unsigned char *pPageData = page_data.data();
					unsigned char *pTileData = pTilesLineData + tileIdxX * tileWidthStride;
				   
					//  fill page data from tile
					for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
						::memcpy(pPageData, pTileData, tileWidthStride );
						pPageData += tileWidthStride;
						pTileData += bufferWidthStride;
					}

					currentPageOffset += writePageData(pFile, currentPageId, currentPageOffset, compressionInfo, page_data, &compressed_page_data);
					currentPageId++;
				}
			}
		}
	}

	header.pagesCount = pagesCount;

	// Write mip tail pages

	header.tailDataOffset = currentPageOffset;

	size_t tailDataSize = 0;
	
	auto *pTailData = page_data.data(); 

	for(uint8_t mipTailLevel = mipInfo.mipTailStart; mipTailLevel < mipInfo.mipLevelsCount; mipTailLevel++) {
		header.mipBases[mipTailLevel] = currentPageId;
		uint32_t mipLevelWidth = std::max(1u, mipInfo.mipLevelsDims[mipTailLevel].x);
		uint32_t mipLevelHeight = std::max(1u, mipInfo.mipLevelsDims[mipTailLevel].y);
		uint32_t mipLevelDepth = std::max(1u, mipInfo.mipLevelsDims[mipTailLevel].z);

		auto tailLevelByteSize = mipLevelWidth * mipLevelHeight * dstBytesPerPixel; 
		
		if (tailLevelByteSize > kLtxPageSize) {
			LLOG_ERR << "Mip tail level " << std::to_string(mipTailLevel) << " data size is greater than " << std::to_string(kLtxPageSize);
			continue;
		}

		oiio::ROI roi(0, mipLevelWidth, 0, mipLevelHeight, 0, 1, 0, dstChannelCount);

		prevBuff.copy(scaledBuff, spec.format);
		scaledBuff = oiio::ImageBuf(oiio::ImageSpec(mipLevelWidth, mipLevelHeight, spec.nchannels, spec.format), oiio::InitializePixels::No);
		oiio::ImageBufAlgo::resample(scaledBuff, prevBuff, true, roi);

		if(kOnePageTailData) {
			scaledBuff.get_pixels(roi, spec.format, pTailData + tailDataSize, oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);
		} else {
			scaledBuff.get_pixels(roi, spec.format, pTailData, oiio::AutoStride, oiio::AutoStride, oiio::AutoStride);	
			LLOG_TRC << "Writing tail level " << std::to_string(mipTailLevel) << " page " << std::to_string(currentPageId) << " while " << std::to_string(header.pagesCount) << " calculated";
			currentPageOffset += writePageData(pFile, currentPageId, currentPageOffset, compressionInfo, page_data, &compressed_page_data);
			currentPageId++;
			header.pagesCount += 1;
		}

		tailDataSize += tailLevelByteSize;
	}

	if(kOnePageTailData) {
		LLOG_WRN << "Writing one tail data page at offset " << header.tailDataOffset;
		header.tailDataSize = writePageData(pFile, currentPageId, currentPageOffset, compressionInfo, page_data, &compressed_page_data);
		LLOG_WRN << "Written one tail data size is " << header.tailDataSize;
		header.flags |= LTX_Header::Flags::ONE_PAGE_MIP_TAIL;
	}
	
	LLOG_TRC << "Tail data size " << std::to_string(tailDataSize);

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

bool ltxCpuGenerateDebugMIPTiles(LTX_Header &header, LTX_MipInfo &mipInfo, oiio::ImageBuf &srcBuff, FILE *pFile, TLCInfo& compressionInfo) {
	assert(pFile);

	if(compressionInfo.topLevelCompression != LTX_Header::TopLevelCompression::NONE) {
		assert(compressionInfo.pPageOffsets);
		assert(compressionInfo.pCompressedPageSizes);
	}

	compressionInfo.compressionTypeSize = 1; // default compresssor shuffle preconditioner

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

	std::vector<unsigned char> page_data(kLtxPageSize);
	std::vector<unsigned char> compressed_page_data(kLtxPageSize + BLOSC_MAX_OVERHEAD); // temporary compressed page data

	std::vector<unsigned char> zero_buff(kLtxPageSize, 0);
	std::vector<unsigned char> tiles_buffer(img_width * page_height * dstBytesPerPixel);

	uint32_t currentPageId = 0;
	uint32_t currentPageOffset = 0;

	// write mip level 0
	LLOG_DBG << "Writing mip level 0 tiles " << pagesCountX << " " << pagesCountY;
	LLOG_DBG << "Partial page dims: " << partialPageDims.x << " " << partialPageDims.y << " " << partialPageDims.z;

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
				
				unsigned char *pPageData = page_data.data();
				unsigned char *pTileData = pBufferData + tileIdxX * tileWidthStride;

				if( partialRow && partialColumn) {
					// fill page data from partial corner tile
					::memset(pPageData, 0, kLtxPageSize);
					for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
						::memcpy(pPageData, pTileData, partialTileWidthStride );
						pPageData += partialTileWidthStride;
						pTileData += bufferWidthStride;
					}
				} else if( partialRow ) {
					// fill page data from partial bottom tile
					::memset(pPageData, 0, kLtxPageSize);
					for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
						::memcpy(pPageData, pTileData, tileWidthStride );
						pPageData += tileWidthStride;
						pTileData += bufferWidthStride;
					}
				} else if( partialColumn ) { 
					// fill page data from partial column tile
					::memset(pPageData, 0, kLtxPageSize);
					for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
						::memcpy(pPageData, pTileData, partialTileWidthStride );
						pPageData += partialTileWidthStride;
						pTileData += bufferWidthStride;
					}
				} else {
					// fill page data from full tile
					for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
						::memcpy(pPageData, pTileData, tileWidthStride );
						pPageData += tileWidthStride;
						pTileData += bufferWidthStride;
					}
				}

				currentPageOffset += writePageData(pFile, currentPageId, currentPageOffset, compressionInfo, page_data, &compressed_page_data);
				currentPageId++;
			}
		}
	}

		// Write 1+ MIP tiles
	for(uint8_t mipLevel = 1; mipLevel < mipInfo.mipTailStart; mipLevel++) {
		header.mipBases[mipLevel] = currentPageId;
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

		LLOG_DBG << "Writing mip level " << std::to_string(mipLevel) << " tiles " << pagesCountX << " " <<  pagesCountY;
		LLOG_DBG << "Mip level width " << mipLevelWidth << " " << mipLevelHeight;
		LLOG_DBG << "Partial page dims: " << partialPageDims.x << " " << partialPageDims.y << " " << partialPageDims.z;

		partialTileWidthStride = partialPageDims.x * dstBytesPerPixel;

		tiles_buffer.resize(mipLevelWidth * mipLevelHeight * dstBytesPerPixel);
		bufferWidthStride = mipLevelWidth * dstBytesPerPixel;

		oiio::ROI roi(0, mipLevelWidth, 0, mipLevelHeight, 0, 1, 0, dstChannelCount);
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

					unsigned char *pPageData = page_data.data();
					unsigned char *pTileData = pTilesLineData + tileIdxX * tileWidthStride;
				   
					if( partialRow && partialColumn) {
						// fill page data from partial corner tile
						::memset(pPageData, 0, kLtxPageSize);
						for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
							::memcpy(pPageData, pTileData, partialTileWidthStride );
							pPageData += partialTileWidthStride;
							pTileData += bufferWidthStride;
						}
					} else if( partialRow ) {
						// fill page data from partial bottom tile
						::memset(pPageData, 0, kLtxPageSize);
						for(uint32_t lineNum = 0; lineNum < writeLinesCount; lineNum++) {
							::memcpy(pPageData, pTileData, tileWidthStride );
							pPageData += tileWidthStride;
							pTileData += bufferWidthStride;
						}
					} else if( partialColumn ) {
						// fill page data from partial column tile
						::memset(pPageData, 0, kLtxPageSize);
						for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
							::memcpy(pPageData, pTileData, partialTileWidthStride );
							pPageData += partialTileWidthStride;
							pTileData += bufferWidthStride;
						}
					} else {
						// //  fill page data from full tile
						for(uint32_t lineNum = 0; lineNum < page_height; lineNum++) {
							::memcpy(pPageData, pTileData, tileWidthStride );
							pPageData += tileWidthStride;
							pTileData += bufferWidthStride;
						}
					}

					currentPageOffset += writePageData(pFile, currentPageId, currentPageOffset, compressionInfo, page_data, &compressed_page_data);
					currentPageId++;
				}
			}
		}
	}

	// write debug tail
	std::vector<unsigned char> tail_buff(kLtxPageSize, 255);
	fwrite(tail_buff.data(), sizeof(uint8_t), kLtxPageSize, pFile);
	pagesCount++;

	fwrite(tail_buff.data(), sizeof(uint8_t), kLtxPageSize, pFile);
	pagesCount++;

	fwrite(tail_buff.data(), sizeof(uint8_t), kLtxPageSize, pFile);
	pagesCount++;

	fwrite(tail_buff.data(), sizeof(uint8_t), kLtxPageSize, pFile);
	pagesCount++;

	fwrite(tail_buff.data(), sizeof(uint8_t), kLtxPageSize, pFile);
	pagesCount++;

	header.pagesCount = pagesCount;

	return true;
}


}  // namespace Falcor
