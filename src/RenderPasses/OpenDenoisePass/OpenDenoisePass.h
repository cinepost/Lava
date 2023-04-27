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
#ifndef SRC_FALCOR_RENDERPASSES_OPENDENOISEPASS_OPENDENOISEPASS_H_
#define SRC_FALCOR_RENDERPASSES_OPENDENOISEPASS_OPENDENOISEPASS_H_

#include "Falcor/Falcor.h"
#include "FalcorExperimental.h"
#include "Falcor/RenderGraph/RenderPass.h"

#include <OpenImageDenoise/oidn.hpp>


using namespace Falcor;

class PASS_API OpenDenoisePass : public RenderPass {
	public:
		using SharedPtr = std::shared_ptr<OpenDenoisePass>;
		using SharedConstPtr = std::shared_ptr<const OpenDenoisePass>;
		static const Info kInfo;

		static SharedPtr create(RenderContext* pRenderContext, const Dictionary& dict = {});

		virtual Dictionary getScriptingDictionary() override;
		virtual RenderPassReflection reflect(const CompileData& compileData) override;
		virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override;
		virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;

		void setOutputFormat(ResourceFormat format);
    void disableHDRInput(bool value);

  private:
		OpenDenoisePass(Device::SharedPtr pDevice, ResourceFormat outputFormat);

    // Bypasses denoising by just copying input image to output
    void bypass(RenderContext* pRenderContext, const RenderData& renderData);

		ResourceFormat mOutputFormat;       // Output format (uses default when set to ResourceFormat::Unknown).
		uint2 mFrameDim = { 0, 0 };

    bool  mDisableHDRInput = false;

		oidn::DeviceRef mIntelDevice;

		std::vector<uint8_t> mMainImageData;
		std::vector<uint8_t> mAlbedoImageData;
		std::vector<uint8_t> mNormalImageData;
		std::vector<uint8_t> mOutputImageData;
};

#endif  // SRC_FALCOR_RENDERPASSES_OPENDENOISEPASS_OPENDENOISEPASS_H_
