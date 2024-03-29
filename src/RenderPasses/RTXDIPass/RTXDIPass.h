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
#pragma once

#include "Falcor.h"
#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/Rendering/RTXDI/RTXDI.h"

using namespace Falcor;

/** This RenderPass provides a simple example of how to use the RTXDI module
    available in the "Source/Falcor/Rendering/RTXDI/" directory.

    See the RTXDI.h header for more explicit instructions.

    This pass consists of two compute passes:

    - PrepareSurfaceData.slang takes in a Falcor VBuffer (e.g. from the GBuffer
      render pass) and sets up the surface data required by RTXDI to perform
      light sampling.
    - FinalShading.slang takes the final RTXDI light samples, checks visiblity
      and shades the pixels by evaluating the actual material's BSDF.

    Please see the README on how to install the RTXDI SDK.
*/
class RTXDIPass : public RenderPass
{
public:
    using SharedPtr = std::shared_ptr<RTXDIPass>;

    static const Info kInfo;

    /** Create a new render pass object.
        \param[in] pRenderContext The render context.
        \param[in] dict Dictionary of serialized parameters.
        \return A new object, or an exception is thrown if creation failed.
    */
    static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

    virtual Dictionary getScriptingDictionary() override;
    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override;
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;

private:
    RTXDIPass(Device::SharedPtr pDevice, const Dictionary& dict);
    void parseDictionary(const Dictionary& dict);

    void prepareSurfaceData(RenderContext* pRenderContext, const Texture::SharedPtr& pVBuffer);
    void finalShading(RenderContext* pRenderContext, const Texture::SharedPtr& pVBuffer, const RenderData& renderData);

    Scene::SharedPtr        mpScene;

    RTXDI::SharedPtr        mpRTXDI;
    RTXDI::Options          mOptions;

    ComputePass::SharedPtr  mpPrepareSurfaceDataPass;
    ComputePass::SharedPtr  mpFinalShadingPass;

    uint2                   mFrameDim = { 0, 0 };
    bool                    mOptionsChanged = false;
    bool                    mGBufferAdjustShadingNormals = false;
};
