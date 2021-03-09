#include "stdafx.h"

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/Formats.h"

#include "BlueNoiseData.h"
#include "BlueNoiseTexture.h"

namespace Falcor {

namespace BlueNoiseTexture {

Texture::SharedPtr create(Device::SharedPtr pDevice) {
    return Texture::create2D(pDevice, 64, 64, ResourceFormat::RGBA32Float, 1, 1, (float *)utils_tex_blue_noise_data);
};

}

}
