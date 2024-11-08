#include <iostream>
#include <fstream>

#include "vk-pipeline-cache-manager.h"

#include "vk-device.h"
#include "vk-helper-functions.h"

#include "Falcor/Utils/Timing/SimpleProfiler.h"

static int64_t computeDataCheckSum(const uint8_t* pData, size_t count) {
	uint64_t checksum = 0;

	for(size_t i = 0; i < count; ++count) checksum += pData[i];

	return checksum;
}

namespace gfx {

namespace vk {

PipelineCacheManager::PipelineCacheManager() {

}

void PipelineCacheManager::init(const fs::path& cachesStoragePath) { 
	PipelineCacheManager::instance()._init(cachesStoragePath); 
}

void PipelineCacheManager::_init(const fs::path& cachesStoragePath) {
	if(isInitialized()) return;

	static bool createMissingDirectory = true;

	if(!cachesStoragePath.empty() && lava::ut::fsys::isDirectoryExists(cachesStoragePath, createMissingDirectory)) {
		mCachesStoragePath = cachesStoragePath;
	
		Falcor::ThreadPool& pool = Falcor::ThreadPool::instance();
  	mInitialized = pool.submit([this]{
  		
  		static size_t headerSize = sizeof(Header);
  		std::vector<uint8_t> shaderCacheFileData;

  		static constexpr size_t cacheFileReserve = 1024 * 1024;
  		shaderCacheFileData.reserve(cacheFileReserve);

  		for (const auto & entry : fs::directory_iterator(mCachesStoragePath)) {
        std::ifstream shaderCacheFile(entry.path().string(), std::ios::in | std::ios::binary | std::ios::ate);
        if(shaderCacheFile.is_open()) {
        	std::uintmax_t shaderCacheFileSize = fs::file_size(entry.path());
        	if(shaderCacheFileSize < headerSize) continue;

        	shaderCacheFileData.resize(shaderCacheFileSize);

        	memset(shaderCacheFileData.data(), 0, headerSize);
        	shaderCacheFile.seekg(0, std::ios::beg);
        	shaderCacheFile.read((char *)shaderCacheFileData.data(), headerSize);

        	const Header* pHeader = reinterpret_cast<const Header*>(shaderCacheFileData.data());

        	if(pHeader->magic != PipelineCache::kHeaderMagic) continue;
        }
    	}

    	return mCacheMap.size() > 0;

  	});
	}
}

bool PipelineCacheManager::isInitialized() const {
	if(mInitialized.valid()) {
		return	mInitialized.get();
	}
	return false;
}

} // namespace vk
} // namespace gfx
