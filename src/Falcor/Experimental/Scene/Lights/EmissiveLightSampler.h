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
#ifndef SRC_FALCOR_EXPERIMENTAL_SCENE_LIGHTS_EMISSIVELIGHTSAMPLER_H_
#define SRC_FALCOR_EXPERIMENTAL_SCENE_LIGHTS_EMISSIVELIGHTSAMPLER_H_

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/Object.h"
#include "Falcor/Core/Program/DefineList.h"
#include "Falcor/Core/Program/ShaderVar.h"
#include "Falcor/Scene/Lights/LightCollection.h"

#include "EmissiveLightSamplerType.slangh"

#include "sigs/sigs.h"


namespace Falcor {

class Device;
class RenderContext;

/** Base class for emissive light sampler implementations.

    All light samplers follows the same interface to make them interchangeable.
    If an unrecoverable error occurs, these functions may throw exceptions.
*/
class FALCOR_API EmissiveLightSampler : public Object {
public:
    virtual ~EmissiveLightSampler() = default;

    /** Updates the sampler to the current frame.
        \param[in] pRenderContext The render context.
        \return True if the sampler was updated.
    */
    virtual bool update(RenderContext* pRenderContext, LightCollection::SharedPtr pLightCollection) { return false; }

    /** Return a list of shader defines to use this light sampler.
    *   \return Returns a list of shader defines.
    */
    virtual DefineList getDefines() const;

    /** Bind the light sampler data to a given shader var
    */
    virtual bool bindShaderData(const ShaderVar& var) const { return true; }

    /** Returns the type of emissive light sampler.
        \return The type of the derived class.
    */
    EmissiveLightSamplerType getType() const { return mType; }

protected:
    EmissiveLightSampler(EmissiveLightSamplerType type, Falcor::SharedPtr<LightCollection> pLightCollection);
    void setLightCollection(Falcor::SharedPtr<LightCollection> pLightCollection);

    // Internal state
    const EmissiveLightSamplerType mType;       ///< Type of emissive sampler. See EmissiveLightSamplerType.slangh.
    Falcor::SharedPtr<Device> mpDevice;
    Falcor::SharedPtr<LightCollection> mpLightCollection;
    sigs::Connection mUpdateFlagsConnection;
    LightCollection::UpdateFlags mLightCollectionUpdateFlags = LightCollection::UpdateFlags::None;
};

}

#endif  // SRC_FALCOR_EXPERIMENTAL_SCENE_LIGHTS_EMISSIVELIGHTSAMPLER_H_