/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
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
#include "stdafx.h"
#include "Falcor/Core/API/RtAccelerationStructure.h"

#include "VKRtAccelerationStructure.h"

namespace Falcor {

template<typename T>
static inline T Max(const T& v1, const T&v2) {
	return v1>v2?v1:v2;
}

static bool getAccelerationStructurePrebuildInfo(const Device::SharedPtr pDevice, const RtAccelerationStructureBuildInputs& buildInputs, RtAccelerationStructurePrebuildInfo* outPrebuildInfo) {
	if (vkGetAccelerationStructureBuildSizesKHR) {
		LLOG_ERR << "No vkGetAccelerationStructureBuildSizesKHR";
		return false;
	}

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	AccelerationStructureBuildGeometryInfoBuilder geomInfoBuilder;
	if(SLANG_FAILED(geomInfoBuilder.build(buildInputs))) {
		LLOG_ERR << "AccelerationStructureBuildGeometryInfoBuilder build error !!!";
		return false;
	}
	
	vkGetAccelerationStructureBuildSizesKHR(
		pDevice->getApiHandle(),
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&geomInfoBuilder.buildInfo,
		geomInfoBuilder.primitiveCounts.data(), //getBuffer(),
		&sizeInfo);
	
	outPrebuildInfo->resultDataMaxSize = (uint64_t)sizeInfo.accelerationStructureSize;
	outPrebuildInfo->scratchDataSize = (uint64_t)sizeInfo.buildScratchSize;
	outPrebuildInfo->updateScratchDataSize = (uint64_t)sizeInfo.updateScratchSize;
	return true;
}

RtAccelerationStructure::RtAccelerationStructure(Device::SharedPtr pDevice, const RtAccelerationStructure::Desc& desc): mpDevice(pDevice), mDesc(desc) { }

RtAccelerationStructure::~RtAccelerationStructure() {
	mpDevice->releaseResource(mApiHandle);
}

RtAccelerationStructurePrebuildInfo RtAccelerationStructure::getPrebuildInfo(Device::SharedPtr pDevice,  const RtAccelerationStructureBuildInputs& inputs) {
	
	assert(pDevice);
	
	RtAccelerationStructurePrebuildInfo prebuildInfo;
	getAccelerationStructurePrebuildInfo(pDevice, inputs, &prebuildInfo);

	return prebuildInfo;
}

bool RtAccelerationStructure::apiInit() {
	//gfx::IAccelerationStructure::CreateDesc createDesc = {};
	//createDesc.buffer = static_cast<gfx::IBufferResource*>(mDesc.getBuffer()->getApiHandle().get());
	//createDesc.kind = mDesc.mKind;
	//createDesc.offset = mDesc.getOffset();
	//createDesc.size = mDesc.getSize();

	//SLANG_RETURN_FALSE_ON_FAIL(mpDevice->getApiHandle()->createAccelerationStructure(createDesc, mApiHandle.writeRef()));

//////////////////////////

	VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
  createInfo.buffer = mDesc.getBuffer()->getApiHandle();
  createInfo.offset = mDesc.getOffset();
  createInfo.size = mDesc.getSize();
  
  switch (mDesc.mKind) {
  	case RtAccelerationStructureKind::BottomLevel:
      createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
      break;
  	case RtAccelerationStructureKind::TopLevel:
      createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
      break;
  	default:
      LLOG_ERR << "Invalid value of RtAccelerationStructureKind encountered in mDesc.kind !!!";
      return SLANG_E_INVALID_ARG;
  }

  VkAccelerationStructureKHR vkHandle;
  if ( VK_FAILED(vkCreateAccelerationStructureKHR(pDevice->getApiHandle(), &createInfo, nullptr, &vkHandle))) {
  	LLOG_ERR << "Error creating acceleration structure !!!";
  	return false;
  }

//////////////////////////
  mApiHandle = ApiHandle::create(pDevice, vkHandle);

	return true;
}

AccelerationStructureHandle RtAccelerationStructure::getApiHandle() const {
	return mApiHandle;
}


RtQueryPool::QueryType getVKAccelerationStructurePostBuildQueryType(RtAccelerationStructurePostBuildInfoQueryType type) {
	switch (type) {
		case RtAccelerationStructurePostBuildInfoQueryType::CompactedSize:
			return RtQueryPool::QueryType::AccelerationStructureCompactedSize;
		case RtAccelerationStructurePostBuildInfoQueryType::SerializationSize:
			return RtQueryPool::QueryType::AccelerationStructureSerializedSize;
		case RtAccelerationStructurePostBuildInfoQueryType::CurrentSize:
			return RtQueryPool::QueryType::AccelerationStructureCurrentSize;
		default:
			assert(false);
			return RtQueryPool::QueryType::AccelerationStructureCompactedSize;
	}
}

SlangResult AccelerationStructureBuildGeometryInfoBuilder::build( const RtAccelerationStructureBuildInputs& buildInputs) {
	buildInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	switch (buildInputs.kind) {
		case RtAccelerationStructureKind::BottomLevel:
			buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			break;
		case RtAccelerationStructureKind::TopLevel:
			buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
			break;
		default:
			LLOG_ERR << "Invalid value of RtAccelerationStructureKind encountered in buildInputs.kind"
			return SLANG_E_INVALID_ARG;
	}

	if (buildInputs.flags & RtAccelerationStructureBuildFlags::PerformUpdate) {
		buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
	} else {
		buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	}

	if (buildInputs.flags & RtAccelerationStructureBuildFlags::AllowCompaction) {
		buildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
	}

	if (buildInputs.flags & RtAccelerationStructureBuildFlags::AllowUpdate) {
		buildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
	}

	if (buildInputs.flags & RtAccelerationStructureBuildFlags::MinimizeMemory) {
		buildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR;
	}

	if (buildInputs.flags & RtAccelerationStructureBuildFlags::PreferFastBuild) {
		buildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
	}

	if (buildInputs.flags & RtAccelerationStructureBuildFlags::PreferFastTrace) {
		buildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	}

	if (buildInputs.kind == RtAccelerationStructureKind::BottomLevel) {
		//mGeometryInfos.setCount(buildInputs.descCount);
		//primitiveCounts.setCount(buildInputs.descCount);
		mGeometryInfos.resize(buildInputs.descCount);
		primitiveCounts.resize(buildInputs.descCount);
		memset( mGeometryInfos.getBuffer(), 0, sizeof(VkAccelerationStructureGeometryKHR) * buildInputs.descCount);
		
		for (int i = 0; i < buildInputs.descCount; i++) {
			auto& geomDesc = buildInputs.geometryDescs[i];
			mGeometryInfos[i].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;

			if (geomDesc.flags & RtGeometryFlags::NoDuplicateAnyHitInvocation) {
				mGeometryInfos[i].flags |= VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
			}
			else if (geomDesc.flags & RtGeometryFlags::Opaque)
			{
				mGeometryInfos[i].flags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
			}
			auto& vkGeomData = mGeometryInfos[i].geometry;
			
			switch (geomDesc.type) {
				case RtGeometryType::Triangles:
					mGeometryInfos[i].geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
					vkGeomData.triangles.sType =
						VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
					vkGeomData.triangles.vertexFormat =
						VulkanUtil::getVkFormat(geomDesc.content.triangles.vertexFormat);
					vkGeomData.triangles.vertexData.deviceAddress =
						geomDesc.content.triangles.vertexData;
					vkGeomData.triangles.vertexStride = geomDesc.content.triangles.vertexStride;
					vkGeomData.triangles.maxVertex = geomDesc.content.triangles.vertexCount - 1;
					
					switch (geomDesc.content.triangles.indexFormat) {
						case Format::R32_UINT:
							vkGeomData.triangles.indexType = VK_INDEX_TYPE_UINT32;
							break;
						case Format::R16_UINT:
							vkGeomData.triangles.indexType = VK_INDEX_TYPE_UINT16;
							break;
						case Format::Unknown:
							vkGeomData.triangles.indexType = VK_INDEX_TYPE_NONE_KHR;
							break;
						default:
							LLOG_ERR << "Unsupported value of Format encountered in GeometryDesc::content.triangles.indexFormat !!!";
							return SLANG_E_INVALID_ARG;
					}
					
					vkGeomData.triangles.indexData.deviceAddress = geomDesc.content.triangles.indexData;
					vkGeomData.triangles.transformData.deviceAddress = geomDesc.content.triangles.transform3x4;
					primitiveCounts[i] = Max(geomDesc.content.triangles.vertexCount, geomDesc.content.triangles.indexCount) / 3;
					break;
				case RtGeometryType::ProcedurePrimitives:
					mGeometryInfos[i].geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
					vkGeomData.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
					vkGeomData.aabbs.data.deviceAddress = geomDesc.content.proceduralAABBs.data;
					vkGeomData.aabbs.stride = geomDesc.content.proceduralAABBs.stride;
					primitiveCounts[i] =
						(uint32_t)buildInputs.geometryDescs[i].content.proceduralAABBs.count;
					break;
				default:
					LLOG_ERR << "Invalid value of IAccelerationStructure::GeometryType encountered in buildInputs.geometryDescs !!!";
					return SLANG_E_INVALID_ARG;
			}
		}
		buildInfo.geometryCount = buildInputs.descCount;
		buildInfo.pGeometries = mGeometryInfos.getBuffer();
	} else {
		mVkInstanceInfo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		mVkInstanceInfo.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		mVkInstanceInfo.geometry.instances.arrayOfPointers = 0;
		mVkInstanceInfo.geometry.instances.data.deviceAddress = buildInputs.instanceDescs;
		buildInfo.pGeometries = &m_vkInstanceInfo;
		buildInfo.geometryCount = 1;
		//primitiveCounts.setCount(1);
		primitiveCounts.resize(1);
		primitiveCounts[0] = buildInputs.descCount;
	}
	return SLANG_OK;
}

}  // namespace Falcor
