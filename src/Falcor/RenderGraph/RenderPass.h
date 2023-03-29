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
#ifndef FALCOR_RENDERGRAPH_RENDERPASS_H_
#define FALCOR_RENDERGRAPH_RENDERPASS_H_

#include <memory>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Utils/Scripting/Dictionary.h"
#include "Falcor/Utils/InternalDictionary.h"
#include "ResourceCache.h"

namespace Falcor {

class Scene;

/** Helper class that's passed to the user during `RenderPass::execute()`
*/
class dlldecl RenderData {
 public:
    /** Get a resource
        \param[in] name The name of the pass' resource (i.e. "outputColor"). No need to specify the pass' name
        \return If the name exists, a pointer to the resource. Otherwise, nullptr
    */
    inline const Resource::SharedPtr& operator[](const std::string& name) const { return getResource(name); }

    /** Get a resource
        \param[in] name The name of the pass' resource (i.e. "outputColor"). No need to specify the pass' name
        \return If the name exists, a pointer to the resource. Otherwise, nullptr
    */
    const Resource::SharedPtr& getResource(const std::string& name) const;

    /** Get a texture resource
        \param[in] name The name of the pass' resource (i.e. "outputColor"). No need to specify the pass' name
        \return If the name exists, a pointer to the resource. Otherwise, nullptr
    */
    const Texture::SharedPtr& getTexture(const std::string& name) const;

    /** Get a buffer resource
        \param[in] name The name of the pass' resource (i.e. "outputColor"). No need to specify the pass' name
        \return If the name exists, a pointer to the resource. Otherwise, nullptr
    */
    const Buffer::SharedPtr& getBuffer(const std::string& name) const;

    /** Get the global dictionary. You can use it to pass data between different passes
    */
    inline InternalDictionary& getDictionary() const { return (*mpDictionary); }

    /** Get the global dictionary. You can use it to pass data between different passes
    */
    inline InternalDictionary::SharedPtr getDictionaryPtr() const { return mpDictionary; }

    /** Get the default dimensions used for Texture2Ds (when `0` is specified as the dimensions in `RenderPassReflection`)
    */
    inline const uint2& getDefaultTextureDims() const { return mDefaultTexDims; }

    /** Get the default format used for Texture2Ds (when `Unknown` is specified as the format in `RenderPassReflection`)
    */
    inline ResourceFormat getDefaultTextureFormat() const { return mDefaultTexFormat; }
 protected:
    friend class RenderGraphExe;
    
    RenderData(const std::string& passName, const ResourceCache::SharedPtr& pResourceCache, const InternalDictionary::SharedPtr& pDict, const uint2& defaultTexDims, ResourceFormat defaultTexFormat
        ,uint32_t frameNumber = 0, uint32_t sampleNumber = 0);
    
    const std::string& mName;
    ResourceCache::SharedPtr mpResources;
    InternalDictionary::SharedPtr mpDictionary;
    uint2 mDefaultTexDims;
    ResourceFormat mDefaultTexFormat;
    uint32_t mFrameNumber;
    uint32_t mSampleNumber;

    Texture::SharedPtr mpNullTexture = nullptr;
    Buffer::SharedPtr mpNullBuffer = nullptr;
};

/** Base class for render passes.

    Render passes are expected to implement a static create() function that returns
    a shared pointer to a new object, or throws an exception if creation failed.
    The constructor should be private to force creation of shared pointers.

    Render passes are inserted in a render graph, which is executed at runtime.
    Each render pass declares its I/O requirements in the reflect() function,
    and as part of the render graph compilation their compile() function is called.
    At runtime, execute() is called each frame to generate the pass outputs.
*/
class dlldecl RenderPass : public std::enable_shared_from_this<RenderPass> {
 public:
    using SharedPtr = std::shared_ptr<RenderPass>;
    virtual ~RenderPass() = default;

    // Render pass info.
    struct Info
    {
        std::string type;   ///< Type name of the render pass. In general this should match the name of the class implementing the render pass.
        std::string desc;   ///< Brief textural description of what the render pass does.
    };

    struct CompileData {
        uint2 defaultTexDims;                       ///< Default texture dimension (same as the swap chain size).
        ResourceFormat defaultTexFormat;            ///< Default texture format (same as the swap chain format).
        RenderPassReflection connectedResources;    ///< Reflection data for connected resources, if available. This field may be empty when reflect() is called.
    };

    /** Get the render pass info data.
    */
    inline const Info& getInfo() const { return mInfo; }

    /** Get the render pass type.
    */
    inline const std::string& getType() const { return mInfo.type; }

    /** Get the render pass description.
    */
    inline const std::string& getDesc() const { return mInfo.desc; }


    /** Called before render graph compilation. Describes I/O requirements of the pass.
        The function may be called repeatedly and should not perform any expensive operations.
        The requirements can't change after the graph is compiled. If the I/O are dynamic, you'll need to
        trigger re-compilation of the render graph yourself by calling 'requestRecompile()'.
    */
    virtual RenderPassReflection reflect(const CompileData& compileData) = 0;

    /** Will be called during graph compilation. You should throw an exception in case the compilation failed
    */
    virtual void compile(RenderContext* pContext, const CompileData& compileData) {}

    /** Executes the pass.
    */
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) = 0;

    /** If pass has frame dependent sparse resources they should be resolved within this call
    */
    virtual void resolvePerFrameSparseResources(RenderContext* pRenderContext, const RenderData& renderData) {}

    /** If pass has sample dependent sparse resources they should be resolved within this call
    */
    virtual void resolvePerSampleSparseResources(RenderContext* pRenderContext, const RenderData& renderData) {}


    /** Get a dictionary that can be used to reconstruct the object
    */
    virtual Dictionary getScriptingDictionary() { return {}; }

    /** Reset frame dependent data
    */
    virtual void reset() {}

    /** Set a scene into the render-pass
    */
    virtual void setScene(RenderContext* pRenderContext, const std::shared_ptr<Scene>& pScene) {}

    /** Called upon hot reload.
        \param[in] reloaded Resources that have been reloaded.
    */
    virtual void onHotReload(HotReloadFlags reloaded) {}

    /** Get the current pass' name as defined in the graph
    */
    inline const std::string& getName() const { return mName; }

 protected:
    friend class RenderGraph;
    RenderPass(Device::SharedPtr pDevice, const Info& info);
    
    /** Request a recompilation of the render graph.
        Call this function if the I/O requirements of the pass have changed.
        During the recompile, reflect() will be called for the pass to report the new requirements.
    */
    inline void requestRecompile() { mPassChangedCB(); }

    const Info mInfo;
    std::string mName;

    std::function<void(void)> mPassChangedCB = [] {};
    Device::SharedPtr mpDevice;
    
};
}

#endif  // SRC_FALCOR_RENDERGRAPH_RENDERPASS_H_
