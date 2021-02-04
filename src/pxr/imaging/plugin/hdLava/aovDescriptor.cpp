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

#include "aovDescriptor.h"

#include "pxr/base/tf/instantiateSingleton.h"

#include "pxr/imaging/hd/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

const HdLavaAovDescriptor kInvalidDesc;

TF_INSTANTIATE_SINGLETON(HdLavaAovRegistry);
TF_DEFINE_PUBLIC_TOKENS(HdLavaAovTokens, HDLAVA_AOV_TOKENS);

HdLavaAovRegistry::HdLavaAovRegistry() {
    const auto rprAovMax = LAVA_AOV_COLOR_RIGHT + 1;
    const GfVec4f idClearValue(255.0f, 255.0f, 255.0f, 0.0f);

    m_aovDescriptors.resize(rprAovMax);
    m_aovDescriptors[LAVA_AOV_COLOR] = HdLavaAovDescriptor(LAVA_AOV_COLOR);
    m_aovDescriptors[LAVA_AOV_DIFFUSE_ALBEDO] = HdLavaAovDescriptor(LAVA_AOV_DIFFUSE_ALBEDO); // XXX: LAVA's albedo can be noisy in some cases, so we left it as multisampled
    m_aovDescriptors[LAVA_AOV_VARIANCE] = HdLavaAovDescriptor(LAVA_AOV_VARIANCE);
    m_aovDescriptors[LAVA_AOV_OPACITY] = HdLavaAovDescriptor(LAVA_AOV_OPACITY);
    m_aovDescriptors[LAVA_AOV_EMISSION] = HdLavaAovDescriptor(LAVA_AOV_EMISSION);
    m_aovDescriptors[LAVA_AOV_DIRECT_ILLUMINATION] = HdLavaAovDescriptor(LAVA_AOV_DIRECT_ILLUMINATION);
    m_aovDescriptors[LAVA_AOV_INDIRECT_ILLUMINATION] = HdLavaAovDescriptor(LAVA_AOV_INDIRECT_ILLUMINATION);
    m_aovDescriptors[LAVA_AOV_AO] = HdLavaAovDescriptor(LAVA_AOV_AO);
    m_aovDescriptors[LAVA_AOV_DIRECT_DIFFUSE] = HdLavaAovDescriptor(LAVA_AOV_DIRECT_DIFFUSE);
    m_aovDescriptors[LAVA_AOV_DIRECT_REFLECT] = HdLavaAovDescriptor(LAVA_AOV_DIRECT_REFLECT);
    m_aovDescriptors[LAVA_AOV_INDIRECT_DIFFUSE] = HdLavaAovDescriptor(LAVA_AOV_INDIRECT_DIFFUSE);
    m_aovDescriptors[LAVA_AOV_INDIRECT_REFLECT] = HdLavaAovDescriptor(LAVA_AOV_INDIRECT_REFLECT);
    m_aovDescriptors[LAVA_AOV_REFRACT] = HdLavaAovDescriptor(LAVA_AOV_REFRACT);
    m_aovDescriptors[LAVA_AOV_VOLUME] = HdLavaAovDescriptor(LAVA_AOV_VOLUME);
    m_aovDescriptors[LAVA_AOV_LIGHT_GROUP0] = HdLavaAovDescriptor(LAVA_AOV_LIGHT_GROUP0);
    m_aovDescriptors[LAVA_AOV_LIGHT_GROUP1] = HdLavaAovDescriptor(LAVA_AOV_LIGHT_GROUP1);
    m_aovDescriptors[LAVA_AOV_LIGHT_GROUP2] = HdLavaAovDescriptor(LAVA_AOV_LIGHT_GROUP2);
    m_aovDescriptors[LAVA_AOV_LIGHT_GROUP3] = HdLavaAovDescriptor(LAVA_AOV_LIGHT_GROUP3);
    m_aovDescriptors[LAVA_AOV_COLOR_RIGHT] = HdLavaAovDescriptor(LAVA_AOV_COLOR_RIGHT);
    m_aovDescriptors[LAVA_AOV_SHADOW_CATCHER] = HdLavaAovDescriptor(LAVA_AOV_SHADOW_CATCHER);
    m_aovDescriptors[LAVA_AOV_REFLECTION_CATCHER] = HdLavaAovDescriptor(LAVA_AOV_REFLECTION_CATCHER);

    m_aovDescriptors[LAVA_AOV_DEPTH] = HdLavaAovDescriptor(LAVA_AOV_DEPTH, false, HdFormatFloat32, GfVec4f(std::numeric_limits<float>::infinity()));
    m_aovDescriptors[LAVA_AOV_UV] = HdLavaAovDescriptor(LAVA_AOV_UV, false, HdFormatFloat32Vec3);
    m_aovDescriptors[LAVA_AOV_SHADING_NORMAL] = HdLavaAovDescriptor(LAVA_AOV_SHADING_NORMAL, false, HdFormatFloat32Vec3);
    m_aovDescriptors[LAVA_AOV_GEOMETRIC_NORMAL] = HdLavaAovDescriptor(LAVA_AOV_GEOMETRIC_NORMAL, false);
    m_aovDescriptors[LAVA_AOV_OBJECT_ID] = HdLavaAovDescriptor(LAVA_AOV_OBJECT_ID, false, HdFormatInt32, idClearValue);
    m_aovDescriptors[LAVA_AOV_MATERIAL_IDX] = HdLavaAovDescriptor(LAVA_AOV_MATERIAL_IDX, false, HdFormatInt32, idClearValue);
    m_aovDescriptors[LAVA_AOV_OBJECT_GROUP_ID] = HdLavaAovDescriptor(LAVA_AOV_OBJECT_GROUP_ID, false, HdFormatInt32, idClearValue);
    m_aovDescriptors[LAVA_AOV_WORLD_COORDINATE] = HdLavaAovDescriptor(LAVA_AOV_WORLD_COORDINATE, false);
    m_aovDescriptors[LAVA_AOV_BACKGROUND] = HdLavaAovDescriptor(LAVA_AOV_BACKGROUND, false);
    m_aovDescriptors[LAVA_AOV_VELOCITY] = HdLavaAovDescriptor(LAVA_AOV_VELOCITY, false);
    m_aovDescriptors[LAVA_AOV_VIEW_SHADING_NORMAL] = HdLavaAovDescriptor(LAVA_AOV_VIEW_SHADING_NORMAL, false);

    m_computedAovDescriptors.resize(kComputedAovsCount);
    m_computedAovDescriptors[kNdcDepth] = HdLavaAovDescriptor(kNdcDepth, false, HdFormatFloat32, GfVec4f(std::numeric_limits<float>::infinity()), true);
    m_computedAovDescriptors[kColorAlpha] = HdLavaAovDescriptor(kColorAlpha, true, HdFormatFloat32Vec4, GfVec4f(0.0f), true);

    auto addAovNameLookup = [this](TfToken const& name, HdLavaAovDescriptor const& descriptor) {
        auto status = m_aovNameLookup.emplace(name, AovNameLookupValue(descriptor.id, descriptor.computed));
        if (!status.second) {
            TF_CODING_ERROR("AOV lookup name should be unique");
        }
    };

    addAovNameLookup(HdAovTokens->color, m_computedAovDescriptors[kColorAlpha]);
    addAovNameLookup(HdAovTokens->normal, m_aovDescriptors[LAVA_AOV_SHADING_NORMAL]);
    addAovNameLookup(HdAovTokens->primId, m_aovDescriptors[LAVA_AOV_OBJECT_ID]);
    addAovNameLookup(HdAovTokens->Neye, m_aovDescriptors[LAVA_AOV_VIEW_SHADING_NORMAL]);
    addAovNameLookup(HdAovTokens->depth, m_computedAovDescriptors[kNdcDepth]);
    addAovNameLookup(HdLavaGetCameraDepthAovName(), m_aovDescriptors[LAVA_AOV_DEPTH]);

    addAovNameLookup(HdLavaAovTokens->rawColor, m_aovDescriptors[LAVA_AOV_COLOR]);
    addAovNameLookup(HdLavaAovTokens->albedo, m_aovDescriptors[LAVA_AOV_DIFFUSE_ALBEDO]);
    addAovNameLookup(HdLavaAovTokens->variance, m_aovDescriptors[LAVA_AOV_VARIANCE]);
    addAovNameLookup(HdLavaAovTokens->opacity, m_aovDescriptors[LAVA_AOV_OPACITY]);
    addAovNameLookup(HdLavaAovTokens->emission, m_aovDescriptors[LAVA_AOV_EMISSION]);
    addAovNameLookup(HdLavaAovTokens->directIllumination, m_aovDescriptors[LAVA_AOV_DIRECT_ILLUMINATION]);
    addAovNameLookup(HdLavaAovTokens->indirectIllumination, m_aovDescriptors[LAVA_AOV_INDIRECT_ILLUMINATION]);
    addAovNameLookup(HdLavaAovTokens->ao, m_aovDescriptors[LAVA_AOV_AO]);
    addAovNameLookup(HdLavaAovTokens->directDiffuse, m_aovDescriptors[LAVA_AOV_DIRECT_DIFFUSE]);
    addAovNameLookup(HdLavaAovTokens->directReflect, m_aovDescriptors[LAVA_AOV_DIRECT_REFLECT]);
    addAovNameLookup(HdLavaAovTokens->indirectDiffuse, m_aovDescriptors[LAVA_AOV_INDIRECT_DIFFUSE]);
    addAovNameLookup(HdLavaAovTokens->indirectReflect, m_aovDescriptors[LAVA_AOV_INDIRECT_REFLECT]);
    addAovNameLookup(HdLavaAovTokens->refract, m_aovDescriptors[LAVA_AOV_REFRACT]);
    addAovNameLookup(HdLavaAovTokens->volume, m_aovDescriptors[LAVA_AOV_VOLUME]);
    addAovNameLookup(HdLavaAovTokens->lightGroup0, m_aovDescriptors[LAVA_AOV_LIGHT_GROUP0]);
    addAovNameLookup(HdLavaAovTokens->lightGroup1, m_aovDescriptors[LAVA_AOV_LIGHT_GROUP1]);
    addAovNameLookup(HdLavaAovTokens->lightGroup2, m_aovDescriptors[LAVA_AOV_LIGHT_GROUP2]);
    addAovNameLookup(HdLavaAovTokens->lightGroup3, m_aovDescriptors[LAVA_AOV_LIGHT_GROUP3]);
    addAovNameLookup(HdLavaAovTokens->colorRight, m_aovDescriptors[LAVA_AOV_COLOR_RIGHT]);
    addAovNameLookup(HdLavaAovTokens->materialIdx, m_aovDescriptors[LAVA_AOV_MATERIAL_IDX]);
    addAovNameLookup(HdLavaAovTokens->objectGroupId, m_aovDescriptors[LAVA_AOV_OBJECT_GROUP_ID]);
    addAovNameLookup(HdLavaAovTokens->geometricNormal, m_aovDescriptors[LAVA_AOV_GEOMETRIC_NORMAL]);
    addAovNameLookup(HdLavaAovTokens->worldCoordinate, m_aovDescriptors[LAVA_AOV_WORLD_COORDINATE]);
    addAovNameLookup(HdLavaAovTokens->primvarsSt, m_aovDescriptors[LAVA_AOV_UV]);
    addAovNameLookup(HdLavaAovTokens->shadowCatcher, m_aovDescriptors[LAVA_AOV_SHADOW_CATCHER]);
    addAovNameLookup(HdLavaAovTokens->reflectionCatcher, m_aovDescriptors[LAVA_AOV_REFLECTION_CATCHER]);
    addAovNameLookup(HdLavaAovTokens->background, m_aovDescriptors[LAVA_AOV_BACKGROUND]);
    addAovNameLookup(HdLavaAovTokens->velocity, m_aovDescriptors[LAVA_AOV_VELOCITY]);
    addAovNameLookup(HdLavaAovTokens->viewShadingNormal, m_aovDescriptors[LAVA_AOV_VIEW_SHADING_NORMAL]);
}

HdLavaAovDescriptor const& HdLavaAovRegistry::GetAovDesc(TfToken const& name) {
    auto it = m_aovNameLookup.find(name);
    if (it == m_aovNameLookup.end()) {
        return kInvalidDesc;
    }

    return GetAovDesc(it->second.id, it->second.isComputed);
}

HdLavaAovDescriptor const& HdLavaAovRegistry::GetAovDesc(uint32_t id, bool computed) {
    size_t descsSize = computed ? m_computedAovDescriptors.size() : m_aovDescriptors.size();
    if (id < 0 || id >= descsSize) {
        TF_RUNTIME_ERROR("Invalid arguments: %#x (computed=%d)", id, int(computed));
        return kInvalidDesc;
    }

    if (computed) {
        return m_computedAovDescriptors[id];
    } else {
        return m_aovDescriptors[id];
    }
}

TfToken const& HdLavaGetCameraDepthAovName() {
#if PXR_VERSION < 2002
    return HdAovTokens->linearDepth;
#else
    return HdAovTokens->cameraDepth;
#endif
}

PXR_NAMESPACE_CLOSE_SCOPE
