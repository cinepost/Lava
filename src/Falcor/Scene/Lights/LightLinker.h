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
#ifndef SRC_FALCOR_SCENE_LIGHTS_LIGH_LINKER_H_
#define SRC_FALCOR_SCENE_LIGHTS_LIGH_LINKER_H_

#include <algorithm>
#include <iterator>
#include <map>
#include <set>

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/Program/ShaderVar.h"
#include "Falcor/Core/State/GraphicsState.h"
#include "Falcor/Core/Program/GraphicsProgram.h"
#include "RenderGraph/BasePasses/ComputePass.h"

#include "LightLinkerShared.slang"


namespace Falcor {

class Scene;
class Device;

/** Class that holds a collection of bit masks for objects/lights for a scene.

    Each bit mask represents a light or object visibility either in lighting/shadow calculations or ray visibility.

    This class has utility functions for updating and pre-processing the object/light bit masks.
    The LightLinker can be used standalone, but more commonly it will be used by an light helper.
*/
class dlldecl LightLinker : public std::enable_shared_from_this<LightLinker> {
    public:
        using SharedPtr = std::shared_ptr<LightLinker>;
        using SharedConstPtr = std::shared_ptr<const LightLinker>;

        using LightNamesList = std::vector<std::string>; // type alias for std::vector<std::string>

        static const uint32_t kInvalidLightSetIndex   = 0xffffffff;
        static const uint32_t kInvalidTraceSetIndex   = 0xffffffff;

        enum class UpdateFlags : uint32_t {
            None                = 0u,   ///< Nothing was changed.
            LightsChanges       = 1u,   ///< Lights data changed.
            ObjectsChanges      = 2u,   ///< Objects (instances) data changed.
        };

        struct UpdateStatus {
            std::vector<UpdateFlags> linksUpdateInfo;
        };

        ~LightLinker() = default;

        /** Creates a light collection for the given scene.
            Note that update() must be called before the collection is ready to use.
            \param[in] pRenderContext The render context.
            \param[in] pScene The scene.
            \return Ptr to the created object, or nullptr if an error occured.
        */
        static SharedPtr create(std::shared_ptr<Device> pDevice, std::shared_ptr<Scene> pScene = nullptr);

        /** Updates the light collection to the current state of the scene.
            \param[in] pRenderContext The render context.
            \param[out] pUpdateStatus Stores information about which type of updates were performed for each mesh light. This is an optional output parameter.
            \return True if the lighting in the scene has changed since the last frame.
        */
        bool update(RenderContext* pRenderContext, UpdateStatus* pUpdateStatus = nullptr);

        uint32_t addLight(const Light::SharedPtr& pLight);

        uint32_t getLightSetIndex(const LightNamesList& lightNames) const;
        uint32_t getOrCreateLightSetIndex(const LightNamesList& lightNames);

        /** Bind the light collection data to a given shader var
            \param[in] var The shader variable to set the data into.
            \return True if successful, false otherwise.
        */
        void setShaderData(const ShaderVar& var) const;

        /** Prepare for syncing the CPU data.
            If the mesh light triangles will be accessed with getMeshLightTriangles()
            performance can be improved by calling this function ahead of time.
            This function schedules the copies so that it can be read back without delay later.
        */
        void prepareSyncCPUData(RenderContext* pRenderContext) const { copyDataToStagingBuffer(pRenderContext); }

        /** Get the total GPU memory usage in bytes.
        */
        uint64_t getMemoryUsageInBytes() const;

        // Internal update flags. This only public for enum_class_operators() to work.
        enum class CPUOutOfDateFlags : uint32_t {
            None         = 0,
            TriangleData = 0x1,
            FluxData     = 0x2,

            All          = TriangleData | FluxData
        };

    private:
        struct LightSetCPU {
            std::set<std::string> mLightNames;

            uint32_t mLightsCount;           
            uint32_t mLightsOffset;             ///< Light sets indices buffer offset

            bool hasLightName(const std::string& name) const;
            void addLightName(const std::string& name);
            size_t lightsCount() const { return mLightNames.size(); }

            operator LightSet() const { return { .lightsCount = mLightsCount, .lightsOffset = mLightsOffset}; }
        };

        void buildLightsBuffer();

        bool findLightSetIndex(const LightNamesList& lightNames, uint32_t& index) const;


    protected:
        LightLinker(std::shared_ptr<Device> pDevice, std::shared_ptr<Scene> pScene = nullptr);

        void copyDataToStagingBuffer(RenderContext* pRenderContext) const;
        void syncCPUData() const;

        // Internal state
        Device::SharedPtr                       mpDevice = nullptr;
        std::weak_ptr<Scene>                    mpScene;                        ///< Weak pointer to scene (scene owns LightLinker).
        
        mutable CPUOutOfDateFlags               mCPUInvalidData = CPUOutOfDateFlags::None;  ///< Flags indicating which CPU data is valid.
        mutable bool                            mStagingBufferValid = true;                 ///< Flag to indicate if the contents of the staging buffer is up-to-date.

        bool                                    mLightsChanged = false;

        std::map<std::string, Light::SharedPtr> mLightsMap;                     ///< All analytic lights. Note that not all may be active.

        std::vector<LightSetCPU>                mLightSets;

        std::vector<uint32_t>                   mLightSetsIndicesBuffer;        ///< Light sets light indices.

        // Resources
        Buffer::SharedPtr mpLightsBuffer;

};
/*
std::string to_string(const LightLinker::LightNamesList& l) {
    std::string out;
    if ( !l.empty() ) {
        for(const std::string& s: l) {
            out += s;
            out += " ";
        }
        out.pop_back();
    }
    return out;
}
*/
enum_class_operators(LightLinker::CPUOutOfDateFlags);
enum_class_operators(LightLinker::UpdateFlags);

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_LIGHTS_LIGH_LINKER_H_