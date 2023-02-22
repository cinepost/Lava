#include <algorithm>
#include <array>
#include <cmath>

#include "Falcor/Core/API/Formats.h"
#include "Falcor/Utils/Math/MathConstants.slangh"
#include "FilterKernelsLUT.h"

namespace Falcor {

namespace Gaussian {
    Texture::SharedPtr createKernelTexture1D(Device::SharedPtr pDevice, uint32_t lutWidth, bool halfSizeTable) {
        std::vector<float> kernel(lutWidth);

        float k = 6.0f / static_cast<float>(lutWidth);

        for(uint32_t i = 0; i < lutWidth; i++) {
            float v = static_cast<float>(i) * k;
            kernel[i] = std::exp(-2.0f * v * v);
        }

        return Texture::create2D(pDevice, lutWidth, 1, ResourceFormat::R32Float, 1, 1, kernel.data());   
    }

    Texture::SharedPtr createKernelTexture2D(Device::SharedPtr pDevice, uint32_t lutWidth, uint32_t lutHeight, bool quarterSizeTable) {
        return nullptr;
    }
}

namespace Blackman {
    Texture::SharedPtr createKernelTexture1D(Device::SharedPtr pDevice, uint32_t lutWidth, bool halfSizeTable) {
        std::vector<float> kernel(lutWidth);

        for(uint32_t i = 0; i < lutWidth; i++) {
            float v =  M_2_PI * (static_cast<float>(i) / static_cast<float>(lutWidth) + 0.5f);
            kernel[i] = 0.35875f - 0.48829f*std::cos(v) + 0.14128f*std::cos(2.0f*v) - 0.01168f*std::cos(3.0f*v);;
        }

        return Texture::create2D(pDevice, lutWidth, 1, ResourceFormat::R32Float, 1, 1, kernel.data());   
    }

    Texture::SharedPtr createKernelTexture2D(Device::SharedPtr pDevice, uint32_t lutWidth, uint32_t lutHeight, bool quarterSizeTable) {
        return nullptr;
    }
}

}


