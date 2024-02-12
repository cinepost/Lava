#ifndef SRC_FALCOR_UTILS_COLOR_HEATMAPCOLORGENERATOR_H_
#define SRC_FALCOR_UTILS_COLOR_HEATMAPCOLORGENERATOR_H_

#include "Falcor/Core/API/Sampler.h"
#include "Falcor/Core/Program/Program.h"
#include "Falcor/Core/Program/ShaderVar.h"


namespace Falcor {


class dlldecl HeatMapColorGenerator : public std::enable_shared_from_this<HeatMapColorGenerator> {
public:

    enum class HeatMapGradientType: uint8_t {
        MONOCHROME,
        TWO_COLORS,
        FIVE_COLORS,
        SEVEN_COLORS
    };

    using SharedPtr = std::shared_ptr<HeatMapColorGenerator>;
    using SharedConstPtr = std::shared_ptr<const HeatMapColorGenerator>;

    static SharedPtr create(Device::SharedPtr pDevice);
    static SharedPtr create(Device::SharedPtr pDevice, HeatMapGradientType grad_type);

    void setShaderData(ShaderVar const& var) const;

protected:
    HeatMapColorGenerator(Device::SharedPtr pDevice, HeatMapGradientType grad_type);

    void generateColorsTexture();

private:
    HeatMapGradientType mGradientType;
    uint32_t            mNumColors = 0;
    float               mUVScale;

    Device::SharedPtr   mpDevice;
    Texture::SharedPtr  mpGradientColorsTexture;
    Sampler::SharedPtr  mpSampler;
    
};

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_COLOR_HEATMAPCOLORGENERATOR_H_
