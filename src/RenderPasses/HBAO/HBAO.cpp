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

    pass.def_property("distance", &HBAO::getAoDistance, &HBAO::setAoDistance);
    pass.def_property("precision", &HBAO::getAoTracePrecision, &HBAO::setAoTracePrecision);
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerClass("HBAO", "Screen-space ambient occlusion", HBAO::create);
    ScriptBindings::registerBinding(regHBAO);
}

const char* HBAO::kDesc = "Screen-space ambient occlusion. Can be used with and without a normal-map";

namespace {

const std::string kFrameSampleCount = "frameSampleCount";

const std::string kHorizonMapSize = "aoMapSize";
const std::string kKernelSize = "kernelSize";
const std::string kNoiseSize = "noiseSize";
const std::string kDistribution = "distribution";
const std::string kRadius = "radius";
const std::string kBlurKernelWidth = "blurWidth";
const std::string kBlurSigma = "blurSigma";

const std::string kAoOut = "aoOut";
const std::string kDepth = "depthH";
const std::string kHiMaxZ = "hiMaxZ";
const std::string kNormals = "normals";
const std::string kHorizonMap = "aoHorizons";

const std::string kComputeHorizonshaderFile = "RenderPasses/HBAO/HBAO.ComputeHorizons.cs.slang";
const std::string kComputeAOShaderFile = "RenderPasses/HBAO/HBAO.ComputeAO.cs.slang";
const std::string kComputeDownSampleDepthShaderFile = "RenderPasses/HBAO/HBAO.HiZ.cs.slang";
}


HBAO::HBAO(Device::SharedPtr pDevice): RenderPass(pDevice) {

  mpHorizonsSearchPass = ComputePass::create(pDevice, kComputeHorizonshaderFile, "main"); 
  setMaxLodLevel(0);
  setMaxSearchIter(8);


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
    
    for (const auto& [key, value] : dict) {
        if (key == kFrameSampleCount) pHBAO->setFrameSampleCount(value);
        else logWarning("Unknown field '" + key + "' in a ForwardLightingPass dictionary");
    }
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
    
    reflector.addInputOutput(kDepth, "Depth-buffer").bindFlags(ResourceBindFlags::ShaderResource);
    reflector.addInputOutput(kHiMaxZ, "Hierarchical max depth-buffer").bindFlags(ResourceBindFlags::ShaderResource)
      .texture2D(0, 0, 1, 0, 1);
    
    reflector.addInput(kNormals, "World space normals, [0, 1] range").flags(RenderPassReflection::Field::Flags::Optional);

    reflector.addOutput(kHorizonMap, "Horizons Map").bindFlags(ResourceBindFlags::UnorderedAccess).format(ResourceFormat::RGBA8Unorm);//RGBA8Unorm);

    return reflector;
}

void HBAO::compile(RenderContext* pRenderContext, const CompileData& compileData) {
  mFrameDim = compileData.defaultTexDims;
  auto pDevice = pRenderContext->device();

  mpNoiseOffsetGenerator = StratifiedSamplePattern::create(mFrameSampleCount);
  mpBlueNoiseTexture = BlueNoiseTexture::create(pDevice);
}

void HBAO::execute(RenderContext* pRenderContext, const RenderData& renderData) {
  if (!mpScene) return;

  auto pDevice = pRenderContext->device();
  auto pCamera = mpScene->getCamera();

  const auto pHiMaxZTex = renderData[kHiMaxZ]->asTexture();

  setMaxLodLevel(mUseDepthAsMipZero ? pHiMaxZTex->getMipCount() : pHiMaxZTex->getMipCount() - 1);
  setMaxSearchIter(std::max(std::min(std::max(mFrameDim[0], mFrameDim[1]) / 32, 16u), 64u));
  
  if(mUseDepthAsMipZero) 
    mpHorizonsSearchPass->addDefine("USE_DEPTH_AS_MIP_ZERO", "");

  // Generate horizons buffer
  float2 f = mpNoiseOffsetGenerator->next();
  uint2 noiseOffset = {64 * (f[0] + 0.5f), 64 * (f[1] + 0.5f)};

  auto cb = mpHorizonsSearchPass["PerFrameCB"];
  cb["gFrameDim"] = mFrameDim;
  cb["gNoiseOffset"] = noiseOffset;
  cb["gAoQuality"] = 1.0f - mAoTracePrecision; // 0 means highest possible quality
  cb["gAoDistance"] = mAoDistance;
  cb["gRotationOffset"] = 0.0f;

  //auto pcb = mpHorizonsSearchPass["PerFrameCB"];
  pCamera->setShaderData(cb["gCamera"]);

  mpHorizonsSearchPass["gPointSampler"] = mpPointSampler;
  mpHorizonsSearchPass["gDepthSampler"] = mpDepthSampler;
  mpHorizonsSearchPass["gNoiseSampler"] = mpNoiseSampler;

  mpHorizonsSearchPass["gNoiseTex"] = mpBlueNoiseTexture;
  mpHorizonsSearchPass["gDepthTex"] = renderData[kDepth]->asTexture();
  mpHorizonsSearchPass["gHiMaxZTex"] = pHiMaxZTex;
  mpHorizonsSearchPass["gOutput"] = renderData[kHorizonMap]->asTexture();

  //auto t1 = std::chrono::high_resolution_clock::now();

  mpHorizonsSearchPass->execute(pRenderContext, mFrameDim.x, mFrameDim.y);
  
  //auto t2 = std::chrono::high_resolution_clock::now();
  //auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
  //std::cout << "1 run in " << ms_int.count() << " ms\n";

}

void HBAO::setFrameSampleCount(uint32_t samples) {
  mFrameSampleCount = samples;
  mDirty = true; 
}

void HBAO::setAoDistance(float distance) {
  if (mAoDistance == distance) return;
  mAoDistance = distance; 
  mDirty = true; 
}

void HBAO::setAoTracePrecision(float precision) {
  if (mAoTracePrecision == precision) return;
  mAoTracePrecision = precision; 
  mDirty = true; 
}

void HBAO::setMaxLodLevel(uint8_t maxHiZLodLevel) {
  if (mMaxHiZLod == maxHiZLodLevel) return;

  mMaxHiZLod = maxHiZLodLevel;
  mpHorizonsSearchPass->addDefine("MAX_HIZ_LOD", std::to_string(mMaxHiZLod).c_str());
}

void HBAO::setMaxSearchIter(uint8_t maxSearchIter) {
  if (mMaxSearchIter == maxSearchIter) return;

  mMaxSearchIter = maxSearchIter;
  mpHorizonsSearchPass->addDefine("MAX_SEARCH_ITER", std::to_string(mMaxSearchIter).c_str());
}