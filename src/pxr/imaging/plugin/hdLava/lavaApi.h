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

#ifndef HDLAVA_LAVA_API_H_
#define HDLAVA_LAVA_API_H_

#include "api.h"

#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/quaternion.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/imaging/hd/types.h"
#include "pxr/imaging/hd/camera.h"
#include "pxr/imaging/hd/material.h"
#include "pxr/imaging/hd/renderPassState.h"
#include "pxr/imaging/hd/renderDelegate.h"

#include <memory>
#include <vector>
#include <string>

#include "lava/lava.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdLavaDelegate;
class HdLavaRenderThread;
class MaterialAdapter;

class HdLavaApiImpl;
class RprUsdMaterial;

struct HdLavaApiVolume;
struct HdLavaApiEnvironmentLight;

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

enum HdLavaVisibilityFlag {
    kVisiblePrimary = 1 << 0,
    kVisibleShadow = 1 << 1,
    kVisibleReflection = 1 << 2,
    kVisibleRefraction = 1 << 3,
    kVisibleTransparent = 1 << 4,
    kVisibleDiffuse = 1 << 5,
    kVisibleGlossyReflection = 1 << 6,
    kVisibleGlossyRefraction = 1 << 7,
    kVisibleLight = 1 << 8,
    kVisibleAll = (kVisibleLight << 1) - 1
};
const uint32_t kInvisible = 0u;

class HdLavaApi final {
 public:
    HdLavaApi(HdLavaDelegate* delegate);
    ~HdLavaApi();

    GfVec2i GetViewportSize() const;
    void SetViewportSize(GfVec2i const& size);

    int GetNumCompletedSamples() const;
    // returns -1 if adaptive sampling is not used
    int GetNumActivePixels() const;

    void CommitResources();
    void Render(HdLavaRenderThread* renderThread);
    void AbortRender();

    bool IsAovFormatConversionAvailable() const;
    bool IsConverged() const;

 private:
    HdLavaDelegate* mDelegate = nullptr;
    lava::Renderer::UniquePtr mRenderer = nullptr;

    GfVec2i mViewportSize;

};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDLAVA_LAVA_API_H_