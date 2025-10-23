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

#include <algorithm>
#include <numeric>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/RenderContext.h"
#include "LightBVHSampler.h"

namespace Falcor {

bool LightBVHSampler::update(RenderContext* pRenderContext, LightCollection::SharedPtr pLightCollection) {
    //FALCOR_PROFILE(pRenderContext, "LightBVHSampler::update");

    bool samplerChanged = false;
    bool needsRefit = false;

    if (mpLightCollection != pLightCollection) {
        setLightCollection(std::move(pLightCollection));
        mNeedsRebuild = true;
        mpBVH = std::make_unique<LightBVH>(mpDevice, mpLightCollection);
    }

    // Check if light collection has changed.
    if (mLightCollectionUpdateFlags == LightCollection::UpdateFlags::LayoutChanged) {
        mNeedsRebuild = true;
    } else if (mLightCollectionUpdateFlags == LightCollection::UpdateFlags::MatrixChanged) {
        if (mOptions.buildOptions.allowRefitting) needsRefit = true;
        else mNeedsRebuild = true;
    }
    mLightCollectionUpdateFlags = LightCollection::UpdateFlags::None;

    // Rebuild BVH if it's marked as dirty.
    if (mNeedsRebuild) {
        mpBVHBuilder->build(pRenderContext, *mpBVH);
        mNeedsRebuild = false;
        samplerChanged = true;
    } else if (needsRefit) {
        mpBVH->refit(pRenderContext);
        samplerChanged = true;
    }

    return samplerChanged;
}

Program::DefineList LightBVHSampler::getDefines() const {
    // Call the base class first.
    auto defines = EmissiveLightSampler::getDefines();

    // Add our defines. None of these change the program vars.
    defines.add("_USE_BOUNDING_CONE", mOptions.useBoundingCone ? "1" : "0");
    defines.add("_USE_LIGHTING_CONE", mOptions.useLightingCone ? "1" : "0");
    defines.add("_DISABLE_NODE_FLUX", mOptions.disableNodeFlux ? "1" : "0");
    defines.add("_USE_UNIFORM_TRIANGLE_SAMPLING", mOptions.useUniformTriangleSampling ? "1" : "0");
    defines.add("_ACTUAL_MAX_TRIANGLES_PER_NODE", std::to_string(mOptions.buildOptions.maxTriangleCountPerLeaf));
    defines.add("_SOLID_ANGLE_BOUND_METHOD", std::to_string((uint32_t)mOptions.solidAngleBoundMethod));

    return defines;
}

void LightBVHSampler::setOptions(const Options& options) {
    if (std::memcmp(&mOptions, &options, sizeof(Options)) != 0) {
        mOptions = options;
        mNeedsRebuild = true;
    }
}

bool LightBVHSampler::bindShaderData(const ShaderVar& var) const {
    assert(var.isValid());
    assert(mpBVH);
    mpBVH->bindShaderData(var["_lightBVH"]);
    return true;
}

LightBVHSampler::LightBVHSampler(RenderContext* pRenderContext, LightCollection::SharedPtr pLightCollection, const Options& options)
    : EmissiveLightSampler(EmissiveLightSamplerType::LightBVH, std::move(pLightCollection))
    , mOptions(options) 
{
    // Create the BVH and builder.
    mpBVHBuilder = std::make_unique<LightBVHBuilder>(mOptions.buildOptions);
    mpBVH = std::make_unique<LightBVH>(mpDevice, mpLightCollection);
}

}  // namespace Falcor
