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

Camera::Camera(): mpDevice(nullptr) {
	mXformList.resize(1);
	mPrevXformList.resize(1);
	mPersistentViewMatList.resize(1);
}

Camera::Camera(const Device::SharedPtr& pDevice): Camera() {
	mpDevice = pDevice;
}

Camera::~Camera() = default;

Camera::SharedPtr Camera::create() {
	Camera* pCamera = new Camera;
	return SharedPtr(pCamera);
}

Camera::SharedPtr Camera::create(const Device::SharedPtr& pDevice) {
	Camera* pCamera = new Camera(pDevice);
	return SharedPtr(pCamera);
}

static Camera::Changes calcCameraDataChanges(Camera::Changes& changes, const CameraData& data, const CameraData& prevData) {
	using Changes = Camera::Changes;

	if (prevData.focalDistance != data.focalDistance) changes    |= Changes::FocalDistance;
	if (prevData.apertureRadius != data.apertureRadius) changes  |= Changes::Aperture | Changes::Exposure;
	if (prevData.shutterSpeed != data.shutterSpeed) changes      |= Changes::Exposure;
	if (prevData.ISOSpeed != data.ISOSpeed) changes              |= Changes::Exposure;

	if (prevData.focalLength != data.focalLength) changes |= Changes::Frustum;
	if (prevData.aspectRatio != data.aspectRatio) changes |= Changes::Frustum;
	if (prevData.nearZ != data.nearZ)             changes |= Changes::Frustum;
	if (prevData.farZ != data.farZ)               changes |= Changes::Frustum;
	if (prevData.frameHeight != data.frameHeight) changes |= Changes::Frustum;
	if (prevData.frameWidth != data.frameWidth)   changes |= Changes::Frustum;
	if (prevData.cropRegion != data.cropRegion)   changes |= Changes::Frustum;
	if (prevData.projMat != data.projMat) changes |= Changes::Frustum;

	// Jitter
	if (prevData.jitterX != data.jitterX) changes |= Changes::Jitter;
	if (prevData.jitterY != data.jitterY) changes |= Changes::Jitter;
}

static Camera::Changes calcCameraXformChanges(Camera::Changes& changes, const CameraXformData& data, const CameraXformData& prevData) {
	using Changes = Camera::Changes;

	if (prevData.cameraU != data.cameraU) changes |= Changes::Movement;
	if (prevData.cameraV != data.cameraV) changes |= Changes::Movement;
	if (prevData.cameraW != data.cameraW) changes |= Changes::Movement;
	if (prevData.viewMat != data.viewMat) changes |= Changes::Movement;
}

Camera::Changes Camera::beginFrame(bool firstFrame) {
	assert(!mXformList.empty() && !mPrevXformList.empty());
	
	if (mJitterPattern.pGenerator) {
		float2 jitter = mJitterPattern.pGenerator->next();
		jitter *= mJitterPattern.scale;
		setJitterInternal(jitter.x, jitter.y);
	}

	calculateCameraParameters();

	if (firstFrame) {
		mPrevData = mData;
		mPrevXformList = mXformList;
	}

	// Keep copies of the transforms used for the previous frame. We need these for computing motion vectors etc.
	mData.prevProjMat = mPrevData.projMat;

	if(!mPrevXformList.empty()) {
		const auto& prevXform = mPrevXformList[0];

		mData.prevViewMat = prevXform.viewMat;
		mData.prevViewProjMat = prevXform.viewProjMat;
	}

	mChanges = is_set(mChanges, Changes::Movement | Changes::Frustum) ? Changes::History : Changes::None;

	calcCameraDataChanges(mChanges, mData, mPrevData);

	if(mXformList.size() == mPrevXformList.size()) {
		for(size_t i = 0; i < mXformList.size(); ++i) {
			calcCameraXformChanges(mChanges, mXformList[i], mPrevXformList[i]);
		}
	} else {
		mChanges |= Changes::Movement;
	}

	mPrevData = mData;
	mPrevXformList = mXformList;

	return getChanges();
}

