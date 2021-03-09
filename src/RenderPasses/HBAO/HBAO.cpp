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
#include <algorithm>

#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Utils/Textures/BlueNoiseTexture.h"
#include "HBAO.h"
#include "glm/gtc/random.hpp"

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

static void regHBAO(pybind11::module& m) {
    pybind11::class_<HBAO, RenderPass, HBAO::SharedPtr> pass(m, "HBAO");

    pass.def_property("sampleRadius", &HBAO::getSampleRadius, &HBAO::setSampleRadius);
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerClass("HBAO", "Screen-space ambient occlusion", HBAO::create);
    ScriptBindings::registerBinding(regHBAO);
}

const char* HBAO::kDesc = "Screen-space ambient occlusion. Can be used with and without a normal-map";

namespace {

const std::string kHorizonMapSize = "aoMapSize";
const std::string kKernelSize = "kernelSize";
const std::string kNoiseSize = "noiseSize";
const std::string kDistribution = "distribution";
const std::string kRadius = "radius";
const std::string kBlurKernelWidth = "blurWidth";
const std::string kBlurSigma = "blurSigma";

const std::string kAoOut = "aoOut";
const std::string kDepth = "depth";
const std::string kNormals = "normals";
const std::string kHorizonMap = "aoHorizons";
const std::string kMaxZ = "maxZBuffer";

const std::string kComputeHorizonshaderFile = "RenderPasses/HBAO/HBAO.ComputeHorizons.cs.slang";
const std::string kComputeAOShaderFile = "RenderPasses/HBAO/HBAO.ComputeAO.cs.slang";
const std::string kComputeDownSampleDepthShaderFile = "RenderPasses/HBAO/HBAO.MinMaxZ.cs.slang";
}


HBAO::HBAO(Device::SharedPtr pDevice): RenderPass(pDevice) {
  mpDownCopyDepthPass = ComputePass::create(pDevice, kComputeDownSampleDepthShaderFile, "main", {{"MAX_PASS", ""}, {"COPY_DEPTH", ""}});
  mpDownSampleDepthPass = ComputePass::create(pDevice, kComputeDownSampleDepthShaderFile, "main", {{"MAX_PASS", ""}}); 


  Program::DefineList horzns_defines = {};
  mpHorizonsSearchPass = ComputePass::create(pDevice, kComputeHorizonshaderFile, "main", horzns_defines); 

  Sampler::Desc samplerDesc;
  samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point)
      .setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap);
  mpNoiseSampler = Sampler::create(pDevice, samplerDesc);
  

  Sampler::Desc depthPointSamplerDesc;
  depthPointSamplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point)
      .setMaxAnisotropy(0)
      .setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
  
  mpPointSampler = Sampler::create(pDevice, samplerDesc); // Used for uninterpolated texel access

  samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
  mpTextureSampler = Sampler::create(pDevice, samplerDesc);

  Sampler::Desc depthSamplerDesc;
  depthSamplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point)
      .setMaxAnisotropy(0)
      .setComparisonMode(Sampler::ComparisonMode::Disabled)
      .setBorderColor({1.0, 1.0, 1.0, 1.0})
      .setAddressingMode(Sampler::AddressMode::Border, Sampler::AddressMode::Border, Sampler::AddressMode::Border);
  
  mpDepthSampler = Sampler::create(pDevice, depthSamplerDesc);  // depth sampler

}

HBAO::SharedPtr HBAO::create(RenderContext* pRenderContext, const Dictionary& dict) {
    SharedPtr pHBAO = SharedPtr(new HBAO(pRenderContext->device()));
    Dictionary blurDict;
    //for (const auto& v : dict) {
    //    if (v.key() == kHorizonMapSize) pHBAO->mAoMapSize = (uint2)v.val();
    //    else if (v.key() == kKernelSize) pHBAO->mData.kernelSize = v.val();
    //    else if (v.key() == kNoiseSize) pHBAO->mNoiseSize = (uint2)v.val();
    //    else if (v.key() == kDistribution) pHBAO->mHemisphereDistribution = (SampleDistribution)v.val();
    //    else if (v.key() == kRadius) pHBAO->mData.radius = v.val();
    //    else if (v.key() == kBlurKernelWidth) pHBAO->mBlurDict["kernelWidth"] = (uint32_t)v.val();
    //    else if (v.key() == kBlurSigma) pHBAO->mBlurDict["sigma"] = (float)v.val();
    //    else logWarning("Unknown field '" + v.key() + "' in a HBAO dictionary");
    //}
    return pHBAO;
}

Dictionary HBAO::getScriptingDictionary() {
    Dictionary dict;
    dict[kKernelSize] = mData.kernelSize;
    dict[kNoiseSize] = mNoiseSize;
    dict[kRadius] = mData.radius;

    return dict;
}

RenderPassReflection HBAO::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    reflector.addInput(kDepth, "Depth-buffer").bindFlags(ResourceBindFlags::ShaderResource);
    reflector.addInput(kNormals, "World space normals, [0, 1] range").flags(RenderPassReflection::Field::Flags::Optional);
    
    reflector.addInternal(kMaxZ, "Max Z buffer").bindFlags(ResourceBindFlags::ShaderResource)
      .format(ResourceFormat::R32Float)
      .texture2D(compileData.defaultTexDims[0]/2, compileData.defaultTexDims[1]/2, 1, RenderPassReflection::Field::kMaxMipLevels, 1);
    
    reflector.addOutput(kHorizonMap, "Horizons Map").bindFlags(ResourceBindFlags::UnorderedAccess).format(ResourceFormat::RGBA8Unorm);//RGBA8Unorm);

    return reflector;
}

