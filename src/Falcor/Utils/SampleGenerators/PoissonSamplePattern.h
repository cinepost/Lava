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
#ifndef SRC_FALCOR_UTILS_SAMPLEGNERATORS_POISSONSAMPLEPATTERN_H_
#define SRC_FALCOR_UTILS_SAMPLEGNERATORS_POISSONSAMPLEPATTERN_H_

#include "CPUSampleGenerator.h"
#include "Falcor/Utils/Image/Bitmap.h"

namespace Falcor {

class dlldecl PoissonSamplePattern : public CPUSampleGenerator, public inherit_shared_from_this<CPUSampleGenerator, PoissonSamplePattern> {
	public:

		enum Distribution {
			SQUARE = 1,
			DISK   = 2
			IMAGE  = 3
		}

		using SharedPtr = std::shared_ptr<PoissonSamplePattern>;
		using inherit_shared_from_this<CPUSampleGenerator, PoissonSamplePattern>::shared_from_this;
		virtual ~PoissonSamplePattern() = default;

		/** Create Halton sample pattern generator.
			\param[in] sampleCount The sample count. This must in the range 1..8 currently.
			\return New object, or throws an exception on error.
		*/
		static SharedPtr create(uint32_t sampleCount = 16, Distribution distribution = Distribution::SQUARE, Bitmap* pBitmap = nullptr);

		virtual uint32_t getSampleCount() const override { return mSampleCount; }

		virtual void reset(uint32_t startID = 0) override { mCurSample = 0; }

		virtual float2 next() override {
			return kPattern[(mCurSample++) % mSampleCount];
		}
	protected:
		PoissonSamplePattern(uint32_t sampleCount, Distribution distribution, Bitmap* pBitmap);

		uint32_t mCurSample = 0;
		uint32_t mSampleCount = 0;
		Distribution mDistribution = Distribution::SQUARE;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_SAMPLEGNERATORS_POISSONSAMPLEPATTERN_H_
