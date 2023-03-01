#include <algorithm>
#include <array>
#include <cmath>

#include "Falcor/Core/API/Formats.h"
#include "Falcor/Utils/Math/MathConstants.slangh"
#include "FilterKernelsLUT.h"

#ifndef M_PI
#define M_PI           3.14159265358979323846  /* pi */
#endif

namespace Falcor {

namespace Kernels {

namespace Gaussian {
    std::vector<float> createKernelData1D(uint32_t kernelHalfWidth, float sigma, bool generateHalfTable, NormalizationMode normalization) {
        assert(kernelHalfWidth > 0);
        uint32_t kernelWidth = generateHalfTable ? kernelHalfWidth : kernelHalfWidth + std::max(0u, kernelHalfWidth - 1u);
        std::vector<float> data(kernelWidth);

        float radius = generateHalfTable ? static_cast<float>(kernelWidth) : (static_cast<float>(kernelWidth) / 2.f);
        float expv = std::exp(-sigma);

        float nca = 0.f; // area normalization coefficient
        float ncp = 0.f; // peak normalization coefficient

        for(uint32_t i = 0; i < kernelHalfWidth; i++) {
            float v = static_cast<float>(i) / radius;
            data[i] = std::max(0.f, std::exp(-sigma * v * v) - expv);
            nca += data[i];
            ncp = std::max(ncp, data[i]);
        }

        // kernel normalization
        if(normalization != NormalizationMode::None) {
            if(normalization == NormalizationMode::Area) {
                if(nca == 0.f) {
                    LLOG_ERR << "Error area normalizing Gaussian kernel. Total area weight is 0 !";
                    return data;
                }
                for(auto& v: data) v /= nca;
            } else {
                if(nca == 0.f) {
                    LLOG_ERR << "Error peak normalizing Gaussian kernel. Total peak weight is 0 !";
                    return data;
                }
                for(auto& v: data) v /= ncp;
            }
        }

        return data;
    }

    Texture::SharedPtr createKernelTexture1D(Device::SharedPtr pDevice, uint32_t kernelHalfWidth, float sigma, bool generateHalfTable, NormalizationMode normalization) {
        std::vector<float> kernel = createKernelData1D(kernelHalfWidth, sigma, generateHalfTable, normalization);
        return Texture::create2D(pDevice, kernelHalfWidth, 1, ResourceFormat::R32Float, 1, 1, kernel.data());   
    }

    Texture::SharedPtr createKernelTexture2D(Device::SharedPtr pDevice, uint32_t kernelHalfWidth, uint32_t kernelHalfHeight, bool generateQuarterTable,  NormalizationMode normalization) {
        return nullptr;
    }
}  // namespace Gaussian

namespace Blackman {
    std::vector<float> createKernelData1D(uint32_t kernelHalfWidth, bool generateHalfTable, NormalizationMode normalization) {
        std::vector<float> data(kernelHalfWidth);

        return data;
    }

    Texture::SharedPtr createKernelTexture1D(Device::SharedPtr pDevice, uint32_t kernelHalfWidth, bool generateHalfTable, NormalizationMode normalization) {
        std::vector<float> kernel = createKernelData1D(kernelHalfWidth, generateHalfTable, normalization);
        return Texture::create2D(pDevice, kernelHalfWidth, 1, ResourceFormat::R32Float, 1, 1, kernel.data());   
    }

    Texture::SharedPtr createKernelTexture2D(Device::SharedPtr pDevice, uint32_t kernelHalfWidth, uint32_t kernelHalfHeight, bool generateQuarterTable, NormalizationMode normalization) {
        return nullptr;
    }
}  // namespace Blackman

namespace Mitchell {

