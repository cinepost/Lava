#include "stdafx.h"
#include "FalseColorGenerator.h"
#include "ColorGenerationUtils.h"


namespace Falcor {

FalseColorGenerator::FalseColorGenerator(Device::SharedPtr pDevice, uint32_t numColors, const uint32_t* pSeed): mNumColors(numColors), mpDevice(pDevice) {
    bool solidAlpha = true;
    mpFalseColorsBuffer = generateRandomColorsBuffer(mpDevice, numColors, ResourceFormat::RGBA32Float, solidAlpha, pSeed);
}

FalseColorGenerator::SharedPtr FalseColorGenerator::create(Device::SharedPtr pDevice, uint32_t numColors, const uint32_t* pSeed) {
    return SharedPtr(new FalseColorGenerator(pDevice, numColors, pSeed));
}

void FalseColorGenerator::setShaderData(const ShaderVar& var) const {
    assert(var.isValid());

    // Set variables.
    var["numColors"] = mNumColors;

    // Bind resources.
    var["colorsBuffer"] = mpFalseColorsBuffer;
}

}  // namespace Falcor
