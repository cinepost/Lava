#ifndef SRC_FALCOR_EXPERIMENTAL_SCENE_LIGHTS_PHYSICALSKYSAMPLER_H_
#define SRC_FALCOR_EXPERIMENTAL_SCENE_LIGHTS_PHYSICALSKYSAMPLER_H_

#include "Falcor/RenderGraph/BasePasses/ComputePass.h"
#include "Falcor/Utils/Timing/Profiler.h"

namespace Falcor {

class Device;

/** Environment map sampler.
    Utily class for sampling and evaluating radiance stored in an omnidirectional environment map.
*/
class dlldecl PhysicalSkySampler : public std::enable_shared_from_this<PhysicalSkySampler> {
 public:
    using SharedPtr = std::shared_ptr<PhysicalSkySampler>;

    virtual ~PhysicalSkySampler() = default;

    /** Create a new object.
        \param[in] pRenderContext A render-context that will be used for processing.
    */
    static SharedPtr create(RenderContext* pRenderContext);
    
    /** Bind the environment map sampler to a given shader variable.
        \param[in] var Shader variable.
    */
    void setShaderData(const ShaderVar& var) const;

    const Texture::SharedPtr& getImportanceMap();

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

    ComputePass::SharedPtr  mpSunTransmittanceLUTSetupPass;     ///< Sun Transmittance LUT creation compute program.
    ComputePass::SharedPtr  mpMultipleScatteringLUTSetupPass;   ///< Multiple scattering LUT creation compute program.
    ComputePass::SharedPtr  mpSkyViewLUTSetupPass;              ///< SkyView LUT creation compute program.
    ComputePass::SharedPtr  mpImportanceMapSetupPass;           ///< Importance map creation compute program.

    std::shared_ptr<Device> mpDevice;

    Texture::SharedPtr      mpSunTransmittanceLUT;
    Texture::SharedPtr      mpMultipleScatteringLUT;
    Texture::SharedPtr      mpSkyViewLUT;

    Sampler::SharedPtr      mpLUTSampler;

    Texture::SharedPtr      mpImportanceMap;    ///< Hierarchical importance map (luminance).
    Sampler::SharedPtr      mpImportanceSampler;

    bool mDirty = true;
    bool mSV_LUT_Dirty = true;
    bool mMS_LUT_Dirty = true;
    bool mST_LUT_Dirty = true;
    bool mIS_LUT_Dirty = true;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_EXPERIMENTAL_SCENE_LIGHTS_PHYSICALSKYSAMPLER_H_