    std::vector<float> createKernelData1D(uint32_t kernelHalfWidth, float B, float C, bool generateHalfTable, NormalizationMode normalization) {
        assert(kernelHalfWidth > 0);
        uint32_t kernelWidth = generateHalfTable ? kernelHalfWidth : kernelHalfWidth + std::max(0u, kernelHalfWidth - 1u);
        std::vector<float> data(kernelWidth);

        float radius = generateHalfTable ? static_cast<float>(kernelWidth) : (static_cast<float>(kernelWidth) / 2.f);
        
        float nca = 0.f; // area normalization coefficient
        float ncp = 0.f; // peak normalization coefficient

        for(uint32_t i = 0; i < kernelHalfWidth; i++) {
            float v = static_cast<float>(i) / radius;
            
            v = std::abs(2.f * v);
            if (v > 1)
                data[i] = ((-B - 6*C) * v*v*v + (6*B + 30*C) * v*v + (-12*B - 48*C) * v + (8*B + 24*C)) * (1.f/6.f);
            else
                data[i] = ((12 - 9*B - 6*C) * v*v*v + (-18 + 12*B + 6*C) * v*v + (6 - 2*B)) * (1.f/6.f);

            nca += data[i];
            ncp = std::max(ncp, data[i]);
        }

        // kernel normalization
        if(normalization != NormalizationMode::None) {
            if(normalization == NormalizationMode::Area) {
                if(nca == 0.f) {
                    LLOG_ERR << "Error area normalizing Mitchell kernel. Total area weight is 0 !";
                    return data;
                }
                for(auto& v: data) v /= nca;
            } else {
                if(nca == 0.f) {
                    LLOG_ERR << "Error peak normalizing Mitchell kernel. Total peak weight is 0 !";
                    return data;
                }
                for(auto& v: data) v /= ncp;
            }
        }

        return data;
    }

    Texture::SharedPtr createKernelTexture1D(Device::SharedPtr pDevice, uint32_t kernelHalfWidth, float B, float C, bool generateHalfTable, NormalizationMode normalization) {
        std::vector<float> kernel = createKernelData1D(kernelHalfWidth, B, C, generateHalfTable, normalization);
        return Texture::create2D(pDevice, kernelHalfWidth, 1, ResourceFormat::R32Float, 1, 1, kernel.data());   
    }

    Texture::SharedPtr createKernelTexture2D(Device::SharedPtr pDevice, uint32_t kernelHalfWidth, uint32_t kernelHalfHeight, bool generateQuarterTable, NormalizationMode normalization) {
        return nullptr;
    }
}  // namespace Mitchell

namespace Sinc {

    float sinc(float v) {
        v = std::abs(v);
        if (v < 1e-5) return 1.f;
        return std::sin(M_PI * v) / (M_PI * v);
    }

    std::vector<float> createKernelData1D(uint32_t kernelHalfWidth, float T, bool generateHalfTable, NormalizationMode normalization) {
        assert(kernelHalfWidth > 0);
        uint32_t kernelWidth = generateHalfTable ? kernelHalfWidth : kernelHalfWidth + std::max(0u, kernelHalfWidth - 1u);
        std::vector<float> data(kernelWidth);

        float radius = generateHalfTable ? static_cast<float>(kernelWidth) : (static_cast<float>(kernelWidth) / 2.f);
        
        float nca = 0.f; // area normalization coefficient
        float ncp = 0.f; // peak normalization coefficient

        for(uint32_t i = 0; i < kernelHalfWidth; i++) {
            float v = static_cast<float>(i) / radius;
            
            v = std::abs(v);
            if (v > 1.f) {
                data[i] = 0.f;
            } else {
                float lanczos = sinc(v * T);
                data[i] = sinc(v) * lanczos;
            }

            nca += data[i];
            ncp = std::max(ncp, data[i]);
        }

        // kernel normalization
        if(normalization != NormalizationMode::None) {
            if(normalization == NormalizationMode::Area) {
                if(nca == 0.f) {
                    LLOG_ERR << "Error area normalizing Mitchell kernel. Total area weight is 0 !";
                    return data;
                }
                for(auto& v: data) v /= nca;
            } else {
                if(nca == 0.f) {
                    LLOG_ERR << "Error peak normalizing Mitchell kernel. Total peak weight is 0 !";
                    return data;
                }
                for(auto& v: data) v /= ncp;
            }
        }

        return data;
    }

    Texture::SharedPtr createKernelTexture1D(Device::SharedPtr pDevice, uint32_t kernelHalfWidth, float T, bool generateHalfTable, NormalizationMode normalization) {
        std::vector<float> kernel = createKernelData1D(kernelHalfWidth, T, generateHalfTable, normalization);
        return Texture::create2D(pDevice, kernelHalfWidth, 1, ResourceFormat::R32Float, 1, 1, kernel.data());   
    }

    Texture::SharedPtr createKernelTexture2D(Device::SharedPtr pDevice, uint32_t kernelHalfWidth, uint32_t kernelHalfHeight, bool generateQuarterTable, NormalizationMode normalization) {
        return nullptr;
    }
}  // namespace Sinc


}  // namespace Kernels

}  // namespace Falcor


