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
#ifndef SRC_FALCOR_RENDERPASSES_ACCUMULATEPASS_ACCUMULATEPASS_H_
#define SRC_FALCOR_RENDERPASSES_ACCUMULATEPASS_ACCUMULATEPASS_H_

#include "Falcor/Falcor.h"
#include "Falcor/Core/API/Sampler.h"
#include "Falcor/Scene/Scene.h"
#include "Falcor/RenderGraph/RenderPass.h"

using namespace Falcor;

/** Temporal accumulation render pass.

    This pass takes a texture as input and writes the temporally accumulated
    result to an output texture. The pass keeps intermediate data internally.

    For accumulating many samples for ground truth rendering etc., fp32 precision
    is not always sufficient. The pass supports higher precision modes using
    either error compensation (Kahan summation) or double precision math.
*/
class AccumulatePass : public RenderPass {
 public:
    using SharedPtr = std::shared_ptr<AccumulatePass>;
    using SharedConstPtr = std::shared_ptr<const AccumulatePass>;

    enum class Precision : uint32_t {
        Double,                 ///< Standard summation in double precision.
        Single,                 ///< Standard summation in single precision.
        SingleCompensated,      ///< Compensated summation (Kahan summation) in single precision.
    };

    enum class PixelFilterType: uint32_t {
        Point,
        Box,
        Gaussian,
        Blackman,
        Mitchell,
        Catmullrom,
        Triangle,
        Sinc,
        Closest,
        Farthest,
        Min,
        Max,
        Additive
    };

    struct FilterPass {
        ComputeProgram::SharedPtr pProgram;
        ComputeVars::SharedPtr    pVars;
        ComputeState::SharedPtr   pState;
    };

    static const Info kInfo;
    
    virtual ~AccumulatePass() = default;

    static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

    virtual Dictionary getScriptingDictionary() override;
    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void compile(RenderContext* pContext, const CompileData& compileData) override;
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
    virtual void onHotReload(HotReloadFlags reloaded) override;


    void setScene(const Scene::SharedPtr& pScene);
    void enableAccumulation(bool enable = true);

    void setOutputFormat(ResourceFormat format);

    void setPixelFilterSize(uint2 size);
    void setPixelFilterSize(uint32_t width, uint32_t height);
    void setPixelFilterType(PixelFilterType type);
    void setPixelFilterType(const std::string& typeName);

    inline Falcor::ResourceFormat format() const { return mOutputFormat; }

    void reset() { mFrameCount = 0; }

    // Get the number of singular values of a filter according to SVD theorem. This might be an overenginered for our use case.. 
    static size_t pixelFilterSingularValueCountSVD(PixelFilterType pixelFilterType, uint32_t width, uint32_t height);

 protected:
    AccumulatePass(Device::SharedPtr pDevice, const Dictionary& dict);
    void prepareAccumulation(RenderContext* pRenderContext, uint32_t width, uint32_t height);
    void preparePixelFilterKernelTexture(RenderContext* pRenderContext);
    void prepareFilteredTextures(const Texture::SharedPtr& pSrc);
    void prepareImageSampler(RenderContext* pContext);

    // Internal state
    Scene::SharedPtr            mpScene;                        ///< The current scene (or nullptr if no scene).
    Camera::SharedPtr           mpCamera;                       ///< Current scene camera;
    std::map<Precision, ComputeProgram::SharedPtr> mpProgram;   ///< Accumulation programs, one per mode.
    ComputeVars::SharedPtr      mpVars;                         ///< Program variables.
    ComputeState::SharedPtr     mpState;
    std::array<FilterPass, 2>   mFilterPasses;                  ///< Horizontal and vertical sample plane filtering passes data
    

    uint32_t                    mFrameCount = 0;                ///< Number of accumulated frames. This is reset upon changes.
    uint2                       mFrameDim = { 0, 0 };           ///< Current frame dimension in pixels.
    Texture::SharedPtr          mpLastFrameSum;                 ///< Last frame running sum. Used in Single and SingleKahan mode.
    Texture::SharedPtr          mpLastFrameCorr;                ///< Last frame running compensation term. Used in SingleKahan mode.
    Texture::SharedPtr          mpLastFrameSumLo;               ///< Last frame running sum (lo bits). Used in Double mode.
    Texture::SharedPtr          mpLastFrameSumHi;               ///< Last frame running sum (hi bits). Used in Double mode.

    Texture::SharedPtr          mpFilteredImage;                ///< We store filtered image here instead fo modifying original as it might be used elsewhere.
    Texture::SharedPtr          mpTmpFilteredImage;             ///< Temporary image data used for two pass filtering
    Texture::SharedPtr          mpKernelTexture;                ///< Filter kernel texture;
    Sampler::SharedPtr          mpKernelSampler;                ///< Filter kernel texture sampler;
    Sampler::SharedPtr          mpImageSampler;                 ///< Unnormalized image reading sampler;
    Texture::SharedPtr          (*mpFilterCreateKernelTextureFunc)(Device::SharedPtr, uint32_t, bool) = NULL;

    bool                        mDoHorizontalFiltering = false;
    bool                        mDoVerticalFiltering = false;
    bool                        mEnableAccumulation = true;     ///< UI control if accumulation is enabled.
    bool                        mAutoReset = false;             ///< Reset accumulation automatically upon scene changes, refresh flags, and/or subframe count.
    Precision                   mPrecisionMode = Precision::Single;
    uint32_t                    mSubFrameCount = 0;             ///< Number of frames to accumulate before reset. Useful for generating references.

    ResourceFormat              mOutputFormat = ResourceFormat::RGBA16Float;

    PixelFilterType             mPixelFilterType = PixelFilterType::Box;
    uint2                       mPixelFilterSize = {1, 1};

    bool                        mDirty = true;

};

#define enum2str(a) case  AccumulatePass::Precision::a: return #a
inline std::string to_string(AccumulatePass::Precision mode) {
    switch (mode) {
        enum2str(Double);
        enum2str(Single);
        enum2str(SingleCompensated);
    default:
        should_not_get_here(); return "";
    }
}
#undef enum2str

#define pftype2str(a) case AccumulatePass::PixelFilterType::a: return #a
inline std::string to_string(AccumulatePass::PixelFilterType a) {
    switch (a) {
        pftype2str(Point);
        pftype2str(Box);
        pftype2str(Gaussian);
        pftype2str(Blackman);
        pftype2str(Mitchell);
        pftype2str(Catmullrom);
        pftype2str(Triangle);
        pftype2str(Sinc);
        pftype2str(Closest);
        pftype2str(Farthest);
        pftype2str(Min);
        pftype2str(Max);
        pftype2str(Additive);
        default: should_not_get_here(); return "";
    }
}
#undef pftype2str

#endif  // SRC_FALCOR_RENDERPASSES_ACCUMULATEPASS_ACCUMULATEPASS_H_