float3 Camera::getPosition(size_t i) const { 
	if(!mEnablePersistentViewMat) return mPosW;
	calculateCameraParameters();
	assert(i < mXformList.size() && !mXformList.empty());
	auto const& xform = mXformList[i];
	return {xform.viewInvMat[3][0], xform.viewInvMat[3][1], xform.viewInvMat[3][2]};
}

float3 Camera::getUpVector(size_t i) const {
	if(!mEnablePersistentViewMat) return mUp;
	calculateCameraParameters();
	assert(i < mXformList.size() && !mXformList.empty());
	return mXformList[i].cameraV;
}

float3 Camera::getTarget(size_t i) const { 
	if(!mEnablePersistentViewMat) return mTarget;
	calculateCameraParameters();
	assert(i < mXformList.size() && !mXformList.empty()); 
	return mXformList[i].cameraW;
}

void Camera::setPosition(const float3& posW) {
	if(mPosW == posW && !mEnablePersistentViewMat) return;
	togglePersistentViewMatrix(false); 
	mPosW = posW; 
	mDirty = true; 
}

void Camera::setUpVector(const float3& up) {
	if(mUp == up && !mEnablePersistentViewMat) return;
	togglePersistentViewMatrix(false);
	mUp = up; 
	mDirty = true; 
}

void Camera::setTarget(const float3& target) {
	if(mTarget == target && !mEnablePersistentViewMat) return;
	togglePersistentViewMatrix(false);
	mTarget = target; 
	mDirty = true; 
}

void Camera::calculateCameraParameters() const {
	if (!mDirty) return;

	if (mPreserveHeight) {
		// Set frame width based on height and aspect ratio
		mData.frameWidth = mData.frameHeight * mData.aspectRatio;
	} else {
		// Set frame height based on width and aspect ratio
		mData.frameHeight = mData.frameWidth / mData.aspectRatio;
	}

	// Interpret focal length of 0 as 0 FOV. Technically 0 FOV should be focal length of infinity.
	const float fovY = mData.focalLength == 0.0f ? 0.0f : focalLengthToFovY(mData.focalLength, mData.frameHeight);

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
			const float halfLookAtLength = 0.5f;
			mData.projMat = glm::ortho(-halfLookAtLength, halfLookAtLength, -halfLookAtLength, halfLookAtLength, mData.nearZ, mData.farZ);
		}
	}
	mData.invProjMat = glm::inverse(mData.projMat);

	mXformList.resize(std::max(size_t(1), mPersistentViewMatList.size()));

	for(size_t i = 0; i < mPersistentViewMatList.size(); ++i) {
		auto& xform = mXformList[i];

		if (mEnablePersistentViewMat) {
			LLOG_WRN << "Camera persistent view matrix";
			xform.viewMat = mPersistentViewMatList[i];
			// Ray tracing related vectors
			xform.cameraU = glm::normalize(float3(xform.viewMat[0][0], xform.viewMat[1][0], xform.viewMat[2][0])); // up
			xform.cameraV = glm::normalize(float3(xform.viewMat[0][1], xform.viewMat[1][1], xform.viewMat[2][1])); // right
			xform.cameraW = -glm::normalize(float3(xform.viewMat[0][2], xform.viewMat[1][2], xform.viewMat[2][2])); // dir
		} else {
			LLOG_WRN << "Camera view matrix from pos, up, target";
			xform.viewMat = glm::lookAt(mPosW, mTarget, mUp);
			// Ray tracing related vectors
			xform.cameraW = glm::normalize(mTarget - mPosW); // dir
			xform.cameraU = glm::normalize(glm::cross(xform.cameraW, mUp)); // right
			xform.cameraV = glm::normalize(glm::cross(xform.cameraU, xform.cameraW)); // up
		}

		xform.viewInvMat = glm::inverse(xform.viewMat);
		xform.viewProjMat = mData.projMat * xform.viewMat;
		xform.invViewProj = glm::inverse(xform.viewProjMat);

		xform.cameraW *= mData.focalDistance;
		xform.cameraU *= mData.focalDistance * std::tan(fovY * 0.5f) * mData.aspectRatio;
		xform.cameraV *= mData.focalDistance * std::tan(fovY * 0.5f);

	}

	// Build jitter matrix
	// (jitterX and jitterY are expressed as subpixel quantities divided by the screen resolution
	//  for instance to apply an offset of half pixel along the X axis we set jitterX = 0.5f / Width)
	glm::mat4 jitterMat(1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		mData.jitterX, mData.jitterY, 0.0f, 1.0f);

	// DOF matrix
	/*
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

		glm::mat4 dofMat(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			p.x, p.y, 0.0f, 1.0f
		);
	}
	*/

	for(auto const& xform: mXformList) {
		// Extract camera space frustum planes from the VP matrix
		// See: https://fgiesen.wordpress.com/2012/08/31/frustum-planes-from-the-projection-matrix/
		glm::mat4 tempMat = glm::transpose(xform.viewProjMat);
		for (int i = 0; i < 6; i++) {
			float4 plane = (i & 1) ? tempMat[i >> 1] : -tempMat[i >> 1];
			if(i != 5) {
				// Z range is [0, w]. For the 0 <= z plane we don't need to add w
				plane += tempMat[3];
			}

			mFrustumPlanes[i].xyz = float3(plane);
			mFrustumPlanes[i].sign = glm::sign(mFrustumPlanes[i].xyz);
			mFrustumPlanes[i].negW = -plane.w;
		}
	}

	mDirty = false;
}

