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
#ifndef SRC_FALCOR_RENDERPASSES_GBUFFER_VBUFFER_VBUFFERRASTER_H_
#define SRC_FALCOR_RENDERPASSES_GBUFFER_VBUFFER_VBUFFERRASTER_H_

#include "../GBufferBase.h"

using namespace Falcor;

/** Rasterized V-buffer pass.

    This pass renders a visibility buffer using ray tracing.
    The visibility buffer encodes the mesh instance ID and primitive index,
    as well as the barycentrics at the hit point.
*/
class VBufferRaster : public GBufferBase {
  public:
    using SharedPtr = std::shared_ptr<VBufferRaster>;

    static const Info kInfo;

    static SharedPtr create(RenderContext* pRenderContext, const Dictionary& dict);

    RenderPassReflection reflect(const CompileData& compileData) override;
    void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
    void execute(RenderContext* pRenderContext, const RenderData& renderData) override;

    VBufferRaster& setPerPixelJitterRaster(bool state=false);

  private:
    struct SubPass {
      Fbo::SharedPtr pFbo;
      Texture::SharedPtr pVBuff;
      Texture::SharedPtr pDepth;
      float2 cameraJitterOffset;
      CPUSampleGenerator::SharedPtr pSampleGenerator;
    };

    VBufferRaster(Device::SharedPtr pDevice, const Dictionary& dict);

    virtual void parseDictionary(const Dictionary& dict) override;
    void initDepth(RenderContext* pContext, const RenderData& renderData);
    void initQuarterBuffers(RenderContext* pContext, const RenderData& renderData);

    // Internal state
    Fbo::SharedPtr                mpFbo;
    Texture::SharedPtr            mpDepth;

    // Quad view rendering (fake per-pixel jitter)
    bool mPerPixelJitterRaster = false;
    uint2  mQuarterFrameDim    = {0, 0};
    std::array<SubPass, 4>        mSubPasses;
    ComputeProgram::SharedPtr     mpCombineQuadsProgram;
    ComputeVars::SharedPtr        mpCombineQuadsVars;
    ComputeState::SharedPtr       mpCombineQuadsState;
    CPUSampleGenerator::SharedPtr mpSampleGenerator;

    struct {
      GraphicsState::SharedPtr pState;
      GraphicsProgram::SharedPtr pProgram;
      GraphicsVars::SharedPtr pVars;
    } mRaster;

    static const char* kDesc;
    friend void getPasses(Falcor::RenderPassLibrary& lib);
};

#endif  // SRC_FALCOR_RENDERPASSES_GBUFFER_VBUFFER_VBUFFERRASTER_H_
