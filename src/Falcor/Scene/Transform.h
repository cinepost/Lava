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
#ifndef FALCOR_SCENE_TRANSFORM_H_
#define FALCOR_SCENE_TRANSFORM_H_

//#include "Falcor.h"
#include "Falcor/Core/Framework.h"

namespace Falcor {

/** Helper to create transformation matrices based on translation,
    rotation and scaling.
*/
class dlldecl Transform {
  public:
    Transform();

    const float3& getTranslation() const { return mTranslation; }
    void setTranslation(const float3& translation);

    const float3& getScaling() const { return mScaling; }
    void setScaling(const float3& scaling);

    const glm::quat& getRotation() const { return mRotation; }
    void setRotation(const glm::quat& rotation);

    float3 getRotationEuler() const;
    void setRotationEuler(const float3& angles);

    float3 getRotationEulerDeg() const;
    void setRotationEulerDeg(const float3& angles);

    void lookAt(const float3& position, const float3& target, const float3& up);

    const glm::float4x4& getMatrix() const;

    bool operator==(const Transform& other) const;
    bool operator!=(const Transform& other) const { return !((*this) == other); }

  private:
    float3 mTranslation = float3(0.f);
    float3 mScaling = float3(1.f);
    glm::quat mRotation = glm::identity<glm::quat>();

    mutable bool mDirty = true;
    mutable glm::float4x4 mMatrix;

    friend class SceneCache;
};

}  // namespace Falcor

#endif  // FALCOR_SCENE_TRANSFORM_H_
