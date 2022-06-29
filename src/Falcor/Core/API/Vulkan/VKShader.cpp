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

#include "lava_utils_lib/logging.h"

namespace Falcor {

    struct ShaderData {
        //ISlangBlob* pBlob;
    
        // new
        Shader::Blob pBlob;
        Slang::ComPtr<slang::IComponentType> pLinkedSlangEntryPoint;

        ISlangBlob* getBlob()
        {
            if (!pBlob)
            {
                Slang::ComPtr<ISlangBlob> pSlangBlob;
                Slang::ComPtr<ISlangBlob> pDiagnostics;

                if (SLANG_FAILED(pLinkedSlangEntryPoint->getEntryPointCode(0, 0, pSlangBlob.writeRef(), pDiagnostics.writeRef())))
                {
                    throw std::runtime_error(std::string("Shader compilation failed. \n") + (const char*)pDiagnostics->getBufferPointer());
                }
                pBlob = Shader::Blob(pSlangBlob.get());
            }
            return pBlob.get();
        }
    };

    Shader::Shader(std::shared_ptr<Device> pDevice, ShaderType type) : mType(type), mpDevice(pDevice) {
       mpPrivateData = std::make_unique<ShaderData>();
    }

    Shader::~Shader() {
        //ShaderData* pData = (ShaderData*)mpPrivateData;
        //safe_delete(pData);
    }

    // typedef ComPtr<ISlangBlob> Blob;

    bool Shader::init(const Blob& shaderBlob, const std::string& entryPointName, CompilerFlags flags, std::string& log) {
        ShaderData* pData = (ShaderData*)mpPrivateData.get();
        pData->pBlob = shaderBlob.get();

        if (pData->pBlob == nullptr) {
            LLOG_ERR << "Shader blob is null !!!";
            return false;
        }

        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = shaderBlob->getBufferSize();
        moduleCreateInfo.pCode = (uint32_t*)shaderBlob->getBufferPointer();

        assert(moduleCreateInfo.codeSize % 4 == 0);

        VkShaderModule shaderModule;
        if (VK_FAILED(vkCreateShaderModule(mpDevice->getApiHandle(), &moduleCreateInfo, nullptr, &shaderModule))) {
            LLOG_ERR << "Could not create shader !!!";
            return false;
        }
        mApiHandle = ApiHandle::create(mpDevice, shaderModule);
        return true;
    }

    bool Shader::init(ISlangBlob *pBlob, const std::string& entryPointName, CompilerFlags flags, std::string& log) {
        ShaderData* pData = (ShaderData*)mpPrivateData.get();
        pData->pBlob = pBlob;

        if (pData->pBlob == nullptr) {
            LLOG_ERR << "Shader blob is null !!!";
            return false;
        }

        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = pBlob->getBufferSize();
        moduleCreateInfo.pCode = (uint32_t*)pBlob->getBufferPointer();

        assert(moduleCreateInfo.codeSize % 4 == 0);

        VkShaderModule shaderModule;
        if (VK_FAILED(vkCreateShaderModule(mpDevice->getApiHandle(), &moduleCreateInfo, nullptr, &shaderModule))) {
            LLOG_ERR << "Could not create shader !!!";
            return false;
        }
        mApiHandle = ApiHandle::create(mpDevice, shaderModule);
        return true;
    }

     bool Shader::init(ComPtr<slang::IComponentType> slangEntryPoint, const std::string& entryPointName, CompilerFlags flags, std::string& log)
    {
        // In GFX, we do not generate actual shader code at program creation.
        // The actual shader code will only be generated and cached when all specialization arguments
        // are known, which is right before a draw/dispatch command is issued, and this is done
        // internally within GFX.
        // The `Shader` implementation here serves as a helper utility for application code that
        // uses raw graphics API to get shader kernel code from an ordinary slang source.
        // Since most users/render-passes do not need to get shader kernel code, we defer
        // the call to slang's `getEntryPointCode` function until it is actually needed.
        // to avoid redundant shader compiler invocation.
        mpPrivateData->pBlob = nullptr;
        mpPrivateData->pLinkedSlangEntryPoint = slangEntryPoint;
        if (slangEntryPoint == nullptr) {
            LLOG_FTL << "slangEntryPoint is NULL";
            return false;
        }

        return init(mpPrivateData->getBlob(), entryPointName, flags, log);
    }


    ISlangBlob* Shader::getISlangBlob() const {
        return mpPrivateData->getBlob();
        //return pData->pBlob;
    }

}  // namespace Falcor