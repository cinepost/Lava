// vk-pipeline-cache.h
#pragma once

#include <future>

#include "vk-base.h"
#include "Falcor/Utils/ThreadPool.h"
#include "lava_utils_lib/ut_fsys.h"


namespace gfx {

using namespace Slang;

namespace vk {

class PipelineCache {
	public:	
		static const uint32_t kHeaderMagic = 1635148140;

		struct Header {
			uint32_t 	magic; // an arbitrary magic header to make sure this is actually our file
			uint32_t 	dataSize; // equal to *pDataSize returned by vkGetPipelineCacheData
			uint64_t 	dataHash; // a hash of pipeline cache data, including the header
			uint32_t 	vendorID; // equal to VkPhysicalDeviceProperties::vendorID
			uint32_t 	deviceID; // equal to VkPhysicalDeviceProperties::deviceID
			uint32_t 	driverVersion; // equal to VkPhysicalDeviceProperties::driverVersion
			uint32_t 	driverABI; // equal to sizeof(void*)
			uint8_t 	uuid[VK_UUID_SIZE]; // equal to VkPhysicalDeviceProperties::pipelineCacheUUID
		};

	public:
		static std::unique_ptr<PipelineCache> create(DeviceImpl* pDevice);

		PipelineCache(DeviceImpl* device);
		~PipelineCache();

		VkPipelineCache getPipelineCache();

	private:
		BreakableReference<DeviceImpl> m_device;
		VkPipelineCache mPipelineCache = VK_NULL_HANDLE;

		std::vector<uint8_t> mCacheData;

		Header mHeader;

		static fs::path mTempDirPath;

};

} // namespace vk
} // namespace gfx
