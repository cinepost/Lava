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
#ifndef SRC_FALCOR_RENDERPASSES_TEXTURESRESOLVEPASS_H_
#define SRC_FALCOR_RENDERPASSES_TEXTURESRESOLVEPASS_H_

#include "Falcor.h"
#include "FalcorExperimental.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/Utils/Math/Vector.h"

#include <slang/slang.h>

using namespace Falcor;

#include "TexturesResolveData.slangh"

#ifdef BUILD_DEPTH_PASS
// #define dllpassdecl __declspec(dllexport)
#define dllpassdecl falcorexport
#else
// #define dllpassdecl __declspec(dllimport)
#define dllpassdecl falcorimport
#endif

class dllpassdecl TexturesResolvePass : public RenderPass {
 public:
    using SharedPtr = std::shared_ptr<TexturesResolvePass>;

    static const Info kInfo;

    /** Create a new object
    */
    static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void execute(RenderContext* pContext, const RenderData& renderData) override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
    virtual Dictionary getScriptingDictionary() override;

    TexturesResolvePass& setDepthStencilState(const DepthStencilState::SharedPtr& pDsState);
    TexturesResolvePass& setRasterizerState(const RasterizerState::SharedPtr& pRsState);

 private:
    TexturesResolvePass(Device::SharedPtr pDevice, const Dictionary& dict);
    void parseDictionary(const Dictionary& dict);

    void initDepth(RenderContext* pContext, const RenderData& renderData);
    void updateTexturesResolveData();

    void createMipCalibrationTexture(RenderContext* pRenderContext);
    void createLtxCalibrationTexture(RenderContext* pRenderContext);

    Fbo::SharedPtr              mpFbo;
    GraphicsState::SharedPtr    mpState;
    GraphicsVars::SharedPtr     mpVars;
    RasterizerState::SharedPtr  mpRsState;
    Scene::SharedPtr            mpScene;

    GraphicsProgram::SharedPtr      mpProgram;
    DepthStencilState::SharedPtr    mpDsNoDepthWrite;

    ResourceFormat mTileDataDebugFormat = ResourceFormat::RGBA16Unorm;

    ParameterBlock::SharedPtr   mpDataBlock;
    Buffer::SharedPtr           mpTexResolveDataBuffer;
    bool                        mUsePreGenDepth = false;


    Texture::SharedPtr          mpMipCalibrationTexture = nullptr;
    Texture::SharedPtr          mpLtxCalibrationTexture = nullptr;
};

#endif  // SRC_FALCOR_RENDERPASSES_TEXTURESRESOLVEPASS_H_
