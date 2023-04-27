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
#ifndef SRC_FALCOR_SCENE_LIGHTS_LIGHT_H_ 
#define SRC_FALCOR_SCENE_LIGHTS_LIGHT_H_

#include <memory>
#include <string>


#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Utils/Math/Vector.h"

#include "Falcor/Utils/HostDeviceShared.slangh"
#include "LightData.slang"
#include "Falcor/Scene/Animation/Animatable.h"

namespace Falcor {

class ShaderVar;
class Scene;

/** Base class for light sources. All light sources should inherit from this.
*/
class dlldecl Light : public Animatable {
  public:
    using SharedPtr = std::shared_ptr<Light>;
    using SharedConstPtr = std::shared_ptr<const Light>;

    virtual ~Light() = default;

    /** Set the light parameters into a shader variable. To use this you need to include/import 'ShaderCommon' inside your shader.
    */
    virtual void setShaderData(const ShaderVar& var);

    /** Get total light power
    */
    virtual float getPower() const = 0;

    /** Get the light type
    */
    LightType getType() const { return (LightType)mData.type; }

    /** Get the light data
    */
    inline const LightData& getData() const { return mData; }

    /** Name the light
    */
    void setName(const std::string& Name) { mName = Name; }

    /** Get the light's name
    */
    const std::string& getName() const { return mName; }

    /** Activate/deactivate the light
    */
    void setActive(bool active);

    /** Check if light is active
    */
    bool isActive() const { return mActive; }

    /** Set light projection texture
    */
    void setTexture(Texture::SharedPtr pTexture);

    /** Get light projection texture
    */
    Texture::SharedPtr getTexture() const { return mpTexture; } 

    /** Gets the size of a single light data struct in bytes
    */
    static uint32_t getShaderStructSize() { return kDataSize; }

    /** Set the light diffuse intensity.
    */
    virtual void setDiffuseIntensity(const float3& intensity);

    /** Set the light specular intensity.
    */
    virtual void setSpecularIntensity(const float3& intensity);

    /** Set the light indirect diffuse intensity.
    */
    virtual void setIndirectDiffuseIntensity(const float3& intensity);

    /** Set the light indirect specular intensity.
    */
    virtual void setIndirectSpecularIntensity(const float3& intensity);

    /** Get the light diffuse intensity.
    */
    float3 getDiffuseIntensity() const { return (float3)mData.directDiffuseIntensity; }

    /** Get the light specular intensity.
    */
    float3 getSpecularIntensity() const { return (float3)mData.directSpecularIntensity; }

    /** Get the light indirect diffuse intensity.
    */
    float3 getIndirectDiffuseIntensity() const { return (float3)mData.indirectDiffuseIntensity; }

    /** Get the light indirect specular intensity.
    */
    float3 getIndirectSpecularIntensity() const { return (float3)mData.indirectSpecularIntensity; }


    /** Set the light shadow color
    */
    void setShadowColor(const float3& shadowColor);

    /** Get the light shadow color
    */
    float3 getShadowColor() const { return (float3)mData.shadowColor; }

    void setShadowType(LightShadowType shadowType);

    LightShadowType getShadowType() const { return (LightShadowType)mData.shadowType; }

    void setLightRadius(float radius);

    const float getLightRadius() const { return mData.radius; }

    enum class Changes {
      None = 0x0,
      Active = 0x1,
      Position = 0x2,
      Direction = 0x4,
      Intensity = 0x8,
      SurfaceArea = 0x10,
      Shadow = 0x20,
      Flags = 0x40,
    };

    /** Begin a new frame. Returns the changes from the previous frame
    */
    Changes beginFrame();

    /** Returns the changes from the previous frame
    */
    Changes getChanges() const { return mChanges; }

    void updateFromAnimation(const glm::mat4& transform) override {}

  protected:
    virtual void update();
    Light(const std::string& name, LightType type);

    static const size_t kDataSize = sizeof(LightData);

    std::string mName;
    bool mActive = true;
    bool mActiveChanged = false;

    Texture::SharedPtr mpTexture = nullptr;

    LightData mData, mPrevData;
    Changes mChanges = Changes::None;

    friend class SceneCache;
};

/** Point light source.
    Simple infinitely-small point light with quadratic attenuation.
*/
class dlldecl PointLight : public Light {
  public:
    using SharedPtr = std::shared_ptr<PointLight>;
    using SharedConstPtr = std::shared_ptr<const PointLight>;

    static SharedPtr create(const std::string& name = "");
    ~PointLight() = default;

    /** Get total light power (needed for light picking)
    */
    float getPower() const override;

    /** Set the light's world-space position
    */
    void setWorldPosition(const float3& pos);

