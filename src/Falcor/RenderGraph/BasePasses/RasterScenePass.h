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
#ifndef SRC_FALCOR_RENDERGRAPH_BASEPASSES_RASTERSCENEPASS_H_
#define SRC_FALCOR_RENDERGRAPH_BASEPASSES_RASTERSCENEPASS_H_

#include "BaseGraphicsPass.h"
#include "../RenderPass.h"

namespace Falcor {

class dlldecl RasterScenePass : public BaseGraphicsPass, public std::enable_shared_from_this<RasterScenePass> {
 public:
    //using SharedPtr = ParameterBlockSharedPtr::shared_ptr<RasterScenePass>;
    using SharedPtr = ParameterBlockSharedPtr<RasterScenePass>;

    /** Create a new object.
        \param[in] pScene The scene object.
        \param[in] progDesc The program description.
        \param[in] programDefines Optional list of macro definitions to set into the program. The macro definitions will be set on all shader stages.
        \return A new object, or throws an exception if creation failed.
    */
    static SharedPtr create(std::shared_ptr<Device> pDevice, const std::shared_ptr<Scene>& pScene, const Program::Desc& progDesc, const Program::DefineList& programDefines = Program::DefineList());

    /** Create a new object.
        \param[in] pScene The scene object
        \param[in] filename Program filename.
        \param[in] vsEntry Vertex shader entry point. If this string is empty (""), it will use a default vertex shader which transforms and outputs all default vertex attributes.
        \param[in] psEntry Pixel shader entry point.
        \param[in] programDefines Optional list of macro definitions to set into the program. The macro definitions will be set on all shader stages.
        \return A new object, or throws an exception if creation failed.
    */
    static SharedPtr create(std::shared_ptr<Device> pDevice, const std::shared_ptr<Scene>& pScene, const std::string& filename, const std::string& vsEntry, const std::string& psEntry, const Program::DefineList& programDefines = Program::DefineList());

    /** Render the scene into the dst FBO
    */
    void renderScene(RenderContext* pContext, const Fbo::SharedPtr& pDstFbo);

    /** Get the scene
    */
    const std::shared_ptr<Scene>& getScene() const { return mpScene; }
 private:
    RasterScenePass(std::shared_ptr<Device> pDevice, const std::shared_ptr<Scene>& pScene, const Program::Desc& progDesc, const Program::DefineList& programDefines);
    std::shared_ptr<Scene> mpScene;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_RENDERGRAPH_BASEPASSES_RASTERSCENEPASS_H_
