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
#include "Falcor/stdafx.h"
#include "Falcor/Core/API/Shader.h"
#include "Falcor/Core/API/Device.h"

namespace Falcor {

    struct ShaderData {
        ISlangBlob* pBlob;
    };

    Shader::Shader(std::shared_ptr<Device> device, ShaderType type) : mType(type), mpDevice(device) {
        mpPrivateData = new ShaderData;
    }

    Shader::~Shader() {
        ShaderData* pData = (ShaderData*)mpPrivateData;
        safe_delete(pData);
    }

    /*
    bool Shader::init(const Blob& shaderBlob, const std::string& entryPointName, CompilerFlags flags, std::string& log) {
        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = shaderBlob->getBufferSize();
        moduleCreateInfo.pCode = (uint32_t*)shaderBlob->getBufferPointer();

        assert(moduleCreateInfo.codeSize % 4 == 0);

        VkShaderModule shaderModule;
        if (VK_FAILED(vkCreateShaderModule(mpDevice->getApiHandle(), &moduleCreateInfo, nullptr, &shaderModule))) {
            logError("Could not create shader!");
            return false;
        }
        mApiHandle = ApiHandle::create(mpDevice, shaderModule);
        return true;
    }
    */

    bool Shader::init(const Blob& shaderBlob, const std::string& entryPointName, CompilerFlags flags, std::string& log) {
        ShaderData* pData = (ShaderData*)mpPrivateData;
        pData->pBlob = shaderBlob.get();

        if (pData->pBlob == nullptr) {
            logError("Shader blob is null !!!");
            return false;
        }

        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = shaderBlob->getBufferSize();
        moduleCreateInfo.pCode = (uint32_t*)shaderBlob->getBufferPointer();

        assert(moduleCreateInfo.codeSize % 4 == 0);

        VkShaderModule shaderModule;
        if (VK_FAILED(vkCreateShaderModule(mpDevice->getApiHandle(), &moduleCreateInfo, nullptr, &shaderModule))) {
            logError("Could not create shader !!!");
            return false;
        }
        mApiHandle = ApiHandle::create(mpDevice, shaderModule);
        return true;
    }

    ISlangBlob* Shader::getISlangBlob() const {
        const ShaderData* pData = (ShaderData*)mpPrivateData;
        return pData->pBlob;
    }

}  // namespace Falcor

/* D3D

bool Shader::init(const Blob& shaderBlob, const std::string& entryPointName, CompilerFlags flags, std::string& log) {
    // Compile the shader
    ShaderData* pData = (ShaderData*)mpPrivateData;
    pData->pBlob = shaderBlob.get();

    if (pData->pBlob == nullptr) {
        return nullptr;
    }

    mApiHandle = { pData->pBlob->GetBufferPointer(), pData->pBlob->GetBufferSize() };
    return true;
}

ID3DBlobPtr Shader::getD3DBlob() const {
    const ShaderData* pData = (ShaderData*)mpPrivateData;
    return pData->pBlob;
}



*/