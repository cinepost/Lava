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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
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
#include "stdafx.h"

#include "glm/gtc/integer.hpp"

#include "Falcor/Core/API/RenderContext.h"
#include "PhysicalSkySampler.h"


#define OUTPUT_DEBUG_MAPS

namespace Falcor {

namespace {
    const char kSTShaderFilenameSetup[] = "Experimental/Scene/Lights/PhysicalSkySamplerSTSetup.cs.slang";
    const char kMSShaderFilenameSetup[] = "Experimental/Scene/Lights/PhysicalSkySamplerMSSetup.cs.slang";
    const char kSVShaderFilenameSetup[] = "Experimental/Scene/Lights/PhysicalSkySamplerSVSetup.cs.slang";
    const char kISShaderFilenameSetup[] = "Experimental/Scene/Lights/PhysicalSkySamplerISSetup.cs.slang";

    // The defaults are 512x512 @ 64spp in the resampling step.
    const uint32_t kDefaultDimension = 512;
    const uint32_t kDefaultSpp = 64;
}

PhysicalSkySampler::SharedPtr PhysicalSkySampler::create(RenderContext* pRenderContext) {
    return SharedPtr(new PhysicalSkySampler(pRenderContext));
}

void PhysicalSkySampler::setShaderData(const ShaderVar& var) const {
    assert(var.isValid());

    // Set variables.
    float2 invDim = 1.f / float2(mpImportanceMap->getWidth(), mpImportanceMap->getHeight());
    var["importanceBaseMip"] = mpImportanceMap->getMipCount() - 1; // The base mip is 1x1 texels
    var["importanceInvDim"] = invDim;

    // Bind resources.
    var["importanceMap"] = mpImportanceMap;
    var["importanceSampler"] = mpImportanceSampler;
}

PhysicalSkySampler::PhysicalSkySampler(RenderContext* pRenderContext) {
    mpDevice = pRenderContext->device();

    // Multiple scattering LUT creation compute program.
    mpSunTransmittanceLUTSetupPass = ComputePass::create(mpDevice, kSTShaderFilenameSetup, "main");

    // Multiple scattering LUT creation compute program.
    mpMultipleScatteringLUTSetupPass = ComputePass::create(mpDevice, kMSShaderFilenameSetup, "main");

    // SkyView LUT creation compute program.
    mpSkyViewLUTSetupPass = ComputePass::create(mpDevice, kSVShaderFilenameSetup, "main");

    // Importance map creation compute program.
    mpImportanceMapSetupPass = ComputePass::create(mpDevice, kISShaderFilenameSetup, "main");

    // Create importance map sampler.
    {
        Sampler::Desc samplerDesc;
        samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
        samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
        mpImportanceSampler = Sampler::create(mpDevice, samplerDesc);
    }

    // Create lut map sampler.
    {
        Sampler::Desc samplerDesc;
        samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
        samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
        mpLUTSampler = Sampler::create(mpDevice, samplerDesc);
    }
}

bool PhysicalSkySampler::createSunTransmittanceLUT(RenderContext* pRenderContext) {
    if(!mST_LUT_Dirty && !mDirty) return true;
    mMS_LUT_Dirty = true;
    mIS_LUT_Dirty = true;

    static const Resource::BindFlags flags = Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess;
    uint32_t mipLevels = 1;
    uint32_t mapWidth = mSunTrasmittanceLUTRes[0];
    uint32_t mapHeight = mSunTrasmittanceLUTRes[1];
    if(!mpSunTransmittanceLUT || (mpSunTransmittanceLUT->getWidth() != mapWidth) || (mpSunTransmittanceLUT->getHeight() != mapHeight)) {
        mpSunTransmittanceLUT = Texture::create2D(mpDevice, mapWidth, mapHeight, ResourceFormat::RGBA32Float, 1, mipLevels, nullptr, flags);
        assert(mpSunTransmittanceLUT);
    }

    mpSunTransmittanceLUTSetupPass["gSunTransmittanceMap"] = mpSunTransmittanceLUT;

    mpSunTransmittanceLUTSetupPass["CB"]["stMapDim"] = mSunTrasmittanceLUTRes;
    mpSunTransmittanceLUTSetupPass["CB"]["msMapDim"] = mMultipleScatteringLUTRes;
    mpSunTransmittanceLUTSetupPass["CB"]["svMapDim"] = mSkyViewLUTRes;
    mpSunTransmittanceLUTSetupPass["CB"]["groundRadiusMM"] = mGroundRadiusMM;
    mpSunTransmittanceLUTSetupPass["CB"]["atmosphereRadiusMM"] = mAtmosphereRadiusMM;
    mpSunTransmittanceLUTSetupPass["CB"]["sunTransmittanceSteps"] = mSunTransmittanceSteps;

    mpSunTransmittanceLUTSetupPass->execute(pRenderContext, mapWidth, mapHeight);

#ifdef OUTPUT_DEBUG_MAPS
    auto pInputTex = mpSunTransmittanceLUT;
    auto pOutputTex = Texture::create2D(mpDevice, mapWidth, mapHeight, ResourceFormat::RGBA16Float, 1, mipLevels, nullptr, flags);
    pRenderContext->blit(pInputTex->getSRV(0, 1, 0, 1), pOutputTex->getRTV(0, 0, 1));
    pOutputTex->captureToFile(0, 0, "/home/max/Desktop/phys_sun_trans_lut_test.exr", Bitmap::FileFormat::ExrFile, Bitmap::ExportFlags::None);
#endif

    mST_LUT_Dirty = false;

    return true;
}

bool PhysicalSkySampler::createMultipleScatteringLUT(RenderContext* pRenderContext) {
    if(!mMS_LUT_Dirty && !mDirty) return true;
    mSV_LUT_Dirty = true;
    mIS_LUT_Dirty = true;

    static const Resource::BindFlags flags = Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess;
    uint32_t mipLevels = 1;
    uint32_t mapWidth = mMultipleScatteringLUTRes[0];
    uint32_t mapHeight = mMultipleScatteringLUTRes[1];
    if(!mpMultipleScatteringLUT || (mpMultipleScatteringLUT->getWidth() != mapWidth) || (mpMultipleScatteringLUT->getHeight() != mapHeight)) {
        mpMultipleScatteringLUT = Texture::create2D(mpDevice, mapWidth, mapHeight, ResourceFormat::RGBA32Float, 1, mipLevels, nullptr, flags);
        assert(mpMultipleScatteringLUT);
    }

    mpMultipleScatteringLUTSetupPass["gSunTransmittanceMap"] = mpSunTransmittanceLUT;
    mpMultipleScatteringLUTSetupPass["gMultiScatterMap"] = mpMultipleScatteringLUT;
    mpMultipleScatteringLUTSetupPass["gLUTSampler"] = mpLUTSampler;

    mpMultipleScatteringLUTSetupPass["CB"]["stMapDim"] = mSunTrasmittanceLUTRes;
    mpMultipleScatteringLUTSetupPass["CB"]["msMapDim"] = mMultipleScatteringLUTRes;
    mpMultipleScatteringLUTSetupPass["CB"]["svMapDim"] = mSkyViewLUTRes;
    mpMultipleScatteringLUTSetupPass["CB"]["groundRadiusMM"] = mGroundRadiusMM;
    mpMultipleScatteringLUTSetupPass["CB"]["atmosphereRadiusMM"] = mAtmosphereRadiusMM;
    mpMultipleScatteringLUTSetupPass["CB"]["groundAlbedo"] = mGroundAlbedo;
    mpMultipleScatteringLUTSetupPass["CB"]["mulScattSteps"] = mMulScattSteps;
    mpMultipleScatteringLUTSetupPass["CB"]["sqrtSamples"] = mSqrtSamples;

    mpMultipleScatteringLUTSetupPass->execute(pRenderContext, mapWidth, mapHeight);

#ifdef OUTPUT_DEBUG_MAPS
    auto pInputTex = mpMultipleScatteringLUT;
    auto pOutputTex = Texture::create2D(mpDevice, mapWidth, mapHeight, ResourceFormat::RGBA16Float, 1, mipLevels, nullptr, flags);
    pRenderContext->blit(pInputTex->getSRV(0, 1, 0, 1), pOutputTex->getRTV(0, 0, 1));
    pOutputTex->captureToFile(0, 0, "/home/max/Desktop/phys_multi_scatt_lut_test.exr", Bitmap::FileFormat::ExrFile, Bitmap::ExportFlags::None);
#endif

    mMS_LUT_Dirty = false;

    return true;
}

bool PhysicalSkySampler::createSkyViewLUT(RenderContext* pRenderContext) {
    if(!mSV_LUT_Dirty && !mDirty) return true;
    mIS_LUT_Dirty = true;
    
    static const Resource::BindFlags flags = Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess;
    uint32_t mipLevels = 1;
    uint32_t mapWidth = mSkyViewLUTRes[0];
    uint32_t mapHeight = mSkyViewLUTRes[1];
    if(!mpSkyViewLUT || (mpSkyViewLUT->getWidth() != mapWidth) || (mpSkyViewLUT->getHeight() != mapHeight)) {
        mpSkyViewLUT = Texture::create2D(mpDevice, mapWidth, mapHeight, ResourceFormat::RGBA32Float, 1, mipLevels, nullptr, flags);
        assert(mpSkyViewLUT);
    }

    mpSkyViewLUTSetupPass["gSunTransmittanceMap"] = mpSunTransmittanceLUT;
    mpSkyViewLUTSetupPass["gMultiScatterMap"] = mpMultipleScatteringLUT;
    mpSkyViewLUTSetupPass["gSkyViewMap"] = mpSkyViewLUT;
    mpSkyViewLUTSetupPass["gLUTSampler"] = mpLUTSampler;

    mpSkyViewLUTSetupPass["CB"]["stMapDim"] = mSunTrasmittanceLUTRes;
    mpSkyViewLUTSetupPass["CB"]["msMapDim"] = mMultipleScatteringLUTRes;
    mpSkyViewLUTSetupPass["CB"]["svMapDim"] = mSkyViewLUTRes;

    mpSkyViewLUTSetupPass->execute(pRenderContext, mapWidth, mapHeight);

#ifdef OUTPUT_DEBUG_MAPS
    auto pInputTex = mpSkyViewLUT;
    auto pOutputTex = Texture::create2D(mpDevice, mapWidth, mapHeight, ResourceFormat::RGBA16Float, 1, mipLevels, nullptr, flags);
    pRenderContext->blit(pInputTex->getSRV(0, 1, 0, 1), pOutputTex->getRTV(0, 0, 1));
    pOutputTex->captureToFile(0, 0, "/home/max/Desktop/phys_sky_view_lut_test.exr", Bitmap::FileFormat::ExrFile, Bitmap::ExportFlags::None);
#endif
    mSV_LUT_Dirty = false;

    return true;
}

bool PhysicalSkySampler::createImportanceMap(RenderContext* pRenderContext, uint32_t dimension, uint32_t samples) {
    // Create sun transmittance lut.
    if (!createSunTransmittanceLUT(pRenderContext)) {
        LLOG_ERR << "Failed to create sun transmittance LUT !!!";
        mDirty = true;
        return false;
    }

    // Create multiple scattering lut.
    if (!createMultipleScatteringLUT(pRenderContext)) {
        LLOG_ERR << "Failed to create multiple scattering LUT !!!";
        mDirty = true;
        return false;
    }

    // Create sky view lut.
    if (!createSkyViewLUT(pRenderContext)) {
        LLOG_ERR << "Failed to create sky view LUT !!!";
        mDirty = true;
        return false;
    }

    if(!mIS_LUT_Dirty && !mDirty) return true;

    assert(isPowerOf2(dimension));
    assert(isPowerOf2(samples));
    assert(pRenderContext->device() == mpDevice);

    // We create log2(N)+1 mips from NxN...1x1 texels resolution.
    uint32_t mips = glm::log2(dimension) + 1;
    assert((1u << (mips - 1)) == dimension);
    assert(mips > 1 && mips <= 12);     // Shader constant limits max resolution, increase if needed.

    // Create importance map. We have to set the RTV flag to be able to use generateMips().
    mpImportanceMap = Texture::create2D(mpDevice, dimension, dimension, ResourceFormat::R32Float, 1, mips, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess);
    assert(mpImportanceMap);

    mpImportanceMapSetupPass["gImportanceMap"] = mpImportanceMap;
    mpImportanceMapSetupPass["importanceSampler"] = mpImportanceSampler;

    uint32_t samplesX = std::max(1u, (uint32_t)std::sqrt(samples));
    uint32_t samplesY = samples / samplesX;
    assert(samples == samplesX * samplesY);

    mpImportanceMapSetupPass["CB"]["outputDim"] = uint2(dimension);
    mpImportanceMapSetupPass["CB"]["outputDimInSamples"] = uint2(dimension * samplesX, dimension * samplesY);
    mpImportanceMapSetupPass["CB"]["numSamples"] = uint2(samplesX, samplesY);
    mpImportanceMapSetupPass["CB"]["invSamples"] = 1.f / (samplesX * samplesY);

    // Execute setup pass to compute the square importance map (base mip).
    mpImportanceMapSetupPass->execute(pRenderContext, dimension, dimension);

    // Populate mip hierarchy. We rely on the default mip generation for this.
    mpImportanceMap->generateMips(pRenderContext);

    mpImportanceMap->captureToFile(0, 0, "/home/max/Desktop/phys_imp_test.exr", Bitmap::FileFormat::ExrFile, Bitmap::ExportFlags::None);

    mIS_LUT_Dirty = mDirty = false;

    return true;
}

const Texture::SharedPtr& PhysicalSkySampler::getImportanceMap() { 
    // Create hierarchical importance map for sampling.
    if (!createImportanceMap(mpDevice->getRenderContext(), kDefaultDimension, kDefaultSpp)) {
        LLOG_ERR << "Failed to create importance map !!!";
        mDirty = true;
    } 

    return mpImportanceMap;
}

}  // namespace Falcor
