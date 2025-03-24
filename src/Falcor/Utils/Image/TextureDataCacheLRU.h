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
#ifndef SRC_FALCOR_UTILS_IMAGE_TEXTUREDATACACHELRU_H_
#define SRC_FALCOR_UTILS_IMAGE_TEXTUREDATACACHELRU_H_

#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/VirtualTexturePage.h"

#include <unordered_map>


namespace Falcor {

class Device;

class FALCOR_API TextureDataCacheLRU {
	public:
		~TextureDataCacheLRU();

		/** Create a texture cache.
			\param[in] maxSystemMemoryLimit Maximum system memory in megabytes that can be used for textures cache.
			\param[in] maxDeviceMemoryLimit Maximum device memory in megabytes that can be used for textures.
			\return A new object.
		*/
		static ref<TextureDataCacheLRU> create(ref<Device> pDevice, size_t maxSystemMemoryLimit = 1024, size_t maxDeviceMemoryLimit = 512);

		void clear();


	private:
		TextureDataCacheLRU(ref<Device> pDevice, size_t maxSystemMemoryLimit, size_t maxDeviceMemoryLimit);

		ref<Device> mpDevice;

		size_t mSystemCachedDataSize = 0;
		size_t mDeviceCachedDataSize = 0;

		size_t mSystemCachedDataSizeLimit = 0;
		size_t mDeviceCachedDataSizeLimit = 0;

		std::unordered_map<ref<Texture>, std::unordered_map<uint32_t, ref<VirtualTexturePage>>> mPagesMap;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_IMAGE_TEXTUREDATACACHELRU_H_
