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

#include "ProgramVars.h"
#include "GraphicsProgram.h"
#include "ComputeProgram.h"
#include "Falcor/Core/API/ComputeContext.h"
#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Utils/Debug/debug.h"

#include <slang/slang.h>

namespace Falcor {

    static bool compareRootSets(const DescriptorSet::Layout& a, const DescriptorSet::Layout& b) {
        if (a.getRangeCount() != b.getRangeCount()) return false;
        if (a.getVisibility() != b.getVisibility()) return false;
        for (uint32_t i = 0; i < a.getRangeCount(); i++) {
            const auto& rangeA = a.getRange(i);
            const auto& rangeB = b.getRange(i);
            if (rangeA.baseRegIndex != rangeB.baseRegIndex) return false;
            if (rangeA.descCount != rangeB.descCount) return false;
#ifdef FALCOR_D3D12
            if (rangeA.regSpace != rangeB.regSpace) return false;
#endif
            if (rangeA.type != rangeB.type) return false;
        }
        return true;
    }

    ProgramVars::ProgramVars(
        const ProgramReflection::SharedConstPtr& pReflector)
        : ParameterBlock(pReflector->getProgramVersion(), pReflector->getDefaultParameterBlock())
        , mpReflector(pReflector)
    {
        assert(pReflector);
    }

    void ProgramVars::addSimpleEntryPointGroups() {
        auto& entryPointGroups = mpReflector->getEntryPointGroups();
        auto groupCount = entryPointGroups.size();

        for( size_t gg = 0; gg < groupCount; ++gg ) {
            auto pGroup = entryPointGroups[gg];
            auto pGroupVars = EntryPointGroupVars::create(pGroup, uint32_t(gg));
            mpEntryPointGroupVars.push_back(pGroupVars);
        }
    }

    GraphicsVars::GraphicsVars(const ProgramReflection::SharedConstPtr& pReflector)
        : ProgramVars(pReflector)
    {
        addSimpleEntryPointGroups();
    }

    GraphicsVars::SharedPtr GraphicsVars::create(const ProgramReflection::SharedConstPtr& pReflector) {
        if (pReflector == nullptr) {
            throw std::runtime_error("Can't create a GraphicsVars object without a program reflector");
        }
        return SharedPtr(new GraphicsVars(pReflector));
    }

    GraphicsVars::SharedPtr GraphicsVars::create(const GraphicsProgram* pProg) {
        if (pProg == nullptr) {
            throw std::runtime_error("Can't create a GraphicsVars object without a program");
        }
        return create(pProg->getReflector());
    }

    ComputeVars::SharedPtr ComputeVars::create(const ProgramReflection::SharedConstPtr& pReflector) {
        if (pReflector == nullptr) {
            throw std::runtime_error("Can't create a ComputeVars object without a program reflector");
        }
        return SharedPtr(new ComputeVars(pReflector));
    }

    ComputeVars::SharedPtr ComputeVars::create(const ComputeProgram* pProg) {
        if (pProg == nullptr) {
            throw std::runtime_error("Can't create a ComputeVars object without a program");
        }
        return create(pProg->getReflector());
    }

    ComputeVars::ComputeVars(const ProgramReflection::SharedConstPtr& pReflector)
        : ProgramVars(pReflector)
    {
        addSimpleEntryPointGroups();
    }

    template<bool forGraphics>
    void bindRootSet(DescriptorSet::SharedPtr const& pSet, CopyContext* pContext, RootSignature* pRootSignature, uint32_t rootIndex)
    {
        if (forGraphics)
        {
            pSet->bindForGraphics(pContext, pRootSignature, rootIndex);
        }
        else
        {
            pSet->bindForCompute(pContext, pRootSignature, rootIndex);
        }
    }

    template<bool forGraphics>
    void bindRootDescriptor(CopyContext* pContext, uint32_t rootIndex, const Resource::SharedPtr& pResource, bool isUav)
    {
        #ifdef _WIN32
        auto pBuffer = pResource->asBuffer();
        assert(!pResource || pBuffer); // If a resource is bound, it must be a buffer
        uint64_t gpuAddress = pBuffer ? pBuffer->getGpuAddress() : 0;

        if (forGraphics) {
            if (isUav)
                pContext->getLowLevelData()->getCommandList()->SetGraphicsRootUnorderedAccessView(rootIndex, gpuAddress);
            else
                pContext->getLowLevelData()->getCommandList()->SetGraphicsRootShaderResourceView(rootIndex, gpuAddress);
        } else {
            if (isUav)
                pContext->getLowLevelData()->getCommandList()->SetComputeRootUnorderedAccessView(rootIndex, gpuAddress);
            else
                pContext->getLowLevelData()->getCommandList()->SetComputeRootShaderResourceView(rootIndex, gpuAddress);
        }
        #endif
    }

