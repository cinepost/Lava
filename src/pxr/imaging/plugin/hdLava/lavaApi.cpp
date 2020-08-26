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

#include "lavaApi.h"
//#include "lavaApiAov.h"

#include "config.h"
#include "camera.h"
#include "renderDelegate.h"
#include "renderBuffer.h"
#include "renderParam.h"
//#include "aovDescriptor.h"

#include "pxr/imaging/glf/glew.h"
#include "pxr/imaging/glf/uvTextureData.h"

//#include "error.h"
//#include "pxr/imaging/rprUsd/helpers.h"
//#include "pxr/imaging/rprUsd/coreImage.h"
//#include "pxr/imaging/rprUsd/imageCache.h"
//#include "pxr/imaging/rprUsd/material.h"
//#include "pxr/imaging/rprUsd/materialRegistry.h"
//#include "pxr/imaging/rprUsd/contextMetadata.h"
//#include "pxr/imaging/rprUsd/contextHelpers.h"

#include "pxr/base/gf/math.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/base/arch/fileSystem.h"
#include "pxr/base/plug/plugin.h"
#include "pxr/base/plug/thisPlugin.h"
#include "pxr/imaging/pxOsd/tokens.h"
#include "pxr/usd/usdRender/tokens.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/base/tf/envSetting.h"

//#include "notify/message.h"

//#include <RadeonProRender_Baikal.h>
//#include <RprLoadStore.h>

#include <fstream>
#include <vector>
#include <mutex>

#ifdef WIN32
#include <shlobj_core.h>
#pragma comment(lib,"Shell32.lib")
#elif defined(__linux__)
#include <limits.h>
#include <sys/stat.h>
#endif // __APPLE__

PXR_NAMESPACE_OPEN_SCOPE

namespace {

using LockGuard = std::lock_guard<std::mutex>;

bool ArchCreateDirectory(const char* path) {
#ifdef WIN32
    return CreateDirectory(path, NULL) == TRUE;
#else
    return mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
#endif
}

template <typename T>
struct RenderSetting {
    T value;
    bool isDirty;
};

GfVec4f ToVec4(GfVec3f const& vec, float w) {
    return GfVec4f(vec[0], vec[1], vec[2], w);
}

} // namespace anonymous

HdFormat ConvertUsdRenderVarDataType(TfToken const& format) {
    static std::map<TfToken, HdFormat> s_mapping = []() {
        std::map<TfToken, HdFormat> ret;

        auto addMappingEntry = [&ret](std::string name, HdFormat format) {
            ret[TfToken(name, TfToken::Immortal)] = format;
        };

        addMappingEntry("int", HdFormatInt32);
        addMappingEntry("half", HdFormatFloat16);
        addMappingEntry("half3", HdFormatFloat16Vec3);
        addMappingEntry("half4", HdFormatFloat16Vec4);
        addMappingEntry("float", HdFormatFloat32);
        addMappingEntry("float3", HdFormatFloat32Vec3);
        addMappingEntry("float4", HdFormatFloat32Vec4);
        addMappingEntry("point3f", HdFormatFloat32Vec3);
        addMappingEntry("vector3f", HdFormatFloat32Vec3);
        addMappingEntry("normal3f", HdFormatFloat32Vec3);
        addMappingEntry("color3f", HdFormatFloat32Vec3);

        return ret;
    }();
    static std::string s_supportedFormats = []() {
        std::string ret;
        auto it = s_mapping.begin();
        for (size_t i = 0; i < s_mapping.size(); ++i, ++it) {
            ret += it->first.GetString();
            if (i + 1 != s_mapping.size()) {
                ret += ", ";
            }
        }
        return ret;
    }();

    auto it = s_mapping.find(format);
    if (it == s_mapping.end()) {
        TF_RUNTIME_ERROR("Unsupported UsdRenderVar format. Supported formats: %s", s_supportedFormats.c_str());
        return HdFormatInvalid;
    }
    return it->second;
}


HdLavaApi::HdLavaApi(HdLavaDelegate* delegate) : mDelegate(delegate) {
    printf("HdLavaApi constructor\n");
    mRenderer = lava::Renderer::create();
}

HdLavaApi::~HdLavaApi() {

}

bool HdLavaApi::IsAovFormatConversionAvailable() const {
    return false;
}

int HdLavaApi::GetNumCompletedSamples() const {
    //return m_impl->GetNumCompletedSamples();
    return 0;
}

GfVec2i HdLavaApi::GetViewportSize() const {
    //return m_impl->GetViewportSize();
    return mViewportSize;
}

void HdLavaApi::SetViewportSize(GfVec2i const& size) {
    //m_impl->SetViewportSize(size);
    mViewportSize = size;
}

void HdLavaApi::CommitResources() {
    //m_impl->CommitResources();
}

void HdLavaApi::Render(HdLavaRenderThread* renderThread) {
    const bool isBatch = mDelegate->IsBatch();
    
    bool firstResolve = true;
    bool stopRequested = false;
    while (!IsConverged() || stopRequested) {
        renderThread->WaitUntilPaused();
        stopRequested = renderThread->IsStopRequested();
        if (stopRequested) {
            break;
        }

        auto status = mRenderer->RenderSample();

        if (status == lava::LAVA_ERROR_ABORTED) {
            stopRequested = true;
            break;
        }

        if (!isBatch && !IsConverged()) {
            // Last framebuffer resolve will be called after "while" in case framebuffer is converged.
            // We do not resolve framebuffers in case user requested render stop
            ResolveFramebuffers(&firstResolve);
        }
        stopRequested = renderThread->IsStopRequested();
    }

    if (!stopRequested) {
        ResolveFramebuffers(&firstResolve);
    }
}

bool HdLavaApi::IsConverged() const {
    return false;
}

void HdLavaApi::AbortRender() {
    //m_impl->AbortRender();
}

PXR_NAMESPACE_CLOSE_SCOPE