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
#ifndef SRC_FALCOR_RENDERPASSES_FORWARDLIGHTINGPASS_FORWARDLIGHTINGPASS_H_
#define SRC_FALCOR_RENDERPASSES_FORWARDLIGHTINGPASS_FORWARDLIGHTINGPASS_H_

#include "Falcor/Falcor.h"
#include "FalcorExperimental.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Utils/Sampling/SampleGenerator.h"
#include "Falcor/Scene/Scene.h"
#include "Experimental/Scene/Lights/EnvMapLighting.h"


using namespace Falcor;

class ForwardLightingPass : public RenderPass {
 public:
    using SharedPtr = std::shared_ptr<ForwardLightingPass>;
    static const char* kDesc;

    /** Create a new object
    */
    static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void execute(RenderContext* pContext, const RenderData& renderData) override;
    virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
    virtual Dictionary getScriptingDictionary() override;

    void setCullMode(RasterizerState::CullMode cullMode) { mCullMode = cullMode; }

    /** Set samples per frame count
    */
    void setFrameSampleCount(uint32_t samples);

    /** Set the color target format. This is always enabled
    */
    ForwardLightingPass& setColorFormat(ResourceFormat format);

    /** Set the output normal map format. Setting this to ResourceFormat::Unknown will disable this output
    */
    ForwardLightingPass& setNormalMapFormat(ResourceFormat format);

    /** Set the motion vectors map format. Setting this to ResourceFormat::Unknown will disable this output
    */
    ForwardLightingPass& setMotionVecFormat(ResourceFormat format);

    /** Set the required output supersample-count. 0 will use the swapchain sample count
    */
    ForwardLightingPass& setSuperSampleCount(uint32_t samples);

    /** Enable super-sampling in the pixel-shader
    */
    ForwardLightingPass& setSuperSampling(bool enable);

    /** If set to true, the pass requires the user to provide a pre-rendered depth-buffer
    */
    ForwardLightingPass& usePreGeneratedDepthBuffer(bool enable);

    /** Set a sampler-state to be used during rendering. The default is tri-linear
    */
    ForwardLightingPass& setSampler(const Sampler::SharedPtr& pSampler);

    /** Get a description of the pass
    */
    std::string getDesc() override { return kDesc; }

    ForwardLightingPass& setRasterizerState(const RasterizerState::SharedPtr& pRsState);

 private:
    ForwardLightingPass(Device::SharedPtr pDevice);
    
    void initDepth(RenderContext* pContext, const RenderData& renderData);
    void initFbo(RenderContext* pContext, const RenderData& renderData);

    void prepareVars(RenderContext* pContext);

    Fbo::SharedPtr                  mpFbo;
    GraphicsState::SharedPtr        mpState;
    DepthStencilState::SharedPtr    mpDsNoDepthWrite;
    Scene::SharedPtr                mpScene;
    GraphicsVars::SharedPtr         mpVars;
    RasterizerState::SharedPtr      mpRsState;
    GraphicsProgram::SharedPtr      mpProgram;
    RasterizerState::CullMode       mCullMode = RasterizerState::CullMode::Back;

    uint2 mFrameDim = { 0, 0 };
    uint32_t mFrameSampleCount = 16;
    uint32_t mSuperSampleCount = 1;  // MSAA stuff

    uint32_t mSampleNumber = 0;

    Sampler::SharedPtr                  mpNoiseSampler;
    Texture::SharedPtr                  mpBlueNoiseTexture;
    CPUSampleGenerator::SharedPtr       mpNoiseOffsetGenerator;      ///< Blue noise texture offsets generator. Sample in the range [-0.5, 0.5) in each dimension.
    SampleGenerator::SharedPtr          mpSampleGenerator;           ///< GPU sample generator.
    EnvMapLighting::SharedPtr           mpEnvMapLighting;

    ResourceFormat mColorFormat = ResourceFormat::RGBA16Float; //Default color rendering format;
    ResourceFormat mNormalMapFormat = ResourceFormat::RGBA16Float;
    ResourceFormat mMotionVecFormat = ResourceFormat::RG8Snorm;

    bool mEnableSuperSampling = false;
    bool mUsePreGenDepth = false;
    bool mUseSSAO = false;

    bool mDirty = false;
};

#endif  // SRC_FALCOR_RENDERPASSES_FORWARDLIGHTINGPASS_FORWARDLIGHTINGPASS_H_
