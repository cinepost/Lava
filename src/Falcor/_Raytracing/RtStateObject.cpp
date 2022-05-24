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
#include "stdafx.h"
#include "RtStateObject.h"
//#include "RtStateObjectHelper.h"
#include "Utils/StringUtils.h"
#include "Core/API/Device.h"
#include "Core/API/RootSignature.h"

#include "Falcor/Core/API/Vulkan/FalcorVK.h"
#include "Falcor/Utils/Debug/debug.h"

#include "ShaderTable.h"

namespace Falcor {

bool RtStateObject::Desc::operator==(const RtStateObject::Desc& other) const {
    bool b = true;
    b = b && (mMaxTraceRecursionDepth == other.mMaxTraceRecursionDepth);
    b = b && (mpKernels == other.mpKernels);
    return b;
}

RtStateObject::SharedPtr RtStateObject::create(Device::SharedPtr pDevice, const Desc& desc) {
    LOG_DBG("RtStateObject::create");

    SharedPtr pState = SharedPtr(new RtStateObject(pDevice, desc));

    //RtStateObjectHelper rtsoHelper;
    
    // Pipeline config
    //rtsoHelper.addPipelineConfig(desc.mMaxTraceRecursionDepth);

    // Loop over the programs
    auto pKernels = pState->getKernels();
    for (const auto& pBaseEntryPointGroup : pKernels->getUniqueEntryPointGroups() ) {

        assert(dynamic_cast<RtEntryPointGroupKernels*>(pBaseEntryPointGroup.get()));
        auto pEntryPointGroup = static_cast<RtEntryPointGroupKernels*>(pBaseEntryPointGroup.get());
        switch( pBaseEntryPointGroup->getType() ) {

            case EntryPointGroupKernels::Type::RtHitGroup: 
            {
                    const Shader* pIss = pEntryPointGroup->getShader(ShaderType::Intersection);
                    const Shader* pAhs = pEntryPointGroup->getShader(ShaderType::AnyHit);
                    const Shader* pChs = pEntryPointGroup->getShader(ShaderType::ClosestHit);

                    ISlangBlob* pIssBlob = pIss ? pIss->getISlangBlob() : nullptr;
                    ISlangBlob* pAhsBlob = pAhs ? pAhs->getISlangBlob() : nullptr;
                    ISlangBlob* pChsBlob = pChs ? pChs->getISlangBlob() : nullptr;

                    const std::wstring& exportName = string_2_wstring(pEntryPointGroup->getExportName());
                    const std::wstring& issExport = pIss ? string_2_wstring(pIss->getEntryPoint()) : L"";
                    const std::wstring& ahsExport = pAhs ? string_2_wstring(pAhs->getEntryPoint()) : L"";
                    const std::wstring& chsExport = pChs ? string_2_wstring(pChs->getEntryPoint()) : L"";

                    //rtsoHelper.addHitProgramDesc(pAhsBlob, ahsExport, pChsBlob, chsExport, pIssBlob, issExport, exportName);

                    //if (issExport.size()) {
                    //    rtsoHelper.addLocalRootSignature(&issExport, 1, pEntryPointGroup->getLocalRootSignature()->getApiHandle().GetInterfacePtr());
                    //    rtsoHelper.addShaderConfig(&issExport, 1, pEntryPointGroup->getMaxPayloadSize(), pEntryPointGroup->getMaxAttributesSize());
                    //}

                    //if (ahsExport.size()) {
                    //    rtsoHelper.addLocalRootSignature(&ahsExport, 1, pEntryPointGroup->getLocalRootSignature()->getApiHandle().GetInterfacePtr());
                    //    rtsoHelper.addShaderConfig(&ahsExport, 1, pEntryPointGroup->getMaxPayloadSize(), pEntryPointGroup->getMaxAttributesSize());
                    //}

                    //if (chsExport.size()) {
                    //    rtsoHelper.addLocalRootSignature(&chsExport, 1, pEntryPointGroup->getLocalRootSignature()->getApiHandle().GetInterfacePtr());
                    //    rtsoHelper.addShaderConfig(&chsExport, 1, pEntryPointGroup->getMaxPayloadSize(), pEntryPointGroup->getMaxAttributesSize());
                    //}
                }
                break;

            default:
            {
                const std::wstring& exportName = string_2_wstring(pEntryPointGroup->getExportName());


                const Shader* pShader = pEntryPointGroup->getShaderByIndex(0);
                //rtsoHelper.addProgramDesc(pShader->getISlangBlob(), exportName);

                // Root signature
                //rtsoHelper.addLocalRootSignature(&exportName, 1, pEntryPointGroup->getLocalRootSignature()->getApiHandle().get());
                // Payload size
                //rtsoHelper.addShaderConfig(&exportName, 1, pEntryPointGroup->getMaxPayloadSize(), pEntryPointGroup->getMaxAttributesSize());
            }
            break;
        }
    }

    // Add an empty global root-signature
    RootSignature* pRootSig = desc.mpGlobalRootSignature ? desc.mpGlobalRootSignature.get() : RootSignature::getEmpty(pDevice).get();
    //rtsoHelper.addGlobalRootSignature(pRootSig->getApiHandle());

    // Create the state
/*
    VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding{};
    accelerationStructureLayoutBinding.binding = 0;
    accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    accelerationStructureLayoutBinding.descriptorCount = 1;
    accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding resultImageLayoutBinding{};
    resultImageLayoutBinding.binding = 1;
    resultImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    resultImageLayoutBinding.descriptorCount = 1;
    resultImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding uniformBufferBinding{};
    uniformBufferBinding.binding = 2;
    uniformBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBufferBinding.descriptorCount = 1;
    uniformBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    std::vector<VkDescriptorSetLayoutBinding> bindings({
        accelerationStructureLayoutBinding,
        resultImageLayoutBinding,
        uniformBufferBinding
    });

    VkDescriptorSetLayoutCreateInfo descriptorSetlayoutCI{};
    descriptorSetlayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetlayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
    descriptorSetlayoutCI.pBindings = bindings.data();
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(pDevice->getApiHandle(), &descriptorSetlayoutCI, nullptr, &descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = 1;
    pipelineLayoutCI.pSetLayouts = &descriptorSetLayout;
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->getApiHandle(), &pipelineLayoutCI, nullptr, &pipelineLayout));

    // Setup ray tracing shader groups
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    // Ray generation group
    {
        shaderStages.push_back(loadShader(getShadersPath() + "raytracingbasic/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
        shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
        shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        shaderGroups.push_back(shaderGroup);
    }

    // Miss group
    {
        shaderStages.push_back(loadShader(getShadersPath() + "raytracingbasic/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
        VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
        shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
        shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        shaderGroups.push_back(shaderGroup);
    }

    // Closest hit group
    {
        shaderStages.push_back(loadShader(getShadersPath() + "raytracingbasic/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
        VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
        shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
        shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        shaderGroups.push_back(shaderGroup);
    }

    
    // Create the ray tracing pipeline
    VkPipeline pipeline;

    VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
    rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    rayTracingPipelineCI.pStages = shaderStages.data();
    rayTracingPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups.size());
    rayTracingPipelineCI.pGroups = shaderGroups.data();
    rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
    rayTracingPipelineCI.layout = pipelineLayout;
    VK_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &pipeline));

    mApiHandle = ApiHandle::create(mpDevice, pipeline);
*/
    // .... todo ....

    for( const auto& pBaseEntryPointGroup : pKernels->getUniqueEntryPointGroups() ) {
        assert(dynamic_cast<RtEntryPointGroupKernels*>(pBaseEntryPointGroup.get()));
        auto pEntryPointGroup = static_cast<RtEntryPointGroupKernels*>(pBaseEntryPointGroup.get());
        const std::wstring& exportName = string_2_wstring(pEntryPointGroup->getExportName());

        //void const* pShaderIdentifier = pRtsoProps->GetShaderIdentifier(exportName.c_str());
        //pState->mShaderIdentifiers.push_back(pShaderIdentifier);
    }

    return pState;
}

}  // namespace Falcor
