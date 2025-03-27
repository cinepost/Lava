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
#ifndef SRC_FALCOR_SCENE_LIGHTS_ENVMAP_H_ 
#define SRC_FALCOR_SCENE_LIGHTS_ENVMAP_H_

#include "Falcor/Core/Macros.h"
#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/Sampler.h"

#include "Falcor/Core/Program/ShaderVar.h"

#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;

#include "EnvMapData.slang"


namespace Falcor {

/** Environment map based radiance probe.
    Utily class for evaluating radiance stored in an lat-long environment map.
*/
class dlldecl EnvMap : public Object {
    FALCOR_OBJECT(EnvMap)
  public:
    virtual ~EnvMap() = default;

    /** Create a new object.
        \param[in] texture The environment map texture.
    */
    static ref<EnvMap> create(ref<Device> pDevice, const ref<Texture>& texture);

    /** Create a new object.
        \param[in] filename The environment map texture filename.
    */
    static ref<EnvMap> createFromFile(ref<Device> pDevice, const fs::path& path);

    /** Set rotation angles.
        Rotation is applied as rotation around Z, Y and X axes, in that order.
        Note that glm::extractEulerAngleXYZ() may be used to extract these angles from
        a transformation matrix.
        \param[in] degreesXYZ Rotation angles in degrees for XYZ.
    */
    void setRotation(float3 degreesXYZ);
    void setTransform(const float4x4& matrix);

    /** Get rotation angles.
    */
    float3 getRotation() const { return mRotation; }

    /** Set intensity (scalar multiplier).
    */
    void setIntensity(float intensity);

    /** Set color tint (rgb multiplier).
    */
    void setTint(const float3& tint);

    /** Get intensity.
    */
    inline float getIntensity() const { return mData.intensity; }

    /** Set envmap visibility
    */
    void setPhantom(bool phantom);

    /** Get envmap visibility
    */
    inline bool isPhantom() const { return mPhantom; }

    /** Get color tint.
    */
    float3 getTint() const { return mData.tint; }

    /** Get the filename of the environment map texture.
    */
    const fs::path& getPath() const { return mpEnvMap->getSourcePath(); }

    const ref<Texture>& getEnvMap() const { return mpEnvMap; }
    const ref<Texture>& getTexture() const { return mpEnvMap; }
    const ref<Sampler>& getEnvSampler() const { return mpEnvSampler; }

    /** Bind the environment map to a given shader variable.
        \param[in] var Shader variable.
    */
    void setShaderData(const ShaderVar& var) const;

    enum class Changes {
        None            = 0x0,
        Transform       = 0x1,
        Intensity       = 0x2,
    };

    /** Begin frame. Should be called once at the start of each frame.
    */
    Changes beginFrame();

    /** Get the environment map changes that happened in since the previous frame.
    */
    Changes getChanges() const { return mChanges; }

    /** Get the total GPU memory usage in bytes.
    */
    uint64_t getMemoryUsageInBytes() const;

  protected:
    EnvMap(ref<Device> pDevice, const ref<Texture>& texture);

    ref<Device>             mpDevice;
    ref<Texture>            mpEnvMap;           ///< Loaded environment map (RGB).
    ref<Sampler>            mpEnvSampler;       ///< Texture sampler for the environment map.

    EnvMapData              mData;
    EnvMapData              mPrevData;

    bool                    mPhantom = true;

    float3                  mRotation = { 0.f, 0.f, 0.f };

    Changes                 mChanges = Changes::None;

    friend class Scene;
    friend class SceneCache;
};

ENUM_CLASS_OPERATORS(EnvMap::Changes);

} // namespace Falcor

#endif  // SRC_FALCOR_SCENE_LIGHTS_ENVMAP_H_