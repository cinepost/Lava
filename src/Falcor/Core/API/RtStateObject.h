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
#ifndef SRC_FALCOR_CORE_API_RTSTATEOBJECT_H_
#define SRC_FALCOR_CORE_API_RTSTATEOBJECT_H_

#include "Raytracing.h"
#include "Falcor/Core/Program/ProgramVersion.h"

namespace Falcor {

class dlldecl RtStateObject {
	public:
		using SharedPtr = std::shared_ptr<RtStateObject>;
		using SharedConstPtr = std::shared_ptr<const RtStateObject>;
		using ApiHandle = RaytracingStateHandle;
		using lala = RasterizerStateHandle;

		class dlldecl Desc {
			public:
				Desc& setKernels(const ProgramKernels::SharedConstPtr& pKernels) { mpKernels = pKernels; return *this; }
				Desc& setMaxTraceRecursionDepth(uint32_t maxDepth) { mMaxTraceRecursionDepth = maxDepth; return *this; }
				Desc& setPipelineFlags(RtPipelineFlags flags) { mPipelineFlags = flags; return *this; }

				bool operator==(const Desc& other) const;

			private:
				ProgramKernels::SharedConstPtr mpKernels;
				uint32_t mMaxTraceRecursionDepth = 0;
				RtPipelineFlags mPipelineFlags = RtPipelineFlags::None;
				friend RtStateObject;
		};

		static SharedPtr create(Device::SharedPtr pDevice, const Desc& desc);
		const ApiHandle& getApiHandle() const { return mApiHandle; }

		const ProgramKernels::SharedConstPtr& getKernels() const { return mDesc.mpKernels; };
		uint32_t getMaxTraceRecursionDepth() const { return mDesc.mMaxTraceRecursionDepth; }

		void const* getShaderIdentifier(uint32_t index) const { return mEntryPointGroupExportNames[index].c_str(); }

		const Desc& getDesc() const { return mDesc; }
	
	private:
		RtStateObject(Device::SharedPtr pDevice, const Desc& desc);
		void apiInit();

		Device::SharedPtr mpDevice = nullptr;
		Desc mDesc;
		ApiHandle mApiHandle;

		std::vector<std::string> mEntryPointGroupExportNames;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_RTSTATEOBJECT_H_