    template<bool forGraphics>
    void bindRootConstants(CopyContext* pContext, uint32_t rootIndex, ParameterBlock* pParameterBlock, const ParameterBlockReflection* pParameterBlockReflector) {
        #ifdef _WIN32
        uint32_t count = uint32_t(pParameterBlockReflector->getElementType()->getByteSize() / sizeof(uint32_t));
        void const* pSrc = pParameterBlock->getRawData();
        if (forGraphics) {
            pContext->getLowLevelData()->getCommandList()->SetGraphicsRoot32BitConstants(
                rootIndex,
                count,
                pSrc,
                0);
        } else {
            pContext->getLowLevelData()->getCommandList()->SetComputeRoot32BitConstants(
                rootIndex,
                count,
                pSrc,
                0);
        }
        #endif
    }

    template<bool forGraphics>
    bool bindParameterBlockSets(
        ParameterBlock*                 pParameterBlock,
        const ParameterBlockReflection* pParameterBlockReflector,
        CopyContext*                    pContext,
        RootSignature*                  pRootSignature,
        bool                            bindRootSig,
        uint32_t&                       descSetIndex,
        uint32_t&                       rootConstIndex)
    {
        auto defaultConstantBufferInfo = pParameterBlockReflector->getDefaultConstantBufferBindingInfo();
        if( defaultConstantBufferInfo.useRootConstants )
        {
            uint32_t rootIndex = rootConstIndex++;

            bindRootConstants<forGraphics>(pContext, rootIndex, pParameterBlock, pParameterBlockReflector);
        }

        auto descriptorSetCount = pParameterBlockReflector->getDescriptorSetCount();
        for(uint32_t s = 0; s < descriptorSetCount; ++s)
        {
            auto pSet = pParameterBlock->getDescriptorSet(s);

            uint32_t rootIndex = descSetIndex++;

            bindRootSet<forGraphics>(pSet, pContext, pRootSignature, rootIndex);
        }

        // Iterate over parameter blocks to recursively bind their descriptor sets.
        auto parameterBlockRangeCount = pParameterBlockReflector->getParameterBlockSubObjectRangeCount();
        for(uint32_t i = 0; i < parameterBlockRangeCount; ++i)
        {
            auto resourceRangeIndex = pParameterBlockReflector->getParameterBlockSubObjectRangeIndex(i);
            auto& resourceRange = pParameterBlockReflector->getResourceRange(resourceRangeIndex);
            auto& bindingInfo = pParameterBlockReflector->getResourceRangeBindingInfo(resourceRangeIndex);

            auto pSubObjectReflector = bindingInfo.pSubObjectReflector;
            auto objectCount = resourceRange.count;

            for(uint32_t i = 0; i < objectCount; ++i)
            {
                auto pSubBlock = pParameterBlock->getParameterBlock(resourceRangeIndex, i);
                if(!bindParameterBlockSets<forGraphics>(pSubBlock.get(), pSubObjectReflector.get(), pContext, pRootSignature, bindRootSig, descSetIndex, rootConstIndex))
                {
                    return false;
                }
            }
        }

        return true;
    }

