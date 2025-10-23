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
#ifndef FALCOR_RENDERGRAPH_RENDERGRAPHEXE_H_
#define FALCOR_RENDERGRAPH_RENDERGRAPHEXE_H_

#include "Falcor/Core/Object.h"
#include "ResourceCache.h"
#include "Falcor/Utils/Dictionary.h"
#include "RenderPass.h"

namespace Falcor {

class RenderGraphCompiler;

class FALCOR_API RenderGraphExe : public Object {
  public:
    struct Context {
        RenderContext* pRenderContext;
        Dictionary& passesDictionary;
        uint2 defaultTexDims;
        ResourceFormat defaultTexFormat;

        uint32_t frameNumber;
        uint32_t sampleNumber;
    };

    /** Execute the graph
    */
    void execute(const Context& ctx, uint32_t frameNumber = 0, uint32_t sampleNumber = 0);

    bool beginFrame(const Context& ctx, uint32_t frameNumber = 0);

    void endFrame(const Context& ctx, uint32_t frameNumber = 0);

    /** Resolve graph's per frame sparse resources
    */
    void resolvePerFrameSparseResources(const Context& ctx);

    /** Resolve graph's per sample sparse resources
    */
    void resolvePerSampleSparseResources(const Context& ctx);

    /** Called upon hot reload (by pressing F5).
        \param[in] reloaded Resources that have been reloaded.
    */
    void onHotReload(HotReloadFlags reloaded);

    /** Get a resource from the cache
    */
    Resource::SharedPtr getResource(const std::string& name) const;

    /** Set an external input resource
        \param[in] name Input name. Has the format `renderPassName.resourceName`
        \param[in] pResource The resource to bind. If this is nullptr, will unregister the resource
    */
    void setInput(const std::string& name, const Resource::SharedPtr& pResource);

private:
    friend class RenderGraphCompiler;

    void insertPass(const std::string& name, const RenderPass::SharedPtr& pPass);

    struct Pass {
        std::string name;
        RenderPass::SharedPtr pPass;
      
      private:
        friend class RenderGraphExe; // Force RenderGraphCompiler to use insertPass() by hiding this Ctor from it
        Pass(const std::string& name_, const RenderPass::SharedPtr& pPass_) : name(name_), pPass(pPass_) {}
    };

    std::vector<Pass> mExecutionList;
    std::unique_ptr<ResourceCache> mpResourceCache;
};

}  // namespace Falcor

#endif  // FALCOR_RENDERGRAPH_RENDERGRAPHEXE_H_
