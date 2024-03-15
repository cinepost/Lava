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
#include "Falcor/Utils/Color/ColorGenerationUtils.h"
#include "Falcor/Utils/Math/Float16.h"

#include "lava_utils_lib/logging.h"

/** Color generation utility functions.

*/

static std::random_device rd;
static std::mt19937 rndEngine(rd());


namespace Falcor {

static inline Buffer::SharedPtr generateRandomColorsBufferF16(Device::SharedPtr pDevice, uint32_t elementsCount, bool solidAlpha, const uint32_t* pSeed) {
    if(pSeed) rndEngine.seed(*pSeed);
    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);
    float16_t4 prevColorVal;

    std::vector<float16_t4> colorVector(elementsCount);

    static const float minComponentDist(0.04f);
    static const float minColorLuminance(0.1f);

    for(auto& colorVal: colorVector) {
        prevColorVal = colorVal = {0.0f, 0.0f, 0.0f, solidAlpha ? 1.0f : 0.0f};

        while ((float(colorVal[0] + colorVal[1] + colorVal[2]) < minColorLuminance) ||
            (abs(float(colorVal[0] - prevColorVal[0])) < minComponentDist) ||
            (abs(float(colorVal[1] - prevColorVal[1])) < minComponentDist) ||
            (abs(float(colorVal[2] - prevColorVal[2])) < minComponentDist)
        ){
            prevColorVal = colorVal;
            colorVal[0] = (float16_t)rndDist(rndEngine);
            colorVal[1] = (float16_t)rndDist(rndEngine);
            colorVal[2] = (float16_t)rndDist(rndEngine);
        }

    }

    return Buffer::create(pDevice, sizeof(float16_t) * colorVector.size(), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, colorVector.data());
}

static inline Buffer::SharedPtr generateRandomColorsBufferF32(Device::SharedPtr pDevice, uint32_t elementsCount, bool solidAlpha, const uint32_t* pSeed) {
    if(pSeed) rndEngine.seed(*pSeed);

    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);
    
    using float32_t4 = std::array<float, 4>;
    float32_t4 prevColorVal;

    std::vector<float32_t4> colorVector(elementsCount);

    static const float minComponentDist(0.2f);
    static const float minColorLuminance(0.1f);

    for(auto& colorVal: colorVector) {
        while ((((colorVal[0] + colorVal[1] + colorVal[2])*.333f) < minColorLuminance) ||
            (abs(colorVal[0] - prevColorVal[0]) < minComponentDist) ||
            (abs(colorVal[1] - prevColorVal[1]) < minComponentDist) ||
            (abs(colorVal[2] - prevColorVal[2]) < minComponentDist)
        ){
            prevColorVal = colorVal;
            colorVal[0] = rndDist(rndEngine);
            colorVal[1] = rndDist(rndEngine);
            colorVal[2] = rndDist(rndEngine);
        }

        colorVal[3] = solidAlpha ? 1.0f : rndDist(rndEngine);
        prevColorVal = colorVal;
    }

    return Buffer::create(pDevice, sizeof(float32_t4) * colorVector.size(), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, colorVector.data());
}

static inline Buffer::SharedPtr generateRandomColorsBufferUInt32(Device::SharedPtr pDevice, uint32_t elementsCount, bool solidAlpha) {
    std::uniform_int_distribution<uint32_t> rndDist(0, std::numeric_limits<uint32_t>::max());

}

static inline Buffer::SharedPtr generateRandomColorsBufferUInt16(Device::SharedPtr pDevice, uint32_t elementsCount, bool solidAlpha) {
    std::uniform_int_distribution<uint16_t> rndDist(0, std::numeric_limits<uint16_t>::max());

}

#ifndef _WIN32
static inline Buffer::SharedPtr generateRandomColorsBufferUInt8(Device::SharedPtr pDevice, uint32_t elementsCount, bool solidAlpha) {
    std::uniform_int_distribution<uint8_t>  rndDist(0, std::numeric_limits<uint8_t>::max());

}
#endif

Buffer::SharedPtr generateRandomColorsBuffer(Device::SharedPtr pDevice, uint32_t elementsCount, ResourceFormat colorFormat, bool solidAlpha, const uint32_t* pSeed) {
    Buffer::SharedPtr pBuffer;

    if( getFormatChannelCount(colorFormat) != 4) {
        LLOG_FN_ERR << "Unsupported channels count " << getFormatChannelCount(colorFormat);
        return nullptr;
    }

    const auto formatType = getFormatType(colorFormat);

    switch(formatType) {
        case FormatType::Float:
            if( isHalfFloatFormat(colorFormat)) { 
                pBuffer = generateRandomColorsBufferF16(pDevice, elementsCount, solidAlpha, pSeed); 
            } else { 
                pBuffer = generateRandomColorsBufferF32(pDevice, elementsCount, solidAlpha, pSeed); 
            }
            break;
        case FormatType::Uint:
            break;
        case FormatType::Sint:
            break;
        default:
            LLOG_FN_ERR << "Unsupported format type " << to_string(formatType);
            return nullptr;
    }

    /*

    std::random_device rd;
    std::mt19937 rndEngine(rd());
    std::uniform_int_distribution<uint32_t> rndDist(0, 255);
    uint8_t rndVal[4] = { 0, 0, 0, 0 };
    while (rndVal[0] + rndVal[1] + rndVal[2] < 10) {
        rndVal[0] = (uint8_t)rndDist(rndEngine);
        rndVal[1] = (uint8_t)rndDist(rndEngine);
        rndVal[2] = (uint8_t)rndDist(rndEngine);
    }
    rndVal[3] = 255;
    for (uint32_t i = 0; i < bufferSize; ++i) {
        for (uint32_t c = 0; c < 4; c++, ++buffer) {
            *buffer = rndVal[c];
        }
    }
    */

    if(!pBuffer) LLOG_ERR << "Error generating random colors buffer of format " << to_string(colorFormat) << " !";

    return pBuffer;
}

}  // namespace Falcor
