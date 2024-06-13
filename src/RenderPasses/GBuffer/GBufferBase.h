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
#ifndef SRC_FALCOR_RENDERPASSES_GBUFFER_GBUFFERBASE_H_
#define SRC_FALCOR_RENDERPASSES_GBUFFER_GBUFFERBASE_H_

#include <pybind11/embed.h>

#include "Falcor/Falcor.h"
#include "Falcor/Core/Framework.h"
#include "Falcor/Scene/Scene.h"
#include "Falcor/RenderGraph/RenderGraph.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"
#include "Falcor/Utils/Scripting/ScriptBindings.h"
#include "Falcor/RenderGraph/RenderPassStandardFlags.h"
#include "Falcor/Utils/SampleGenerators/DxSamplePattern.h"
#include "Falcor/Utils/SampleGenerators/HaltonSamplePattern.h"
#include "Falcor/Utils/SampleGenerators/StratifiedSamplePattern.h"


using namespace Falcor;

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib);

/** Base class for the different types of G-buffer passes (including V-buffer).
*/
class PASS_API GBufferBase : public RenderPass {
 public:
    enum class SamplePattern : uint32_t {
        Center,
        DirectX,
        Halton,
        Stratified,
    };

    virtual void compile(RenderContext* pContext, const CompileData& compileData) override;
    virtual void resolvePerFrameSparseResources(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual Dictionary getScriptingDictionary() override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
    virtual void setCullMode(RasterizerState::CullMode mode);
    
    GBufferBase& setDepthBufferFormat(ResourceFormat format);

    static const std::string& to_define_string(RasterizerState::CullMode mode);
    
 protected:
    GBufferBase(Device::SharedPtr pDevice, Info info);
    virtual void parseDictionary(const Dictionary& dict);
    void updateFrameDim(const uint2 frameDim);
    void updateSamplePattern();
    Texture::SharedPtr getOutput(const RenderData& renderData, const std::string& name) const;

    // Internal state
    Scene::SharedPtr                mpScene;
    CPUSampleGenerator::SharedPtr   mpSampleGenerator;

    uint32_t                        mFrameCount = 0;
    uint2                           mFrameDim = {};
    float2                          mInvFrameDim = {};
    ResourceFormat                  mDepthFormat = ResourceFormat::D32Float;
    ResourceFormat                  mVBufferFormat = HitInfo::kDefaultFormat;

    // UI variables
    SamplePattern                   mSamplePattern = SamplePattern::Stratified; ///< Which camera jitter sample pattern to use.
    uint32_t                        mSampleCount = 1024;                        ///< Sample count for camera jitter.
    bool                            mUseAlphaTest = true;                           ///< Enable alpha test.
    bool                            mAdjustShadingNormals = false;                  ///< Adjust shading normals.
    bool                            mForceCullMode = false;                         ///< Force cull mode for all geometry, otherwise set it based on the scene.
    RasterizerState::CullMode       mCullMode = RasterizerState::CullMode::None;    ///< Cull mode to use for when mForceCullMode is true.
    bool                            mOptionsChanged = false;

    bool                            mDirty = true; ///< Pass parameters/resources changed

    static void registerBindings(pybind11::module& m);
    friend void getPasses(Falcor::RenderPassLibrary& lib);
};

#endif  // SRC_FALCOR_RENDERPASSES_GBUFFER_GBUFFERBASE_H_
