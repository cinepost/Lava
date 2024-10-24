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
#ifndef SRC_FALCOR_SCENE_CAMERA_CAMERA_H_
#define SRC_FALCOR_SCENE_CAMERA_CAMERA_H_

#include "CameraData.slang"

#include "Falcor/Core/Framework.h" 
#include "Falcor/Scene/Animation/Animatable.h"
#include "Falcor/Utils/SampleGenerators/CPUSampleGenerator.h"
#include "Falcor/Core/API/ParameterBlock.h"

namespace Falcor {

struct AABB;
class ParameterBlock;

/** Camera class. Default transform matrices are interpreted as left eye transform during stereo rendering.
*/
class dlldecl Camera : public Animatable
{
public:
    using SharedPtr = std::shared_ptr<Camera>;
    using SharedConstPtr = std::shared_ptr<const Camera>;

    // Default dimensions of full frame cameras and 35mm film
    static const float kDefaultFrameHeight;

    /** Create a new camera object.
    */
    static SharedPtr create();
    ~Camera();

    /** Name the camera.
    */
    void setName(const std::string& name) { mName = name; }

    /** Get the camera's name.
    */
    const std::string& getName() const { return mName; }

    /** Set the camera's aspect ratio.
    */
    void setAspectRatio(float aspectRatio) { mData.aspectRatio = aspectRatio; mDirty = true; }

    /** Get the camera's aspect ratio.
    */
    inline float getAspectRatio() const { return mData.aspectRatio; }

    /** Set camera focal length in mm. See FalcorMath.h for helper functions to convert between fovY angles.
    */
    void setFocalLength(float length) { mData.focalLength = length; mDirty = true; }

    /** Get the camera's focal length. See FalcorMath.h for helper functions to convert between fovY angles.
    */
    inline float getFocalLength() const { return mData.focalLength; }

    /** Set the camera's normalized crop region box.
    */
    void setCropRegion(float4 crop) {  mData.cropRegion = crop; mDirty = true; }

    /** Get the camera's normalized crop region box.
    */
    inline float4 getCropRegion() const { return mData.cropRegion; }

    /** Set the camera's film plane height in mm.
    */
    void setFrameHeight(float height) { mData.frameHeight = height; mPreserveHeight = true;  mDirty = true; }

    /** Get the camera's film plane height in mm.
    */
    inline float getFrameHeight() const { return mData.frameHeight; }

    /** Set the camera's film plane width in mm.
    */
    void setFrameWidth(float width) { mData.frameWidth = width; mPreserveHeight = false;  mDirty = true; }

    /** Get the camera's film plane width in mm.
    */
    inline float getFrameWidth() const { return mData.frameWidth; }

    /** Set the camera's focal distance in scene units.  Used for depth-of-field.
    */
    void setFocalDistance(float distance) { mData.focalDistance = distance; mDirty = true; }

    /** Get the camera's focal distance in scene units.
    */
    inline float getFocalDistance() const { return mData.focalDistance; }

    /** Set camera aperture radius in scene units. See FalcorMath.h for helper functions to convert between aperture f-number.
    */
    void setApertureRadius(float radius) { mData.apertureRadius = radius; mDirty = true; }

    /** Get camera aperture radius in scene units. See FalcorMath.h for helper functions to convert between aperture f-number.
    */
    inline float getApertureRadius() const { return mData.apertureRadius; }

    /** Set camera shutter speed in seconds.
    */
    void setShutterSpeed(float shutterSpeed) { mData.shutterSpeed = shutterSpeed; mDirty = true; }

    /** Get camera shutter speed in seconds.
    */
    inline float getShutterSpeed() const { return mData.shutterSpeed; }

    /** Set camera's film speed based on ISO standards.
    */
    void setISOSpeed(float ISOSpeed) { mData.ISOSpeed = ISOSpeed; mDirty = true; }

    /** Get camera's film speed based on ISO standards.
    */
    inline float getISOSpeed() const { return mData.ISOSpeed; }

    /** Get the camera's world space position.
    */
    inline const float3& getPosition() const { return mData.posW; }

    /** Get the camera's world space up vector.
    */
    inline const float3& getUpVector() const {return mData.up;}

    /** Get the camera's world space target position.
    */
    inline const float3& getTarget() const { return mData.target; }

    /** Set the camera's world space position.
    */
    void setPosition(const float3& posW) { mData.posW = posW; mDirty = true; }

    /** Set the camera's world space up vector.
    */
    void setUpVector(const float3& up) { mData.up = up; mDirty = true; }

    /** Set the camera's world space target position.
    */
    void setTarget(const float3& target) { mData.target = target; mDirty = true; }

    /** Set the camera's depth range.
    */
    void setDepthRange(float nearZ, float farZ) { mData.farZ = farZ;  mData.nearZ = nearZ; mDirty = true; }

    /** Set the near plane depth.
    */
    void setNearPlane(float nearZ) { mData.nearZ = nearZ; mDirty = true; }

    /** Get the near plane depth.
    */
    inline float getNearPlane() const { return mData.nearZ; }

    /** Set the far plane depth.
    */
    void setFarPlane(float farZ) { mData.farZ = farZ; mDirty = true; }

    /** Get the far plane depth.
    */
    inline float getFarPlane() const { return mData.farZ; }

    /** Set a pattern generator. If a generator is set, then a jitter will be set every frame based on the generator
    */
    void setPatternGenerator(const CPUSampleGenerator::SharedPtr& pGenerator, const float2& scale = float2(1.f));

