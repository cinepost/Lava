// vk-pipeline-cache.cpp
#include "vk-pipeline-state.h"

#include "vk-device.h"
#include "vk-helper-functions.h"

#include "Falcor/Utils/Timing/SimpleProfiler.h"


namespace gfx {

using namespace Slang;

namespace vk {


std::unique_ptr<PipelineCache> PipelineCache::create(DeviceImpl* pDevice) {
	return std::make_unique<PipelineCache>(pDevice);
}

PipelineCache::PipelineCache(DeviceImpl* pDevice) {
	LLOG_WRN << "PipelineCache::PipelineCache() constructor";
	m_device.setWeakReference(pDevice);

	/* Add initial pipeline cache data from the cached file */
	VkPipelineCacheCreateInfo create_info = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
	create_info.initialDataSize = 0;
	create_info.pInitialData    = nullptr;

	/* Create Vulkan pipeline cache */
	SLANG_VK_CHECK(m_device->m_api.vkCreatePipelineCache(m_device->m_device, &create_info, nullptr, &mPipelineCache));
}

PipelineCache::~PipelineCache() {
	LLOG_WRN << "PipelineCache::~PipelineCache() destructor";

	VkPipelineCache pipeline_cache = getPipelineCache();
	if (pipeline_cache != VK_NULL_HANDLE) {
		/* Get size of pipeline cache */
		size_t size = 0;
		SLANG_VK_CHECK(m_device->m_api.vkGetPipelineCacheData(m_device->m_device, pipeline_cache, &size, nullptr));

		/* Get data of pipeline cache */
		std::vector<uint8_t> data(size);
		SLANG_VK_CHECK(m_device->m_api.vkGetPipelineCacheData(m_device->m_device, pipeline_cache, &size, data.data()));

		/* Write pipeline cache data to a file in binary format */
		//vkb::fs::write_temp(data, "pipeline_cache.data");
		auto pipelineCacheFilePath = mTempDirPath / "pipeline_cache.data";
		lava::ut::fsys::writeBinaryFile(pipelineCacheFilePath, data.data(), data.size());

		/* Destroy Vulkan pipeline cache */
		m_device->m_api.vkDestroyPipelineCache(m_device->m_device, pipeline_cache, nullptr);
	}

	//vkb::fs::write_temp(device->get_resource_cache().serialize(), "cache.data");
}

VkPipelineCache PipelineCache::getPipelineCache() {
	return mPipelineCache;
}

} // namespace vk
} // namespace gfx

fs::path gfx::vk::PipelineCache::mTempDirPath = lava::ut::fsys::getTempDirPath("pipeline_cache", true);