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
#include "CameraController.h"
#include "Utils/Math/FalcorMath.h"
#include "Camera.h"

namespace Falcor {

float2 convertCamPosRange(const float2 pos) {
    // Convert [0,1] range to [-1, 1], and inverse the Y (screen-space y==0 is top)
    const float2 scale(2, -2);
    const float2 offset(-1, 1);
    float2 res = (pos * scale) + offset;
    return res;
}

void OrbiterCameraController::setModelParams(const float3& center, float radius, float distanceInRadius) {
    mModelCenter = center;
    mModelRadius = radius;
    mCameraDistance = distanceInRadius;
    mRotation = glm::mat3();
    mbDirty = true;
}

bool OrbiterCameraController::update() {
    if(mpCamera && mbDirty) {
        mbDirty = false;
        mShouldRotate = false;
        mpCamera->setTarget(mModelCenter);

        float3 camPos = mModelCenter;
        camPos += (float3(0,0,1) * mRotation) * mModelRadius * mCameraDistance;
        mpCamera->setPosition(camPos);

        float3 up(0, 1, 0);
        up = up * mRotation;
        mpCamera->setUpVector(up);
        return true;
    }
    return false;
}

template<bool b6DoF>
FirstPersonCameraControllerCommon<b6DoF>::FirstPersonCameraControllerCommon(const Camera::SharedPtr& pCamera) : CameraController(pCamera) {
    mTimer.update();
}

template<bool b6DoF>
bool FirstPersonCameraControllerCommon<b6DoF>::update() {
    mTimer.update();

    bool dirty = false;
    if(mpCamera) {
        if(mShouldRotate) {
            float3 camPos = mpCamera->getPosition();
            float3 camTarget = mpCamera->getTarget();
            float3 camUp = b6DoF ? mpCamera->getUpVector() : float3(0, 1, 0);;

            float3 viewDir = glm::normalize(camTarget - camPos);
            if(mIsLeftButtonDown) {
                float3 sideway = glm::cross(viewDir, normalize(camUp));

                // Rotate around x-axis
                glm::quat qy = glm::angleAxis(mMouseDelta.y * mSpeedModifier, sideway);
                glm::mat3 rotY(qy);
                viewDir = viewDir * rotY;
                camUp = camUp * rotY;

                // Rotate around y-axis
                glm::quat qx = glm::angleAxis(mMouseDelta.x * mSpeedModifier, camUp);
                glm::mat3 rotX(qx);
                viewDir = viewDir * rotX;

                mpCamera->setTarget(camPos + viewDir);
                mpCamera->setUpVector(camUp);
                dirty = true;
            }

            if(b6DoF && mIsRightButtonDown) {
                // Rotate around x-axis
                glm::quat q = glm::angleAxis(mMouseDelta.x * mSpeedModifier, viewDir);
                glm::mat3 rot(q);
                camUp = camUp * rot;
                mpCamera->setUpVector(camUp);
                dirty = true;
            }

            mShouldRotate = false;
        }

        if(mMovement.any()) {
            float3 movement(0, 0, 0);
            movement.z += mMovement.test(Direction::Forward) ? 1 : 0;
            movement.z += mMovement.test(Direction::Backward) ? -1 : 0;
            movement.x += mMovement.test(Direction::Left) ? 1 : 0;
            movement.x += mMovement.test(Direction::Right) ? -1 : 0;
            movement.y += mMovement.test(Direction::Up) ? 1 : 0;
            movement.y += mMovement.test(Direction::Down) ? -1 : 0;

            float3 camPos = mpCamera->getPosition();
            float3 camTarget = mpCamera->getTarget();
            float3 camUp = mpCamera->getUpVector();

            float3 viewDir = normalize(camTarget - camPos);
            float3 sideway = glm::cross(viewDir, normalize(camUp));

            float elapsedTime = (float)mTimer.delta();

            float curMove = mSpeedModifier * mSpeed * elapsedTime;
            camPos += movement.z * curMove * viewDir;
            camPos += movement.x * curMove * sideway;
            camPos += movement.y * curMove * camUp;

            camTarget = camPos + viewDir;

            mpCamera->setPosition(camPos);
            mpCamera->setTarget(camTarget);
            dirty = true;
        }
    }

    return dirty;
}

template class FirstPersonCameraControllerCommon < true > ;
template class FirstPersonCameraControllerCommon < false > ;

}  // namespace Falcor
