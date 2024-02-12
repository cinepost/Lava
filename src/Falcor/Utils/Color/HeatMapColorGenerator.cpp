#include "stdafx.h"
#include "HeatMapColorGenerator.h"


namespace Falcor {

HeatMapColorGenerator::HeatMapColorGenerator(Device::SharedPtr pDevice, HeatMapGradientType grad_type): mGradientType(grad_type), mpDevice(pDevice) {
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear)
        .setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    mpSampler = Sampler::create(mpDevice, samplerDesc);

    generateColorsTexture();
}

HeatMapColorGenerator::SharedPtr HeatMapColorGenerator::create(Device::SharedPtr pDevice) {
    return SharedPtr(new HeatMapColorGenerator(pDevice, HeatMapColorGenerator::HeatMapGradientType::FIVE_COLORS));
}

HeatMapColorGenerator::SharedPtr HeatMapColorGenerator::create(Device::SharedPtr pDevice, HeatMapGradientType grad_type) {
    return SharedPtr(new HeatMapColorGenerator(pDevice, grad_type));
}

void HeatMapColorGenerator::setShaderData(const ShaderVar& var) const {
    assert(var.isValid());

    var["numColors"] = mNumColors;
    var["halfTexelSize"] = 0.5f / static_cast<float>(mNumColors);
    var["uvScale"] = mUVScale;

    // Bind resources.
    var["colorsTexture"] = mpGradientColorsTexture;
    var["sampler"] = mpSampler;
}

void HeatMapColorGenerator::generateColorsTexture() {
    std::vector<float4> colors;

    switch(mGradientType) {
        case HeatMapGradientType::MONOCHROME:
            colors.push_back({0.0, 0.0, 0.0, 1.0});
            colors.push_back({1.0, 1.0, 1.0, 1.0});
            break;
        case HeatMapGradientType::TWO_COLORS:
            colors.push_back({0.0, 0.0, 1.0, 1.0});
            colors.push_back({1.0, 0.0, 0.0, 1.0});
            break;
        case HeatMapGradientType::SEVEN_COLORS:
            colors.push_back({0.0, 0.0, 0.0, 1.0});
            colors.push_back({0.0, 0.0, 1.0, 1.0});
            colors.push_back({0.0, 1.0, 1.0, 1.0});
            colors.push_back({0.0, 1.0, 0.0, 1.0});
            colors.push_back({1.0, 1.0, 0.0, 1.0});
            colors.push_back({1.0, 0.0, 0.0, 1.0});
            colors.push_back({1.0, 1.0, 1.0, 1.0});
            break;
        case HeatMapGradientType::FIVE_COLORS:
        default:
            colors.push_back({0.0, 0.0, 1.0, 1.0});
            colors.push_back({0.0, 1.0, 1.0, 1.0});
            colors.push_back({0.0, 1.0, 0.0, 1.0});
            colors.push_back({1.0, 1.0, 0.0, 1.0});
            colors.push_back({1.0, 0.0, 0.0, 1.0});
            break;
    }

    mNumColors = static_cast<uint32_t>(colors.size());
    float numColorsF = static_cast<float>(mNumColors);
    mUVScale = 1.0f - (1.0f / numColorsF);

    mpGradientColorsTexture = Texture::create1D(mpDevice, mNumColors, ResourceFormat::RGBA32Float, 1, 1, colors.data(), Resource::BindFlags::ShaderResource);
}

}  // namespace Falcor
