/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
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
#include "../GBufferBase.h"
#include "Utils/Sampling/SampleGenerator.h"

using namespace Falcor;

/** Ray traced V-buffer pass.

    This pass renders a visibility buffer using ray tracing.
    The visibility buffer encodes the mesh instance ID and primitive index,
    as well as the barycentrics at the hit point.
*/
class PASS_API VBufferRT : public GBufferBase {
        FALCOR_OBJECT(VBufferRT)
        FALCOR_PLUGIN_CLASS(VBufferRT, "VBufferRT", "Ray traced V-buffer generation pass.");
public:
    static SharedPtr create(RenderContext* pRenderContext, const Properties& props);

    RenderPassReflection reflect(const CompileData& compileData) override;
    void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    Properties getProperties() const override;
    void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;

    void setPerPixelJitter(bool value);
    void enableMotionBlur(bool value);
    void enableDepthOfField(bool value);

    virtual bool beginFrame(RenderContext *pContext, const RenderData& renderData) override;

private:
    void executeRaytrace(RenderContext* pRenderContext, const RenderData& renderData);
    void executeCompute(RenderContext* pRenderContext, const RenderData& renderData);

    Program::DefineList getShaderDefines(const RenderData& renderData) const;
    void bindShaderData(const ShaderVar& var, const RenderData& renderData);
    void recreatePrograms();

    VBufferRT(Device::SharedPtr pDevice, const Properties& props);
    void parseProperties(const Properties& props) override;

    // Internal state
    bool mUsePerPixelJitter = false;
    bool mUseMotionBlur = false;
    bool mComputeDOF = false;           ///< Flag indicating if depth-of-field is computed for the current frame.
    SampleGenerator::SharedPtr mpSampleGenerator;

    bool mUseCompute = true;
    bool mUseDOF = true;                ///< Option for enabling depth-of-field when camera's aperture radius is nonzero.

    uint32_t mSampleNumber = 0;

    struct {
        Program::SharedPtr pProgram;
        RtProgramVars::SharedPtr pVars;
    } mRaytrace;

    ComputePass::SharedPtr mpComputePass;
};