const glm::mat4& Camera::getViewMatrix() const {
	calculateCameraParameters();
	return mXformList[0].viewMat;
}

const std::vector<glm::mat4> Camera::getViewMatrixList() const {
	calculateCameraParameters();
	std::vector<glm::mat4> list;
	for(auto const& xform: mXformList) list.push_back(xform.viewMat);
	return list;
}

const glm::mat4& Camera::getPrevViewMatrix() const {
	calculateCameraParameters();
	return mData.prevViewMat;
}

const glm::mat4& Camera::getProjMatrix() const {
	calculateCameraParameters();
	return mData.projMat;
}

const glm::mat4& Camera::getInvProjMatrix() const {
	calculateCameraParameters();
	return mData.invProjMat;
}

const glm::mat4& Camera::getViewProjMatrix() const {
	calculateCameraParameters();
	return mXformList[0].viewProjMat;
}

const glm::mat4& Camera::getInvViewProjMatrix() const {
	calculateCameraParameters();
	return mXformList[0].invViewProj;
}

void Camera::setProjectionMatrix(const glm::mat4& proj) {
	if(mPersistentProjMat == proj) return;
	mDirty = true;
	mPersistentProjMat = proj;
	togglePersistentProjectionMatrix(true);
}

void Camera::setViewMatrix(const glm::mat4& view) {
	if(mPersistentViewMatList.size() == 0 && mPersistentViewMatList[0] == view) return;
	if(mPersistentViewMatList.size() != 1) {
		LLOG_WRN << "Trying to set view matrix for camera " << mName << " while multiple matrices already exists !!! Resetting camera view matrices list...";
		mPersistentViewMatList.resize(1);
	}
	mPersistentViewMatList[0] = view;
	togglePersistentViewMatrix(true);
	mDirty = true;
}

void Camera::setViewMatrixList(const std::vector<glm::mat4>& views) {
	assert(!views.empty());
	if(views.empty()) {
		LLOG_ERR << "Empty views list provided for camera " << mName << ". No changes applied ...";
		return;
	}
	if(mPersistentViewMatList == views) return;
	mPersistentViewMatList = views;
	togglePersistentViewMatrix(true);
	mDirty = true;
}

void Camera::togglePersistentProjectionMatrix(bool persistent) {
	if(mEnablePersistentProjMat == persistent) return;
	mEnablePersistentProjMat = persistent;
	mDirty = true;
}

