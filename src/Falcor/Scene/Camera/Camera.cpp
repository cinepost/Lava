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
#include "stdafx.h"

#include <stdlib.h>

#include "Falcor/Utils/Math/AABB.h"
#include "Falcor/Utils/Math/FalcorMath.h"
#include "glm/gtc/type_ptr.hpp"

#include "Camera.h"

namespace Falcor {

namespace {
	const std::string kAnimated = "animated";
	const std::string kPosition = "position";
	const std::string kTarget = "target";
	const std::string kUp = "up";
}

static_assert(sizeof(CameraData) % (sizeof(float4)) == 0, "CameraData size should be a multiple of 16");

constexpr float M_2PI = 2.0f * M_PI;

// Default dimensions of full frame cameras and 35mm film
const float Camera::kDefaultFrameHeight = 24.0f;

Camera::Camera() {}

Camera::~Camera() = default;

Camera::SharedPtr Camera::create() {
	Camera* pCamera = new Camera;
	return SharedPtr(pCamera);
}

Camera::Changes Camera::beginFrame(bool firstFrame) {
	if (mJitterPattern.pGenerator) {
		float2 jitter = mJitterPattern.pGenerator->next();
		jitter *= mJitterPattern.scale;
		setJitterInternal(jitter.x, jitter.y);
	}

	calculateCameraParameters();

	if (firstFrame) mPrevData = mData;

	// Keep copies of the transforms used for the previous frame. We need these for computing motion vectors etc.
	mData.prevViewMat = mPrevData.viewMat;
	mData.prevViewProjMatNoJitter = mPrevData.viewProjMatNoJitter;
	mData.prevPosW = mPrevData.posW;

	mChanges = is_set(mChanges, Changes::Movement | Changes::Frustum) ? Changes::History : Changes::None;

	if (mPrevData.posW != mData.posW) mChanges |= Changes::Movement;
	if (mPrevData.up != mData.up) mChanges |= Changes::Movement;
	if (mPrevData.target != mData.target) mChanges |= Changes::Movement;
	if (mPrevData.viewMat != mData.viewMat) mChanges |= Changes::Movement;

	if (mPrevData.focalDistance != mData.focalDistance) mChanges    |= Changes::FocalDistance;
	if (mPrevData.apertureRadius != mData.apertureRadius) mChanges  |= Changes::Aperture | Changes::Exposure;
	if (mPrevData.shutterSpeed != mData.shutterSpeed) mChanges      |= Changes::Exposure;
	if (mPrevData.ISOSpeed != mData.ISOSpeed) mChanges              |= Changes::Exposure;

	if (mPrevData.focalLength != mData.focalLength) mChanges |= Changes::Frustum;
	if (mPrevData.aspectRatio != mData.aspectRatio) mChanges |= Changes::Frustum;
	if (mPrevData.nearZ != mData.nearZ)             mChanges |= Changes::Frustum;
	if (mPrevData.farZ != mData.farZ)               mChanges |= Changes::Frustum;
	if (mPrevData.frameHeight != mData.frameHeight) mChanges |= Changes::Frustum;
	if (mPrevData.frameWidth != mData.frameWidth)   mChanges |= Changes::Frustum;
	if (mPrevData.cropRegion != mData.cropRegion)   mChanges |= Changes::Frustum;
	if (mPrevData.projMat != mData.projMat)         mChanges |= Changes::Frustum;

	// Jitter
	if (mPrevData.jitterX != mData.jitterX) mChanges |= Changes::Jitter;
	if (mPrevData.jitterY != mData.jitterY) mChanges |= Changes::Jitter;

	mPrevData = mData;

	return getChanges();
}

void Camera::calculateCameraParameters() const {
	if (mDirty) {

		if (mPreserveHeight) {
			// Set frame width based on height and aspect ratio
			mData.frameWidth = mData.frameHeight * mData.aspectRatio;
		} else {
			// Set frame height based on width and aspect ratio
			mData.frameHeight = mData.frameWidth / mData.aspectRatio;
		}

		// Interpret focal length of 0 as 0 FOV. Technically 0 FOV should be focal length of infinity.
		const float fovY = mData.focalLength == 0.0f ? 0.0f : focalLengthToFovY(mData.focalLength, mData.frameHeight);

		if (mEnablePersistentViewMat) {
			mData.viewMat = mPersistentViewMat;
			auto viewInvMat = glm::inverse(mPersistentViewMat);

			// set camera word position from our persistent view matrix
			mData.posW = {viewInvMat[3][0], viewInvMat[3][1], viewInvMat[3][2]};
			mData.target = mData.posW - glm::normalize(float3({mData.viewMat[0][2], mData.viewMat[1][2], mData.viewMat[2][2]}));
			mData.up = glm::normalize(float3({mData.viewMat[0][1], mData.viewMat[1][1], mData.viewMat[2][1]}));
		} else {
			mData.viewMat = glm::lookAt(mData.posW, mData.target, mData.up);
		}

		// if camera projection is set to be persistent, don't override it.
		if (mEnablePersistentProjMat) {
			mData.projMat = mPersistentProjMat;
		} else {
			if (fovY != 0.f) {
				float left   = ((mData.cropRegion[0]-.5f) / mData.focalLength) * (mData.nearZ * mData.frameWidth);
				float right  = ((mData.cropRegion[2]-.5f) / mData.focalLength) * (mData.nearZ * mData.frameWidth);
				float top    = ((mData.cropRegion[1]-.5f) / mData.focalLength) * (mData.nearZ * -mData.frameHeight);
				float bottom = ((mData.cropRegion[3]-.5f) / mData.focalLength) * (mData.nearZ * -mData.frameHeight);
				mData.projMat = glm::frustum(left, right, bottom, top, mData.nearZ, mData.farZ);
			} else {
				// Take the length of look-at vector as half a viewport size
				const float halfLookAtLength = length(mData.posW - mData.target) * 0.5f;
				mData.projMat = glm::ortho(-halfLookAtLength, halfLookAtLength, -halfLookAtLength, halfLookAtLength, mData.nearZ, mData.farZ);
			}
		}

		// Build jitter matrix
		// (jitterX and jitterY are expressed as subpixel quantities divided by the screen resolution
		//  for instance to apply an offset of half pixel along the X axis we set jitterX = 0.5f / Width)
		glm::mat4 jitterMat(1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			2.0f * mData.jitterX, 2.0f * mData.jitterY, 0.0f, 1.0f);
		// Apply jitter matrix to the projection matrix
		mData.viewProjMatNoJitter = mData.projMat * mData.viewMat;
		mData.projMatNoJitter = mData.projMat;
		mData.projMat = jitterMat * mData.projMat;

		// DOF matrix
		{
			float R = .5f;
			float r = R * sqrt(static_cast <float>(rand()) / static_cast<float>(RAND_MAX));
			float theta = (static_cast <float>(rand()) / static_cast<float>(RAND_MAX)) * M_2PI;

			float2 p = {(r * cos(theta)) / static_cast <float>(mData.frameWidth), (r * sin(theta)) / static_cast <float>(mData.frameHeight)};

			float left   = ((mData.cropRegion[0]-.5f) / mData.focalLength) * (mData.nearZ * mData.frameWidth);
			float right  = ((mData.cropRegion[2]-.5f) / mData.focalLength) * (mData.nearZ * mData.frameWidth);
			float top    = ((mData.cropRegion[1]-.5f) / mData.focalLength) * (mData.nearZ * -mData.frameHeight);
			float bottom = ((mData.cropRegion[3]-.5f) / mData.focalLength) * (mData.nearZ * -mData.frameHeight);

			float xwsize = right - left;
			float ywsize = top - bottom;
			float focus = 1.0f;//mData.focalDistance;
			float dx = (p.x * mData.nearZ / focus) * 0.0;
   		float dy = (p.y * mData.nearZ / focus) * 0.0;

   		//mData.projMat = glm::frustum(left + dx, right + dx, bottom + dy, top + dy, mData.nearZ, mData.farZ);

   		//mData.projMat[2][0] += dx;
   		//mData.projMat[2][1] += dy;

   		//mData.viewMat[3][0] -= dx * 2.0;
   		//mData.viewMat[3][1] -= dy * 10.0;
   		
   		//mData.viewMat = glm::translate(mData.viewMat, {-p.x, -p.y, 0.0});

   		//mData.projMat[0][0] /= dx;
   		//mData.projMat[1][1] /= dy;

			glm::mat4 dofMat(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				p.x, p.y, 0.0f, 1.0f
			);
			//mData.projMat = dofMat * mData.projMat;

		}

		mData.viewProjMat = mData.projMat * mData.viewMat;
		mData.invViewProj = glm::inverse(mData.viewProjMat);

		// Extract camera space frustum planes from the VP matrix
		// See: https://fgiesen.wordpress.com/2012/08/31/frustum-planes-from-the-projection-matrix/
		glm::mat4 tempMat = glm::transpose(mData.viewProjMat);
		for (int i = 0; i < 6; i++) {
			float4 plane = (i & 1) ? tempMat[i >> 1] : -tempMat[i >> 1];
			if(i != 5) // Z range is [0, w]. For the 0 <= z plane we don't need to add w
			{
				plane += tempMat[3];
			}

			mFrustumPlanes[i].xyz = float3(plane);
			mFrustumPlanes[i].sign = glm::sign(mFrustumPlanes[i].xyz);
			mFrustumPlanes[i].negW = -plane.w;
		}

		// Ray tracing related vectors
		mData.cameraW = glm::normalize(mData.target - mData.posW) * mData.focalDistance;
		mData.cameraU = glm::normalize(glm::cross(mData.cameraW, mData.up));
		mData.cameraV = glm::normalize(glm::cross(mData.cameraU, mData.cameraW));
		const float ulen = mData.focalDistance * std::tan(fovY * 0.5f) * mData.aspectRatio;
		mData.cameraU *= ulen;
		const float vlen = mData.focalDistance * std::tan(fovY * 0.5f);
		mData.cameraV *= vlen;

		mDirty = false;
	}
}

const glm::mat4& Camera::getViewMatrix() const {
	calculateCameraParameters();
	return mData.viewMat;
}

const glm::mat4& Camera::getPrevViewMatrix() const {
	return mData.prevViewMat;
}

const glm::mat4& Camera::getProjMatrix() const {
	calculateCameraParameters();
	return mData.projMat;
}

const glm::mat4& Camera::getViewProjMatrix() const {
	calculateCameraParameters();
	return mData.viewProjMat;
}

const glm::mat4& Camera::getInvViewProjMatrix() const {
	calculateCameraParameters();
	return mData.invViewProj;
}

void Camera::setProjectionMatrix(const glm::mat4& proj) {
	mDirty = true;
	mPersistentProjMat = proj;
	togglePersistentProjectionMatrix(true);
}

void Camera::setViewMatrix(const glm::mat4& view) {
	mDirty = true;
	mPersistentViewMat = view;
	togglePersistentViewMatrix(true);
}

void Camera::togglePersistentProjectionMatrix(bool persistent) {
	mEnablePersistentProjMat = persistent;
}

void Camera::togglePersistentViewMatrix(bool persistent) {
	mEnablePersistentViewMat = persistent;
}

bool Camera::isObjectCulled(const AABB& box) const {
	calculateCameraParameters();

	bool isInside = true;
	// AABB vs. frustum test
	// See method 4b: https://fgiesen.wordpress.com/2010/10/17/view-frustum-culling/
	for (int plane = 0; plane < 6; plane++) {
		float3 signedHalfExtent = 0.5f * box.extent() * mFrustumPlanes[plane].sign;
		float dr = glm::dot(box.center() + signedHalfExtent, mFrustumPlanes[plane].xyz);
		isInside = isInside && (dr > mFrustumPlanes[plane].negW);
	}

	return !isInside;
}

void Camera::setShaderData(const ShaderVar& var) const {
	calculateCameraParameters();
	var["data"].setBlob(mData);
}

void Camera::setPatternGenerator(const CPUSampleGenerator::SharedPtr& pGenerator, const float2& scale) {
	mJitterPattern.pGenerator = pGenerator;
	mJitterPattern.scale = scale;
	if (!pGenerator) {
		setJitterInternal(0, 0);
	}
}

void Camera::setJitter(float jitterX, float jitterY) {
	if (mJitterPattern.pGenerator) {
		LLOG_WRN << "Camera::setJitter() called when a pattern-generator object was attached to the camera. Detaching the pattern-generator";
		mJitterPattern.pGenerator = nullptr;
	}
	setJitterInternal(jitterX, jitterY);
}

void Camera::setJitterInternal(float jitterX, float jitterY) {
	mData.jitterX = jitterX;
	mData.jitterY = jitterY;
	mDirty = true;
}

float Camera::computeScreenSpacePixelSpreadAngle(const uint32_t winHeightPixels) const {
	const float FOVrad = focalLengthToFovY(getFocalLength(), Camera::kDefaultFrameHeight);
	const float angle = std::atan(2.0f * std::tan(FOVrad * 0.5f) / winHeightPixels);
	return angle;
}

void Camera::updateFromAnimation(const glm::mat4& transform) {
	float3 up = float3(transform[1]);
	float3 fwd = float3(transform[2]);
	float3 pos = float3(transform[3]);
	setUpVector(up);
	setPosition(pos);
	setTarget(pos + fwd);
}

std::string Camera::getScript(const std::string& cameraVar) {
#ifdef SCRIPTING
	std::string c;

	if (hasAnimation() && !isAnimated()) {
		c += Scripting::makeSetProperty(cameraVar, kAnimated, false);
	}

	if (!hasAnimation() || !isAnimated()) {
		c += Scripting::makeSetProperty(cameraVar, kPosition, getPosition());
		c += Scripting::makeSetProperty(cameraVar, kTarget, getTarget());
		c += Scripting::makeSetProperty(cameraVar, kUp, getUpVector());
	}
	return c;
#else
	return "";
#endif
}

#ifdef SCRIPTING
SCRIPT_BINDING(Camera) {
	pybind11::class_<Camera, Animatable, Camera::SharedPtr> camera(m, "Camera");
	camera.def_property_readonly("name", &Camera::getName);
	camera.def_property("aspectRatio", &Camera::getAspectRatio, &Camera::setAspectRatio);
	camera.def_property("focalLength", &Camera::getFocalLength, &Camera::setFocalLength);
	camera.def_property("cropRegion", &Camera::getCropRegion, &Camera::setCropRegion);
	camera.def_property("frameHeight", &Camera::getFrameHeight, &Camera::setFrameHeight);
	camera.def_property("frameWidth", &Camera::getFrameWidth, &Camera::setFrameWidth);
	camera.def_property("focalDistance", &Camera::getFocalDistance, &Camera::setFocalDistance);
	camera.def_property("apertureRadius", &Camera::getApertureRadius, &Camera::setApertureRadius);
	camera.def_property("shutterSpeed", &Camera::getShutterSpeed, &Camera::setShutterSpeed);
	camera.def_property("ISOSpeed", &Camera::getISOSpeed, &Camera::setISOSpeed);
	camera.def_property("nearPlane", &Camera::getNearPlane, &Camera::setNearPlane);
	camera.def_property("farPlane", &Camera::getFarPlane, &Camera::setFarPlane);
	camera.def_property(kPosition.c_str(), &Camera::getPosition, &Camera::setPosition);
	camera.def_property(kTarget.c_str(), &Camera::getTarget, &Camera::setTarget);
	camera.def_property(kUp.c_str(), &Camera::getUpVector, &Camera::setUpVector);
}
#endif

}  // namespace Falcor