    /** Set the light's world-space direction.
        \param[in] dir Light direction. Does not have to be normalized.
    */
    void setWorldDirection(const float3& dir);

    /** Set the cone opening angle for use as a spot light
        \param[in] openingAngle Angle in radians.
    */
    void setOpeningAngle(float openingAngle);

  /** Set the cone opening half-angle for use as a spot light
        \param[in] openingAngle Angle in radians.
    */
    void setOpeningHalfAngle(float openingAngle);

    /** Get the light's world-space position
    */
    const float3& getWorldPosition() const { return mData.posW; }

    /** Get the light's world-space direction
    */
    const float3& getWorldDirection() const { return mData.dirW; }

    /** Get the penumbra half-angle
    */
    float getPenumbraAngle() const { return mData.penumbraAngle; }

    /** Set the penumbra half-angle
        \param[in] angle Angle in radians
    */
    void setPenumbraHalfAngle(float angle);

    /** Set the penumbra angle
        \param[in] angle Angle in radians
    */
    void setPenumbraAngle(float angle);

    /** Get the cone opening half-angle
    */
    float getOpeningAngle() const { return mData.openingAngle; }

    void updateFromAnimation(const glm::mat4& transform) override;

  private:
    virtual void update() override;
    PointLight(const std::string& name);
};


/** Directional light source.
*/
class dlldecl DirectionalLight : public Light {
  public:
    using SharedPtr = std::shared_ptr<DirectionalLight>;
    using SharedConstPtr = std::shared_ptr<const DirectionalLight>;

    static SharedPtr create(const std::string& name = "");
    ~DirectionalLight() = default;

    /** Set the light's world-space direction.
        \param[in] dir Light direction. Does not have to be normalized.
    */
    void setWorldDirection(const float3& dir);

    /** Set the scene parameters
    */
    void setWorldParams(const float3& center, float radius);

    /** Get the light's world-space direction.
    */
    const float3& getWorldDirection() const { return mData.dirW; }

    /** Get total light power (needed for light picking)
    */
    float getPower() const override { return 0.f; }

    void updateFromAnimation(const glm::mat4& transform) override;

  private:
    DirectionalLight(const std::string& name);
};

/** Distant light source.
    Same as directional light source but subtending a non-zero solid angle.
*/
class dlldecl DistantLight : public Light {
  public:
    using SharedPtr = std::shared_ptr<DistantLight>;
    using SharedConstPtr = std::shared_ptr<const DistantLight>;

    static SharedPtr create(const std::string& name = "");
    ~DistantLight() = default;

    /** Set the half-angle subtended by the light
        \param[in] theta Light angle
    */
    void setAngle(float theta);
    void setAngleDegrees(float deg);

    /** Get the half-angle subtended by the light
    */
    float getAngle() const { return mAngle; }

    /** Set the light's world-space direction.
        \param[in] dir Light direction. Does not have to be normalized.
    */
    void setWorldDirection(const float3& dir);

    /** Get the light's world-space direction.
    */
    const float3& getWorldDirection() const { return mData.dirW; }

    /** Get total light power
    */
    float getPower() const override { return 0.f; }

    void updateFromAnimation(const glm::mat4& transform) override;

    /** Set the light diffuse intensity.
    */
    virtual void setDiffuseIntensity(const float3& intensity) override;

    /** Set the light specular intensity.
    */
    virtual void setSpecularIntensity(const float3& intensity) override;

    /** Set the light indirect diffuse intensity.
    */
    virtual void setIndirectDiffuseIntensity(const float3& intensity) override;

    /** Set the light indirect specular intensity.
    */
    virtual void setIndirectSpecularIntensity(const float3& intensity) override;

  private:
    DistantLight(const std::string& name);
    virtual void update();
    float mAngle;       ///<< Half-angle subtended by the source.
    float3 mDiffuseIntensity;
    float3 mSpecularIntensity;
    float3 mIndirectDiffuseIntensity;
    float3 mIndirectSpecularIntensity;

    friend class SceneCache;
};

/** Environment light source.
*/

class dlldecl EnvironmentLight: public Light {
  public:
    using SharedPtr = std::shared_ptr<EnvironmentLight>;
    using SharedConstPtr = std::shared_ptr<const EnvironmentLight>;

    static SharedPtr create(const std::string& name = "", Texture::SharedPtr pTexture = nullptr);
    ~EnvironmentLight() = default;

    /** Get total light power
    */
    float getPower() const override;

    void updateFromAnimation(const glm::mat4& transform) override;

    /** Set the light diffuse intensity.
    */
    virtual void setDiffuseIntensity(const float3& intensity) override;

