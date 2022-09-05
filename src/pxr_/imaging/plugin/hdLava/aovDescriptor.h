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

#ifndef HDLAVA_AOV_DESCRIPTOR_H_
#define HDLAVA_AOV_DESCRIPTOR_H_

#include "pxr/base/tf/singleton.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/gf/vec4f.h"

#include "pxr/imaging/hd/types.h"

#include "lava/types.h"

#include <map>

PXR_NAMESPACE_OPEN_SCOPE

#define HDLAVA_AOV_TOKENS \
    (rawColor) \
    (albedo) \
    (variance) \
    (worldCoordinate) \
    (opacity) \
    ((primvarsSt, "primvars:st")) \
    (materialIdx) \
    (geometricNormal) \
    (objectGroupId) \
    (shadowCatcher) \
    (background) \
    (emission) \
    (velocity) \
    (directIllumination) \
    (indirectIllumination) \
    (ao) \
    (directDiffuse) \
    (directReflect) \
    (indirectDiffuse) \
    (indirectReflect) \
    (refract) \
    (volume) \
    (lightGroup0) \
    (lightGroup1) \
    (lightGroup2) \
    (lightGroup3) \
    (viewShadingNormal) \
    (reflectionCatcher) \
    (colorRight)

TF_DECLARE_PUBLIC_TOKENS(HdLavaAovTokens, HDLAVA_AOV_TOKENS);

const lava::Aov kAovNone = static_cast<lava::Aov>(-1);

enum ComputedAovs {
    kNdcDepth = 0,
    kColorAlpha,
    kComputedAovsCount
};

struct HdLavaAovDescriptor {
    uint32_t id;
    HdFormat format;
    bool multiSampled;
    bool computed;
    GfVec4f clearValue;

    HdLavaAovDescriptor(uint32_t id = kAovNone, bool multiSampled = true, HdFormat format = HdFormatFloat32Vec4, GfVec4f clearValue = GfVec4f(0.0f), bool computed = false)
        : id(id), format(format), multiSampled(multiSampled), computed(computed), clearValue(clearValue) {

    }
};

class HdLavaAovRegistry {
public:
    static HdLavaAovRegistry& GetInstance() {
        return TfSingleton<HdLavaAovRegistry>::GetInstance();
    }

    HdLavaAovDescriptor const& GetAovDesc(TfToken const& name);
    HdLavaAovDescriptor const& GetAovDesc(uint32_t id, bool computed);

    HdLavaAovRegistry(HdLavaAovRegistry const&) = delete;
    HdLavaAovRegistry& operator=(HdLavaAovRegistry const&) = delete;
    HdLavaAovRegistry(HdLavaAovRegistry&&) = delete;
    HdLavaAovRegistry& operator=(HdLavaAovRegistry&&) = delete;

private:
    HdLavaAovRegistry();
    ~HdLavaAovRegistry() = default;

    friend class TfSingleton<HdLavaAovRegistry>;

private:
    struct AovNameLookupValue {
        uint32_t id;
        bool isComputed;

        AovNameLookupValue(uint32_t id, bool isComputed = false)
            : id(id), isComputed(isComputed) {

        }
    };
    std::map<TfToken, AovNameLookupValue> m_aovNameLookup;

    std::vector<HdLavaAovDescriptor> m_aovDescriptors;
    std::vector<HdLavaAovDescriptor> m_computedAovDescriptors;
};

TfToken const& HdLavaGetCameraDepthAovName();

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDLAVA_AOV_DESCRIPTOR_H_