void Camera::togglePersistentViewMatrix(bool persistent) {
	if(mEnablePersistentViewMat == persistent) return;
	mEnablePersistentViewMat = persistent;
	mDirty = true;
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
	assert(!mXformList.empty());

	calculateCameraParameters();
	if(!mpDevice || mXformList.empty()) return;

	var["data"].setBlob(mData);
	var["xform"].setBlob(mXformList[0]);

	if(!mpXformListBuffer || (mpXformListBuffer->getElementCount() != mXformList.size())) {
		mpXformListBuffer = Buffer::createStructured(mpDevice, sizeof(CameraXformData), (uint32_t)mXformList.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
	}

	var["xformListBufferSize"] = mpXformListBuffer ? mpXformListBuffer->getElementCount() : 0;

	mpXformListBuffer->setBlob(mXformList.data(), 0, sizeof(CameraXformData) * mXformList.size());
  var["xformListBuffer"].setBuffer(mpXformListBuffer);
}

void Camera::setPatternGenerator(const CPUSampleGenerator::SharedPtr& pGenerator, const float2& scale) {
	mJitterPattern.pGenerator = pGenerator;
	mJitterPattern.scale = scale;
	if (!pGenerator) {
		setJitterInternal(0, 0);
	}
}

void Camera::setBackgroundColor(const float4& color) {
	mData.backgroundColor = color;
}

void Camera::setJitter(float2 jitter) {
	setJitter(jitter.x, jitter.y);
}

void Camera::setJitter(float jitterX, float jitterY) {
	if (mJitterPattern.pGenerator) {
		LLOG_WRN << "Camera::setJitter() called when a pattern-generator object was attached to the camera. Detaching the pattern-generator";
		mJitterPattern.pGenerator = nullptr;
	}
	setJitterInternal(jitterX, jitterY);
}

void Camera::setJitterInternal(float jitterX, float jitterY) {
	if(mData.jitterX == jitterX && mData.jitterY == jitterY) return;
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

void Camera::updateFromAnimation(const std::vector<glm::mat4>& transformList) {
	if(transformList.empty() || mPersistentViewMatList == transformList) return;
	mPersistentViewMatList = transformList;
	mEnablePersistentViewMat = true;
	mDirty = true;
}

void Camera::setBackgroundImageFilename(const std::string& filename) {
	if(mBackgroundImageFilename == filename) return;
	mBackgroundImageFilename = filename;
}

std::vector<std::string> Camera::getDataFormattedDebugStrings() const {
	  calculateCameraParameters();

	  std::vector<std::string> out;

	  auto const& xform = mXformList[0];

	  out.push_back("World position: " + to_string(getPosition()));
	  //out.push_back("World position prev: " + to_string(mData.prevPosW));
	  out.push_back("Up: " + to_string(getUpVector()));
	  out.push_back("Target: " + to_string(getTarget()));
	  out.push_back("Camera U: " + to_string(xform.cameraU));
	  out.push_back("Camera V: " + to_string(xform.cameraV));
	  out.push_back("Camera W: " + to_string(xform.cameraW));
    out.push_back("Focal length: " + std::to_string(mData.focalLength));
    out.push_back("Aspect ratio: " + std::to_string(mData.aspectRatio));
    out.push_back("Clipping plane near: " + std::to_string(mData.nearZ));
    out.push_back("Clipping plane far: " + std::to_string(mData.farZ));
    out.push_back("Crop region: " + to_string(mData.cropRegion));

    out.push_back("Frame height: " + std::to_string(mData.frameHeight));
    out.push_back("Frame width: " + std::to_string(mData.frameWidth));
    out.push_back("Focal distance: " + std::to_string(mData.focalDistance));
    out.push_back("Aperture radius: " + std::to_string(mData.apertureRadius));

    out.push_back("Shutter speed: " + std::to_string(mData.shutterSpeed));
    out.push_back("ISO: " + std::to_string(mData.ISOSpeed));

    out.push_back("Backend color: " + to_string(mData.backgroundColor));

    return out;
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
