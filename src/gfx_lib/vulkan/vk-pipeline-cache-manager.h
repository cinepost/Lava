// vk-pipeline-cache.h
#pragma once

#include <future>
#include <mutex>

#include "vk-base.h"
#include "Falcor/Utils/ThreadPool.h"
#include "lava_utils_lib/ut_fsys.h"

#include "vk-pipeline-cache.h"


namespace gfx {

namespace vk {

class PipelineCacheManager {
	using Header = PipelineCache::Header;

  public:
  	struct PipelineCacheEntry {
  		Header header;
  	};

    static PipelineCacheManager& instance() {
        static PipelineCacheManager instance;
        return instance;
    }

    static void init(const fs::path& cachesStoragePath);

    bool isInitialized() const;

  private:
  	PipelineCacheManager();
    PipelineCacheManager(const PipelineCacheManager&) = delete;
    PipelineCacheManager& operator=(const PipelineCacheManager&) = delete;

    void _init(const fs::path& cachesStoragePath);

    std::map<std::string, PipelineCacheEntry> mCacheMap;
    fs::path mCachesStoragePath;

    std::mutex mMapMutex;
    mutable std::future<bool> mInitialized;
};


} // namespace vk
} // namespace gfx
