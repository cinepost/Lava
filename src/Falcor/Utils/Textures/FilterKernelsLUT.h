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
#ifndef SRC_FALCOR_UTILS_TEXTURES_FILTERKERNELSLUT_H_
#define SRC_FALCOR_UTILS_TEXTURES_FILTERKERNELSLUT_H_

#include <memory>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Texture.h"

namespace Falcor {

namespace Kernels {

enum class NormalizationMode: uint8_t {
  None = 0,
  Area = 1,
  Peak = 2
};

namespace Gaussian {
  std::vector<float> createKernelData1D(uint32_t kernelHalfWidth, float sigma= 6.f, bool generateHalfTable = true, NormalizationMode normalization = NormalizationMode::Peak);
  
  Texture::SharedPtr createKernelTexture1D(
    Device::SharedPtr pDevice, 
    uint32_t kernelHalfWidth, 
    float sigma = 6.f, 
    bool generateHalfTable = true, 
    NormalizationMode normalization = NormalizationMode::Peak);
  
  Texture::SharedPtr createKernelTexture2D(
    Device::SharedPtr pDevice, 
    uint32_t kernelHalfWidth, 
    uint32_t kernelHalfHeight, 
    bool generateQuarterTable = true, 
    NormalizationMode normalization = NormalizationMode::Peak);
}

namespace Blackman {
  std::vector<float> createKernelData1D(uint32_t kernelHalfWidth, bool generateHalfTable = true, bool normalize = true);
  
  Texture::SharedPtr createKernelTexture1D(
    Device::SharedPtr pDevice, 
    uint32_t kernelHalfWidth, 
    bool generateHalfTable = true, 
    NormalizationMode normalization = NormalizationMode::Peak);
  
  Texture::SharedPtr createKernelTexture2D(
    Device::SharedPtr pDevice, 
    uint32_t kernelHalfWidth, 
    uint32_t kernelHalfHeight, 
    bool generateQuarterTable = true, 
    NormalizationMode normalization = NormalizationMode::Peak);
}

namespace Mitchell {
  std::vector<float> createKernelData1D(uint32_t kernelHalfWidth, float B, float C, bool generateHalfTable = true, bool normalize = true);
  
  Texture::SharedPtr createKernelTexture1D(
    Device::SharedPtr pDevice, 
    uint32_t kernelHalfWidth, 
    float B, 
    float C, 
    bool generateHalfTable = true, 
    NormalizationMode normalization = NormalizationMode::Peak);
  
  Texture::SharedPtr createKernelTexture2D(
    Device::SharedPtr pDevice, 
    uint32_t kernelHalfWidth, 
    uint32_t kernelHalfHeight, 
    float B, 
    float C, 
    bool generateQuarterTable = true, 
    NormalizationMode normalization = NormalizationMode::Peak);
}

namespace Sinc {
  std::vector<float> createKernelData1D(uint32_t kernelHalfWidth, float T = 3.f, bool generateHalfTable = true, bool normalize = true);
  
  Texture::SharedPtr createKernelTexture1D(
    Device::SharedPtr pDevice, 
    uint32_t kernelHalfWidth, 
    float T = 3.f, 
    bool generateHalfTable = true, 
    NormalizationMode normalization = NormalizationMode::Peak);
  
  Texture::SharedPtr createKernelTexture2D(
    Device::SharedPtr pDevice, 
    uint32_t kernelHalfWidth, 
    uint32_t kernelHalfHeight, 
    float T, 
    bool generateQuarterTable = true, 
    NormalizationMode normalization = NormalizationMode::Peak);
}

}

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_TEXTURES_FILTERKERNELSLUT_H_