    template<bool forGraphics>
    bool bindParameterBlockRootDescs(
        ParameterBlock*                 pParameterBlock,
        const ParameterBlockReflection* pParameterBlockReflector,
        CopyContext*                    pContext,
        RootSignature*                  pRootSignature,
        bool                            bindRootSig,
        uint32_t&                       rootDescIndex)
    {
        auto rootDescriptorRangeCount = pParameterBlockReflector->getRootDescriptorRangeCount();
        //LOG_DBG("for loop rootDescriptorRangeCount");
        for (uint32_t i = 0; i < rootDescriptorRangeCount; ++i)
        {
            auto resourceRangeIndex = pParameterBlockReflector->getRootDescriptorRangeIndex(i);
            auto& resourceRange = pParameterBlockReflector->getResourceRange(resourceRangeIndex);

            assert(resourceRange.count == 1); // Root descriptors cannot be arrays
            auto [pResource, isUav] = pParameterBlock->getRootDescriptor(resourceRangeIndex, 0);

            bindRootDescriptor<forGraphics>(pContext, rootDescIndex++, pResource, isUav);
        }
        //LOG_DBG("for loop rootDescriptorRangeCount done");

        // Iterate over constant buffers and parameter blocks to recursively bind their root descriptors.
        uint32_t resourceRangeCount = pParameterBlockReflector->getResourceRangeCount();
        //LOG_DBG("for loop resourceRangeCount");
        for (uint32_t resourceRangeIndex = 0; resourceRangeIndex < resourceRangeCount; ++resourceRangeIndex) {
            auto& resourceRange = pParameterBlockReflector->getResourceRange(resourceRangeIndex);
            auto& bindingInfo = pParameterBlockReflector->getResourceRangeBindingInfo(resourceRangeIndex);

            if (bindingInfo.flavor != ParameterBlockReflection::ResourceRangeBindingInfo::Flavor::ConstantBuffer &&
                bindingInfo.flavor != ParameterBlockReflection::ResourceRangeBindingInfo::Flavor::ParameterBlock)
                continue;

            auto pSubObjectReflector = bindingInfo.pSubObjectReflector;
            auto objectCount = resourceRange.count;

            //LOG_DBG("subloop resourceRangeCount");
            for (uint32_t i = 0; i < objectCount; ++i) {
                auto pSubBlock = pParameterBlock->getParameterBlock(resourceRangeIndex, i);
                if (!bindParameterBlockRootDescs<forGraphics>(pSubBlock.get(), pSubObjectReflector.get(), pContext, pRootSignature, bindRootSig, rootDescIndex)) {
                    return false;
                }
            }
            //LOG_DBG("subloop resourceRangeCount done");
        }
        //LOG_DBG("for loop resourceRangeCount done");

        //LOG_DBG("return true");

        return true;
    }

    template<bool forGraphics>
    bool bindRootSetsCommon(ParameterBlock* pVars, CopyContext* pContext, bool bindRootSig, RootSignature* pRootSignature) {
        //LOG_DBG("bind root sets common");
        if(!pVars->prepareDescriptorSets(pContext)) return false;

        //LOG_DBG("descSetIndex");
        uint32_t descSetIndex = pRootSignature->getDescriptorSetBaseIndex();
        
        //LOG_DBG("rootDescIndex");
        uint32_t rootDescIndex = pRootSignature->getRootDescriptorBaseIndex();
        
        //LOG_DBG("rootConstIndex");
        uint32_t rootConstIndex = pRootSignature->getRootConstantBaseIndex();

        //LOG_DBG("test bindParameterBlockSets");
        if (!bindParameterBlockSets<forGraphics>(pVars, pVars->getSpecializedReflector().get(), pContext, pRootSignature, bindRootSig, descSetIndex, rootConstIndex)) return false;
        //LOG_DBG("test bindParameterBlockRootDescs");
        if (!bindParameterBlockRootDescs<forGraphics>(pVars, pVars->getSpecializedReflector().get(), pContext, pRootSignature, bindRootSig, rootDescIndex)) return false;

        return true;
    }

    template<bool forGraphics>
    bool applyProgramVarsCommon(ParameterBlock* pVars, CopyContext* pContext, bool bindRootSig, RootSignature* pRootSignature) {
        LOG_DBG("apply program vars common");
        if (bindRootSig) {
            if (forGraphics) {
                pRootSignature->bindForGraphics(pContext);
            } else {
                pRootSignature->bindForCompute(pContext);
            }
        }

        return bindRootSetsCommon<forGraphics>(pVars, pContext, bindRootSig, pRootSignature);
    }

    bool ProgramVars::updateSpecializationImpl() const {
        ParameterBlock::SpecializationArgs specializationArgs;
        collectSpecializationArgs(specializationArgs);
        if( specializationArgs.size() == 0 ) {
            mpSpecializedReflector = ParameterBlock::mpReflector;
            return false;
        }

        // TODO: Want a caching step here, if possible...

        auto pProgramKernels = mpProgramVersion->getKernels(this);
        mpSpecializedReflector = pProgramKernels->getReflector()->getDefaultParameterBlock();
        return false;
    }

    bool ComputeVars::apply(ComputeContext* pContext, bool bindRootSig, RootSignature* pRootSignature) {
        LOG_DBG("apply ComputeVars");
        return applyProgramVarsCommon<false>(this, pContext, bindRootSig, pRootSignature);
    }

    bool GraphicsVars::apply(RenderContext* pContext, bool bindRootSig, RootSignature* pRootSignature) {
        LOG_DBG("apply GraphicsVars");
        return applyProgramVarsCommon<true>(this, pContext, bindRootSig, pRootSignature);
    }

}  // namespace Falcor