    /** Set the light specular intensity.
    */
    virtual void setSpecularIntensity(const float3& intensity) override;

    /** Set the light indirect diffuse intensity.
    */
    virtual void setIndirectDiffuseIntensity(const float3& intensity) override;

    /** Set the light indirect specular intensity.
    */
    virtual void setIndirectSpecularIntensity(const float3& intensity) override;

  private:
    virtual void update();

    EnvironmentLight(const std::string& name, Texture::SharedPtr pTexture);

    friend class SceneCache;  
};

/** Analytic area light source.
*/
class dlldecl AnalyticAreaLight : public Light {
  public:
    using SharedPtr = std::shared_ptr<AnalyticAreaLight>;
    using SharedConstPtr = std::shared_ptr<const AnalyticAreaLight>;

    enum class LightSamplingMode {
      MONTE_CARLO,
      SOLID_ANGLE,
      SPHERICAL_SOLID_ANGLE,
      LTC,
    };

    ~AnalyticAreaLight() = default;

    /** Set light source scaling
        \param[in] scale x,y,z scaling factors
    */
    void setScaling(float3 scale);

    /** Set light source scale
      */
    inline float3 getScaling() const { return mScaling; }

    /** Get total light power (needed for light picking)
    */
    float getPower() const override;

    /** Set transform matrix
        \param[in] mtx object to world space transform matrix
    */
    void setTransformMatrix(const glm::mat4& mtx) { mTransformMatrix = mtx; update();  }

    void setSingleSided(bool value);

    inline bool isSingleSided() const { return mData.isSingleSided(); }

    void setNormalizeArea(bool value) { mNormalizeArea = value; update(); }

    inline bool isAreaNormalized() const { return mNormalizeArea; }

    /** Get transform matrix
    */
    inline glm::mat4 getTransformMatrix() const { return mTransformMatrix; }

    void updateFromAnimation(const glm::mat4& transform) override { setTransformMatrix(transform); }

    /** Set the light diffuse intensity.
    */
    virtual void setDiffuseIntensity(const float3& intensity) override;

    /** Set the light specular intensity.
    */
    virtual void setSpecularIntensity(const float3& intensity) override;

    /** Set the light indirect diffuse intensity.
    */
    virtual void setIndirectDiffuseIntensity(const float3& intensity) override;

    /** Set the light indirect specular intensity.
    */
    virtual void setIndirectSpecularIntensity(const float3& intensity) override;

  protected:
    AnalyticAreaLight(const std::string& name, LightType type);

    virtual void update();

    float3 mScaling;                ///< Scaling, controls the size of the light
    glm::mat4 mTransformMatrix;     ///< Transform matrix minus scaling component
    bool mNormalizeArea = false;    ///< Normalize light area

    friend class SceneCache;
};

/** Rectangular area light source.
*/
class dlldecl RectLight : public AnalyticAreaLight {
  public:
    using SharedPtr = std::shared_ptr<RectLight>;
    using SharedConstPtr = std::shared_ptr<const RectLight>;

    static SharedPtr create(const std::string& name = "");
    ~RectLight() = default;

  private:
    RectLight(const std::string& name) : AnalyticAreaLight(name, LightType::Rect) {}

    virtual void update();
};

/** Disc area light source.
*/
class dlldecl DiscLight : public AnalyticAreaLight {
  public:
    using SharedPtr = std::shared_ptr<DiscLight>;
    using SharedConstPtr = std::shared_ptr<const DiscLight>;

    static SharedPtr create(const std::string& name = "");
    ~DiscLight() = default;

  private:
    DiscLight(const std::string& name) : AnalyticAreaLight(name, LightType::Disc) {}

    virtual void update() override;
};

/** Sphere area light source.
*/
class dlldecl SphereLight : public AnalyticAreaLight {
  public:
    using SharedPtr = std::shared_ptr<SphereLight>;
    using SharedConstPtr = std::shared_ptr<const SphereLight>;

    static SharedPtr create(const std::string& name = "");
    ~SphereLight() = default;

private:
    SphereLight(const std::string& name) : AnalyticAreaLight(name, LightType::Sphere) {}

    virtual void update() override;
};

enum_class_operators(Light::Changes);

inline std::string to_string(LightType lt) {
#define t2s(t_) case LightType::t_: return #t_;
    switch (lt) {
        t2s(Point);
        t2s(Directional);
        t2s(Distant);
        t2s(Rect);
        t2s(Disc);
        t2s(Sphere);
        t2s(Env);
        default:
            should_not_get_here();
            return "";
    }
#undef t2s
}

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_LIGHTS_LIGHT_H_
