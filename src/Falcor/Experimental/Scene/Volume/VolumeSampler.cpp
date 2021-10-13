/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
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
#include "VolumeSampler.h"

namespace Falcor {


    VolumeSampler::SharedPtr VolumeSampler::create(RenderContext* pRenderContext, Scene::SharedPtr pScene, const Options& options)
    {
        return SharedPtr(new VolumeSampler(pRenderContext, pScene, options));
    }

    Program::DefineList VolumeSampler::getDefines() const
    {
        Program::DefineList defines;
        defines.add("VOLUME_SAMPLER_USE_BRICKEDGRID", std::to_string((uint32_t)mOptions.useBrickedGrid));
        defines.add("VOLUME_SAMPLER_TRANSMITTANCE_ESTIMATOR", std::to_string((uint32_t)mOptions.transmittanceEstimator));
        defines.add("VOLUME_SAMPLER_DISTANCE_SAMPLER", std::to_string((uint32_t)mOptions.distanceSampler));
        return defines;
    }

    void VolumeSampler::setShaderData(const ShaderVar& var) const
    {
        assert(var.isValid());
    }

    VolumeSampler::VolumeSampler(RenderContext* pRenderContext, Scene::SharedPtr pScene, const Options& options)
        : mpScene(pScene)
        , mOptions(options)
    {
        assert(pScene);
    }

#ifdef SCRIPTING
    SCRIPT_BINDING(VolumeSampler)
    {
        pybind11::enum_<TransmittanceEstimator> transmittanceEstimator(m, "TransmittanceEstimator");
        transmittanceEstimator.value("DeltaTracking", TransmittanceEstimator::DeltaTracking);
        transmittanceEstimator.value("RatioTracking", TransmittanceEstimator::RatioTracking);
        transmittanceEstimator.value("RatioTrackingLocalMajorant", TransmittanceEstimator::RatioTrackingLocalMajorant);

        pybind11::enum_<DistanceSampler> distanceSampler(m, "DistanceSampler");
        distanceSampler.value("DeltaTracking", DistanceSampler::DeltaTracking);
        distanceSampler.value("DeltaTrackingLocalMajorant", DistanceSampler::DeltaTrackingLocalMajorant);

        // TODO use a nested class in the bindings when supported.
        ScriptBindings::SerializableStruct<VolumeSampler::Options> options(m, "VolumeSamplerOptions");
#define field(f_) field(#f_, &VolumeSampler::Options::f_)
        options.field(transmittanceEstimator);
        options.field(distanceSampler);
        options.field(useBrickedGrid);
#undef field
    }
#endif  // SCRIPTING
}
