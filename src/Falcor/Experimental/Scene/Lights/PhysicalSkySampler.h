#ifndef SRC_FALCOR_EXPERIMENTAL_SCENE_LIGHTS_PHYSICALSKYSAMPLER_H_
#define SRC_FALCOR_EXPERIMENTAL_SCENE_LIGHTS_PHYSICALSKYSAMPLER_H_

#include "Falcor/Core/Macros.h"
#include "Falcor/Core/Object.h"
#include "Falcor/Core/Pass/ComputePass.h"
#include "Falcor/Utils/Timing/Profiler.h"

namespace Falcor {

class Device;
class RenderContext;

/** Environment map sampler.
    Utily class for sampling and evaluating radiance stored in an omnidirectional environment map.
*/
class FALCOR_API PhysicalSkySampler {
 public:
    PhysicalSkySampler(RenderContext* pRenderContext);
    virtual ~EnvMapSampler() = default;

    /** Bind the environment map sampler to a given shader variable.
        \param[in] var Shader variable.
    */
    void bindShaderData(const ShaderVar& var) const;

    const ref<Texture>& getImportanceMap();

 protected:
    PhysicalSkySampler(RenderContext* pRenderContext);

    bool createSunTransmittanceLUT(RenderContext* pRenderContext);
    bool createMultipleScatteringLUT(RenderContext* pRenderContext);
    bool createSkyViewLUT(RenderContext* pRenderContext);
    bool createImportanceMap(RenderContext* pRenderContext, uint32_t dimension, uint32_t samples);

    uint2 mSunTrasmittanceLUTRes = {256, 64};
    uint2 mMultipleScatteringLUTRes = {32, 32};
    uint2 mSkyViewLUTRes = {256, 256};

    float3  mGroundAlbedo = float3(0.3);
    float   mGroundRadiusMM = 6.360;
    float   mAtmosphereRadiusMM = 6.460;
    float   mSunTransmittanceSteps = 40.0;
    float   mMulScattSteps = 20.0;
    int     mSqrtSamples = 8;

    ref<ComputePass>        mpSunTransmittanceLUTSetupPass;     ///< Sun Transmittance LUT creation compute program.
    ref<ComputePass>        mpMultipleScatteringLUTSetupPass;   ///< Multiple scattering LUT creation compute program.
    ref<ComputePass>        mpSkyViewLUTSetupPass;              ///< SkyView LUT creation compute program.
    ref<ComputePass>        mpImportanceMapSetupPass;           ///< Importance map creation compute program.

    ref<Device> mpDevice;

    ref<Texture>            mpSunTransmittanceLUT;
    ref<Texture>            mpMultipleScatteringLUT;
    ref<Texture>            mpSkyViewLUT;

    ref<Sampler>            mpLUTSampler;

    ref<Texture>            mpImportanceMap;    ///< Hierarchical importance map (luminance).
    ref<Sampler>            mpImportanceSampler;

    bool mDirty = true;
    bool mSV_LUT_Dirty = true;
    bool mMS_LUT_Dirty = true;
    bool mST_LUT_Dirty = true;
    bool mIS_LUT_Dirty = true;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_EXPERIMENTAL_SCENE_LIGHTS_PHYSICALSKYSAMPLER_H_
