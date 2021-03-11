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
#ifndef SRC_FALCOR_RENDERPASSES_HBAO_HBAO_H_
#define SRC_FALCOR_RENDERPASSES_HBAO_HBAO_H_

#include "Falcor.h"
#include "FalcorExperimental.h"
#include "Utils/Sampling/SampleGenerator.h"

#include "HBAOData.slang"

using namespace Falcor;

class HBAO : public RenderPass, public inherit_shared_from_this<RenderPass, HBAO> {
 public:
    using SharedPtr = std::shared_ptr<HBAO>;
    using inherit_shared_from_this::shared_from_this;
    static const char* kDesc;

    enum class SampleDistribution : uint32_t {
        Random,
        UniformHammersley,
        CosineHammersley
    };

    static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

    std::string getDesc() override { return kDesc; }
    virtual Dictionary getScriptingDictionary() override;
    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override;
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override { mpScene = pScene; }

    void setFrameSampleCount(uint32_t samples);

    void setAoDistance(float distance);
    float getAoDistance() const { return mAoDistance; }

    void setAoTracePrecision(float precision);
    float getAoTracePrecision() const { return mAoTracePrecision; }

 private:
    HBAO(Device::SharedPtr pDevice);

    float mAoDistance;
    float mAoTracePrecision; // [0, 1] range 

    ComputePass::SharedPtr mpDownCopyDepthPass;
    ComputePass::SharedPtr mpDownSampleDepthPass;
    ComputePass::SharedPtr mpHorizonsSearchPass;

    uint2                  mFrameDim = { 0, 0 };
    uint32_t               mFrameSampleCount = 16;

    HBAOData mData;
    bool mDirty = false;

    float4 mViewVecs[2];

    Fbo::SharedPtr mpHorzonsFbo;

    Sampler::SharedPtr mpNoiseSampler;
    Texture::SharedPtr mpBlueNoiseTexture;
    Texture::SharedPtr mpNoiseTexture;
    uint2 mNoiseSize = uint2(64);

    Sampler::SharedPtr mpTextureSampler;
    Sampler::SharedPtr mpPointSampler;
    Sampler::SharedPtr mpDepthSampler;
    CPUSampleGenerator::SharedPtr      mpNoiseOffsetGenerator;      ///< Blue noise texture offsets generator. Sample in the range [-0.5, 0.5) in each dimension.

    Scene::SharedPtr mpScene;
};

#define str(a) case HBAO::SampleDistribution::a: return #a
inline std::string to_string(HBAO::SampleDistribution type) {
    switch (type) {
        str(Random);
        str(UniformHammersley);
        str(CosineHammersley);
        default:
            should_not_get_here();
            return "";
    }
}
#undef str

#endif  // SRC_FALCOR_RENDERPASSES_HBAO_HBAO_H_
