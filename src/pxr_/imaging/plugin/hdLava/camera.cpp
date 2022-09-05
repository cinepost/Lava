/************************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
************************************************************************/

#include "camera.h"
#include "renderParam.h"

#include "pxr/usd/usdGeom/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(HdLavaCameraTokens,
    (apertureBlades)
);

namespace {

template<typename T>
bool EvalCameraParam(T* value, const TfToken& paramName, HdSceneDelegate* sceneDelegate, const SdfPath& primPath, T defaultValue) {
    VtValue vtval = sceneDelegate->GetCameraParamValue(primPath, paramName);
    if (vtval.IsEmpty()) {
        *value = defaultValue;
        return false;
    }
    if (!vtval.IsHolding<T>()) {
        *value = defaultValue;
        TF_CODING_ERROR("%s: type mismatch - %s", paramName.GetText(), vtval.GetTypeName().c_str());
        return false;
    }

    *value = vtval.UncheckedGet<T>();
    return true;
}

template <typename T>
bool EvalCameraParam(T* value, const TfToken& paramName, HdSceneDelegate* sceneDelegate, const SdfPath& primPath) {
    return EvalCameraParam(value, paramName, sceneDelegate, primPath, std::numeric_limits<T>::quiet_NaN());
}

}  // namespace anonymous

HdLavaCamera::HdLavaCamera(SdfPath const& id) : HdCamera(id),
    m_horizontalAperture(std::numeric_limits<float>::quiet_NaN()),
    m_verticalAperture(std::numeric_limits<float>::quiet_NaN()),
    m_horizontalApertureOffset(std::numeric_limits<float>::quiet_NaN()),
    m_verticalApertureOffset(std::numeric_limits<float>::quiet_NaN()),
    m_focalLength(std::numeric_limits<float>::quiet_NaN()),
    m_fStop(std::numeric_limits<float>::quiet_NaN()),
    m_focusDistance(std::numeric_limits<float>::quiet_NaN()),
    m_apertureBlades(0),
    m_shutterOpen(std::numeric_limits<double>::quiet_NaN()),
    m_shutterClose(std::numeric_limits<double>::quiet_NaN()),
    m_clippingRange(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()) {

}

HdDirtyBits HdLavaCamera::GetInitialDirtyBitsMask() const {
    return HdCamera::DirtyParams | HdCamera::GetInitialDirtyBitsMask();
}

void HdLavaCamera::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) {
    // HdLavaApi uses HdLavaCamera directly, so we need to stop the render thread before changing the camera.
    static_cast<HdLavaRenderParam*>(renderParam)->AcquireLavaApiForEdit();

    m_rprDirtyBits |= *dirtyBits;

    if (*dirtyBits & HdCamera::DirtyParams) {
        SdfPath const& id = GetId();

        EvalCameraParam(&m_focalLength, HdCameraTokens->focalLength, sceneDelegate, id);

        EvalCameraParam(&m_horizontalAperture, HdCameraTokens->horizontalAperture, sceneDelegate, id);
        EvalCameraParam(&m_verticalAperture, HdCameraTokens->verticalAperture, sceneDelegate, id);
        EvalCameraParam(&m_horizontalApertureOffset, HdCameraTokens->horizontalApertureOffset, sceneDelegate, id);
        EvalCameraParam(&m_verticalApertureOffset, HdCameraTokens->verticalApertureOffset, sceneDelegate, id);

        EvalCameraParam(&m_fStop, HdCameraTokens->fStop, sceneDelegate, id);
        EvalCameraParam(&m_focusDistance, HdCameraTokens->focusDistance, sceneDelegate, id);
        EvalCameraParam(&m_shutterOpen, HdCameraTokens->shutterOpen, sceneDelegate, id);
        EvalCameraParam(&m_shutterClose, HdCameraTokens->shutterClose, sceneDelegate, id);
        EvalCameraParam(&m_clippingRange, HdCameraTokens->clippingRange, sceneDelegate, id, GfRange1f(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()));

        EvalCameraParam(&m_projectionType, UsdGeomTokens->projection, sceneDelegate, id, TfToken());

        EvalCameraParam(&m_apertureBlades, HdLavaCameraTokens->apertureBlades, sceneDelegate, id, 16);
    }

    if (*dirtyBits & HdCamera::DirtyViewMatrix) {
        sceneDelegate->SampleTransform(GetId(), &m_transform);
    }

    HdCamera::Sync(sceneDelegate, renderParam, dirtyBits);
}

void HdLavaCamera::Finalize(HdRenderParam* renderParam) {
    // HdLavaApi uses HdLavaCamera directly, so we need to stop the render thread before releasing the camera.
    static_cast<HdLavaRenderParam*>(renderParam)->AcquireLavaApiForEdit();
}

bool HdLavaCamera::GetApertureSize(GfVec2f* v) const {
    if (!std::isnan(m_horizontalAperture) &&
        !std::isnan(m_verticalAperture)) {
        *v = {m_horizontalAperture, m_verticalAperture};
        return true;
    }
    return false;
}

bool HdLavaCamera::GetApertureOffset(GfVec2f* v) const {
    if (!std::isnan(m_horizontalApertureOffset) &&
        !std::isnan(m_verticalApertureOffset)) {
        *v = {m_horizontalApertureOffset, m_verticalApertureOffset};
        return true;
    }
    return false;
}

bool HdLavaCamera::GetFocalLength(float* v) const {
    if (!std::isnan(m_focalLength)) {
        *v = m_focalLength;
        return true;
    }
    return false;
}

bool HdLavaCamera::GetFStop(float* v) const {
    if (!std::isnan(m_fStop)) {
        *v = m_fStop;
        return true;
    }
    return false;
}

bool HdLavaCamera::GetFocusDistance(float* v) const {
    if (!std::isnan(m_focusDistance)) {
        *v = m_focusDistance;
        return true;
    }
    return false;
}

bool HdLavaCamera::GetShutterOpen(double* v) const {
    if (!std::isnan(m_shutterOpen)) {
        *v = m_shutterOpen;
        return true;
    }
    return false;
}

bool HdLavaCamera::GetShutterClose(double* v) const {
    if (!std::isnan(m_shutterClose)) {
        *v = m_shutterClose;
        return true;
    }
    return false;
}

bool HdLavaCamera::GetClippingRange(GfRange1f* v) const {
    if (!std::isnan(m_clippingRange.GetMin()) &&
        !std::isnan(m_clippingRange.GetMax())) {
        *v = m_clippingRange;
        return true;
    }
    return false;
}

bool HdLavaCamera::GetProjectionType(TfToken* v) const {
    if (!m_projectionType.IsEmpty()) {
        *v = m_projectionType;
        return true;
    }
    return false;
}

PXR_NAMESPACE_CLOSE_SCOPE