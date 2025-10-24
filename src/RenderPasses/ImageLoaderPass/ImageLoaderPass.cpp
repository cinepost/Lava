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
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"

#include "ImageLoaderPass.h"

const RenderPass::Info ImageLoaderPass::kInfo {
    "ImageLoaderPass",

    "Loads image data from file.\n"
    ""
};

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(ImageLoaderPass::kInfo, ImageLoaderPass::create);
}

namespace {
    const std::string kDst   = "output";
    const std::string kImage = "filename";
    const std::string kMips  = "mips";
    const std::string kSrgb  = "srgb";
    const std::string kArraySlice = "arrayIndex";
    const std::string kMipLevel   = "mipLevel";
}

RenderPassReflection ImageLoaderPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    
    reflector.addOutput(kDst, "Destination color texture");    
    return reflector;
}

ImageLoaderPass::SharedPtr ImageLoaderPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    SharedPtr pPass = SharedPtr(new ImageLoaderPass(pRenderContext->getDevice()));

    for (const auto& [key, value] : dict) {
        if (key == kImage) pPass->mImageName = value.operator fs::path();
        else if (key == kSrgb) pPass->mLoadSRGB = value;
        else if (key == kMips) pPass->mGenerateMips = value;
        else if (key == kArraySlice) pPass->mArraySlice = value;
        else if (key == kMipLevel) pPass->mMipLevel = value;
    }
    
    return pPass;
}

Dictionary ImageLoaderPass::getScriptingDictionary() {
    Dictionary dict;
    dict[kImage] = mImageName;
    dict[kMips] = mGenerateMips;
    dict[kSrgb] = mLoadSRGB;
    dict[kArraySlice] = mArraySlice;
    dict[kMipLevel] = mMipLevel;
    return dict;
}

ImageLoaderPass::ImageLoaderPass(Device::SharedPtr pDevice): RenderPass(pDevice, kInfo), mDirty(true) {

}

void ImageLoaderPass::compile(RenderContext* pContext, const CompileData& compileData) {
    mDirty = true;
}

void ImageLoaderPass::execute(RenderContext* pContext, const RenderData& renderData) {
    const auto& pDstTexture = renderData[kDst]->asTexture();

    if(!mDirty || !pDstTexture) return;

    
    if (mImageName.empty()) {
        pContext->clearRtv(pDstTexture->getRTV().get(), float4(0.f));
    } else {
        auto pSrcTex = Texture::createFromFile(pContext->getDevice(), mImageName, mGenerateMips, mLoadSRGB);
        pContext->blit(pSrcTex->getSRV(0, 1, 0, 1), pDstTexture->getRTV(0, 0, 1));
    }

    mDirty = false;
}