    /** Set/Get camera film background image file name.
    */
    void setBackgroundImageFilename(const std::string& filename);
    const std::string& getBackgroundImageFilename() const { return mBackgroundImageFilename; }

    /** Set camera film background color.
    */
    void setBackgroundColor(const float4& color = float4(0.f));

    /** Get camera film background color.
    */
    const float4& getBackgroundColor() const { return mData.backgroundColor; }

    /** Get the bound pattern generator
    */
    const CPUSampleGenerator::SharedPtr& getPatternGenerator() const { return mJitterPattern.pGenerator; }

    float2 getPatternGeneratorScale() const { return mJitterPattern.scale; }

    /** Set the camera's jitter.
        \param[in] jitterX Subpixel offset along X axis divided by screen width (positive value shifts the image right).
        \param[in] jitterY Subpixel offset along Y axis divided by screen height (positive value shifts the image up).
    */
    void setJitter(float jitterX, float jitterY);
    
    /** Set the camera's jitter.
        \param[in] jitter Subpixel offset along X and Y axis divided by screen width (positive value shifts the image right).
    */
    void setJitter(float2 jitter);

    float getJitterX() const { return mData.jitterX; }
    float getJitterY() const { return mData.jitterY; }

    /** Get camera subpixel jitter in [-0.5, 0.5] range both directions.
    */
    float2 getSubpixelJitter() const { return {getSubpixelJitterX(), getSubpixelJitterY()}; }
    float  getSubpixelJitterX() const { return mData.jitterX / mJitterPattern.scale[0]; }
    float  getSubpixelJitterY() const { return mData.jitterY / mJitterPattern.scale[1]; }

    /** Compute pixel spread in screen space -- to be used with RayCones for texture level-of-detail.
        \param[in] winHeightPixels Window height in pixels
        \return the pixel spread angle in screen space
    */
    float computeScreenSpacePixelSpreadAngle(const uint32_t winHeightPixels) const;

    /** Get the view matrix.
    */
    const glm::mat4& getViewMatrix() const;

    /** Get the previous frame view matrix, which possibly includes the previous frame's camera jitter.
    */
    const glm::mat4& getPrevViewMatrix() const;

    /** Get the projection matrix.
    */
    const glm::mat4& getProjMatrix() const;

    /** Get the inverse projection matrix.
    */
    const glm::mat4& getInvProjMatrix() const;

    /** Get the view-projection matrix.
    */
    const glm::mat4& getViewProjMatrix() const;

    /** Get the inverse of the view-projection matrix.
    */
    const glm::mat4& getInvViewProjMatrix() const;

    /** Set the persistent projection matrix and sets camera to use the persistent matrix instead of calculating the matrix from its other settings.
    */
    void setProjectionMatrix(const glm::mat4& proj);

    /** Set the persistent view matrix and sets camera to use the persistent matrix instead of calculating the matrix from its other settings.
    */
    void setViewMatrix(const glm::mat4& view);

    /** Enable or disable usage of persistent projection matrix
        \param[in] persistent whether to set it persistent
    */
    void togglePersistentProjectionMatrix(bool persistent);
    void togglePersistentViewMatrix(bool persistent);

    /** Check if an object should be culled
        \param[in] box Bounding box of the object to check
    */
    bool isObjectCulled(const AABB& box) const;

    /** Set the camera into a shader var
    */
    void setShaderData(const ShaderVar& var) const;

    /** Returns the raw camera data
    */
    const CameraData& getData() const { calculateCameraParameters(); return  mData; }

    void updateFromAnimation(const glm::mat4& transform) override;

    enum class Changes {
        None            = 0x0,
        Movement        = 0x1,
        Exposure        = 0x2,
        FocalDistance   = 0x4,
        Jitter          = 0x8,
        Frustum         = 0x10,
        Aperture        = 0x20,
        History         = 0x40, ///< The previous frame matrix changed. This indicates that the camera motion-vectors changed
    };

    /** Begin frame. Should be called once at the start of each frame.
        This is where we store the previous frame matrices.
        \param[in] firstFrame Set to true on the first frame or after switching cameras.
    */
    Changes beginFrame(bool firstFrame = false);

    /** Get the camera changes that happened in since the previous frame
    */
    Changes getChanges() const { return mChanges; }

    std::string getScript(const std::string& cameraVar);

private:
    Camera();
    Changes mChanges = Changes::None;

    mutable bool mDirty = true;
    mutable bool mEnablePersistentProjMat = false;
    mutable bool mEnablePersistentViewMat = false;
    mutable glm::mat4 mPersistentProjMat;
    mutable glm::mat4 mPersistentViewMat;

    std::string mName;
    std::string mBackgroundImageFilename;
    
    bool mPreserveHeight = true;    ///< If true, preserve frame height on change of aspect ratio. Otherwise, preserve width.

    void calculateCameraParameters() const;
    mutable CameraData mData;
    CameraData mPrevData;

    struct {
        float3 xyz;     ///< Camera frustum plane position
        float negW;     ///< Camera frustum plane, sign of the coordinates
        float3 sign;    ///< Camera frustum plane position
    } mutable mFrustumPlanes[6];

    struct {
        CPUSampleGenerator::SharedPtr pGenerator;
        float2 scale;
    } mJitterPattern;

    void setJitterInternal(float jitterX, float jitterY);

    friend class SceneBuilder;
    friend class SceneCache;
};

enum_class_operators(Camera::Changes);

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_CAMERA_CAMERA_H_