void HBAO::compile(RenderContext* pRenderContext, const CompileData& compileData) {
  mFrameDim = compileData.defaultTexDims;
  auto pDevice = pRenderContext->device();

  mpNoiseOffsetGenerator = StratifiedSamplePattern::create(16);
  mpBlueNoiseTexture = BlueNoiseTexture::create(pDevice);
}

void HBAO::execute(RenderContext* pRenderContext, const RenderData& renderData) {
  if (!mpScene) return;

  auto pDevice = pRenderContext->device();
  auto pCamera = mpScene->getCamera();

  // Downcopy depth buffet to HiZ texture

  auto pHizTexture = renderData[kMaxZ]->asTexture();
  uint32_t hiZMipLevelsCount = pHizTexture->getMipCount();
  std::vector<int2> hiZmipSizes(std::max(10u, hiZMipLevelsCount), {1,1});

  uint2 hiZDims = {pHizTexture->getWidth(0), pHizTexture->getHeight(0)};
  hiZmipSizes[0] = {hiZDims[0], hiZDims[1]};

  mpDownCopyDepthPass["CB"]["gMipLevel"] = 0;
  mpDownCopyDepthPass["CB"]["frameDim"] = hiZDims;

  mpDownCopyDepthPass["gGatherSampler"] = mpDepthSampler;
  mpDownCopyDepthPass["gSourceBuffer"] = renderData[kDepth]->asTexture();
  mpDownCopyDepthPass["gOutputBuffer"] = renderData[kMaxZ]->asTexture();//.setUav(renderData[kMaxZ]->asTexture()->getUAV(0, 0, 1));// = renderData[kMaxZ]->asTexture();
  mpDownCopyDepthPass->execute(pRenderContext, hiZDims[0], hiZDims[1]);

  // Run Recursive Downsample depth pass (max depth)

  for(uint32_t mipLevel = 1; mipLevel < hiZMipLevelsCount; mipLevel++) {
      
    auto pMaxZBuffer = renderData[kMaxZ]->asTexture();

    int mip_width = pMaxZBuffer->getWidth(mipLevel);
    int mip_height = pMaxZBuffer->getHeight(mipLevel);

    hiZmipSizes[mipLevel] = int2({mip_width, mip_height});

    mpDownSampleDepthPass["CB"]["gMipLevel"] = mipLevel;
    mpDownSampleDepthPass["CB"]["frameDim"] = int2({mip_width, mip_height});

    mpDownSampleDepthPass["gGatherSampler"] = mpDepthSampler;
    mpDownSampleDepthPass["gSourceBuffer"].setSrv(pMaxZBuffer->getSRV(mipLevel - 1, 1, 0, 1));
    mpDownSampleDepthPass["gOutputBuffer"].setUav(pMaxZBuffer->getUAV(mipLevel, 0, 1));
    mpDownSampleDepthPass->execute(pRenderContext, mip_width, mip_height);
  }

  /**
   * Compute Mipmap texel alignment.
   */
  std::vector<float2> mipRatios(10);  
  for (int i = 0; i < 10; i++) {
    int2 mip_size;
    mipRatios[i][0] = mFrameDim.x / (hiZmipSizes[i].x * powf(2.0f, i));
    mipRatios[i][1] = mFrameDim.y / (hiZmipSizes[i].y * powf(2.0f, i));
  }

  // Generate horizons buffer
  float2 f = mpNoiseOffsetGenerator->next();
  uint2 noiseOffset = {64 * (f[0] + 0.5f), 64 * (f[1] + 0.5f)};

  auto cb = mpHorizonsSearchPass["PerFrameCB"];
  cb["gFrameDim"] = mFrameDim;
  cb["gNoiseOffset"] = noiseOffset;
  cb["gAoQuality"] = 0.0f; // 0 means highest possible quality
  cb["gHizMipOffset"] = 1; // For Half-Res HiZ use 1
  cb["gRotationOffset"] = 0.0f;
  cb["gAoDistance"] = 0.13f;

  //auto pcb = mpHorizonsSearchPass["PerFrameCB"];
  pCamera->setShaderData(cb["gCamera"]);

  mpHorizonsSearchPass["gPointSampler"] = mpPointSampler;
  mpHorizonsSearchPass["gDepthSampler"] = mpDepthSampler;
  mpHorizonsSearchPass["gNoiseSampler"] = mpNoiseSampler;

  auto pBuff = Buffer::createTyped<float2>(pDevice, 10, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, mipRatios.data());

  mpHorizonsSearchPass["mipRatio"] = pBuff;
  mpHorizonsSearchPass["gNoiseTex"] = mpBlueNoiseTexture;
  mpHorizonsSearchPass["gMaxZBuffer"] = renderData[kMaxZ]->asTexture();
  mpHorizonsSearchPass["gDepthTex"] = renderData[kDepth]->asTexture();
  mpHorizonsSearchPass["gOutput"] = renderData[kHorizonMap]->asTexture();
  mpHorizonsSearchPass->execute(pRenderContext, mFrameDim.x, mFrameDim.y);
}

void HBAO::setSampleRadius(float radius) {
    mData.radius = radius;
    mDirty = true;
}
