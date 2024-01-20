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
#include "Falcor/Scene/Scene.h"
#include "RenderGraph/BasePasses/ComputePass.h"

#include "LightLinkerShared.slang"


namespace Falcor {

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

        using StringList     = std::vector<std::string>; // type alias for std::vector<std::string>
        using StringSet      = std::set<std::string>;
        using LightMap       = std::unordered_map<std::string, Light::SharedPtr>;

        static const uint32_t       kInvalidLightSetIndex   = 0xffffffff;
        static const uint32_t       kInvalidTraceSetIndex   = 0xffffffff;

        enum class UpdateFlags : uint32_t {
            None                = 0u,   ///< Nothing was changed.
            LightsChanged       = 1u,   ///< Lights data changed.
            LightSetsChanged    = 2u,   ///< LightSets data changed.
            All                 = LightsChanged | LightSetsChanged
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

        /** Get default shader defines.
            This is the minimal set of defines needed for a program to compile that imports the material system module.
            Note that the actual defines need to be set at runtime, call getDefines() to query them.
        */
        static Shader::DefineList getDefaultDefines();

        /** Get shader defines.
            These need to be set before binding the material system parameter block.
            \return List of shader defines.
        */
        Shader::DefineList getDefines() const;

        /** Updates the light collection to the current state of the scene.
        */
        UpdateFlags update(bool forceUpdate);

        uint32_t addLight(const Light::SharedPtr& pLight);

        void     updateLight(const Light::SharedPtr& pLight);

        uint32_t getOrCreateLightSetIndex(const std::string& lightNamesString);

        uint32_t getOrCreateLightSetIndex(const StringList& lightNamesList);

        bool findLightSetIndex(const StringList& lightNamesList, uint32_t& index) const;

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

        void setLightsActive(bool state);

        void deleteLight(const std::string& name);

        void setLightActive(const std::string& name, bool state);

        static StringList lightNamesStringToList(const std::string& lightNamesString);

        // Internal update flags. This only public for enum_class_operators() to work.
        enum class CPUOutOfDateFlags : uint32_t {
            None          = 0,
            LightsData    = 0x1,
            LightSetsData = 0x2,

            All           = LightsData | LightSetsData
        };

    private:
        class NameSet {
            public:
                NameSet(const StringList& names);
                NameSet(const StringSet& namesSet);

                bool isInUse() const { return mUseCounter > 0; }

                bool hasName(const std::string& name) const;
                size_t namesCount() const { return mNames.size(); }

                const std::set<std::string>& names() const { return mNames; }
        
            private:
                NameSet();

                void increaseUseCounter() { if(mUseCounter < std::numeric_limits<size_t>::max()) mUseCounter++;}
                void decreaseUseCounter() { if(mUseCounter > 0) mUseCounter--;}

            private:
                StringSet   mNames;
                size_t      mUseCounter;   ///< Number of scene entities using this set

                friend LightLinker;
        };

        class LightSet: public NameSet {
            public:
                LightSet(const StringList& lightNames);
                LightSet(const StringSet& lightNamesSet);

                const LightSetData&  getData() const { return mLightSetData; }

            private:
                LightSet();

            private:
                LightSetData          mLightSetData;

                friend LightLinker;
        };

        class TraceSet: public NameSet {
            public:
                Scene::TlasData&      getTlasData() { return mTlasData; }

            private:
                Scene::TlasData       mTlasData;
        };

        bool buildActiveLightsData(bool force);
        bool buildLightsIndirectionData(bool force);
        bool buildLightSetsData(bool force);

    protected:
        LightLinker(std::shared_ptr<Device> pDevice, std::shared_ptr<Scene> pScene = nullptr);

        void copyDataToStagingBuffer(RenderContext* pRenderContext) const;
        void syncCPUData() const;

        // Internal state
        Device::SharedPtr                           mpDevice = nullptr;
        std::weak_ptr<Scene>                        mpScene;                        ///< Weak pointer to scene (scene owns LightLinker).
        
        mutable CPUOutOfDateFlags                   mCPUInvalidData = CPUOutOfDateFlags::None;  ///< Flags indicating which CPU data is valid.
        mutable bool                                mStagingBufferValid = true;                 ///< Flag to indicate if the contents of the staging buffer is up-to-date.

        LightMap                                    mLightsMap;                     ///< All analytic lights. Note that not all may be active.
        std::unordered_map<std::string, uint32_t>   mActiveLightIDsMap;

        std::vector<LightSet>                       mLightSets;
        std::vector<TraceSet>                       mShadowSets;
        std::vector<TraceSet>                       mReflectSets;
        std::vector<TraceSet>                       mRefractSets;

        std::vector<uint32_t>                       mLightSetsIndicesBuffer;        ///< Light sets light indices.

        // State
        mutable bool                                mLightSetsChanged = false;
        mutable bool                                mLightsChanged = false;

        // Resources
        mutable std::vector<LightData>              mActiveLightsData;
        mutable std::vector<LightSetData>           mLightSetsData;
        mutable std::vector<uint32_t>               mIndirectionData;

        mutable Buffer::SharedPtr                   mpLightsDataBuffer;
        mutable Buffer::SharedPtr                   mpLightSetsDataBuffer;
        mutable Buffer::SharedPtr                   mpIndirectionTableBuffer;

};


enum_class_operators(LightLinker::CPUOutOfDateFlags);
enum_class_operators(LightLinker::UpdateFlags);

inline std::string to_string(LightLinker::UpdateFlags flags) {
    if(flags == LightLinker::UpdateFlags::None) return "None";
    if(is_set(flags, LightLinker::UpdateFlags::All)) return "All";
    std::string s;
    if(is_set(flags, LightLinker::UpdateFlags::LightsChanged)) s += "LightsChanged, ";
    if(is_set(flags, LightLinker::UpdateFlags::LightSetsChanged)) s += "LightSetsChanged, ";
    if(!s.empty()) s.erase(s.size() - 2);
    return s;
}

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_LIGHTS_LIGH_LINKER_H_