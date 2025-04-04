/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
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

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Device.h"

#include "Falcor/Core/Program/ProgramVersion.h"
#include "Falcor/Core/API/ParameterBlock.h"

#include "Falcor/Utils/Math/Float16.h"

namespace Falcor {

namespace {

gfx::ShaderOffset getGFXShaderOffset(const UniformShaderVarOffset& offset) {
    gfx::ShaderOffset result;
    result.bindingArrayIndex = 0;
    result.bindingRangeIndex = 0;
    result.uniformOffset = offset.getByteOffset();
    return result;
}

gfx::ShaderOffset getGFXShaderOffset(const ParameterBlock::BindLocation& bindLoc) {
    gfx::ShaderOffset gfxOffset = {};
    gfxOffset.bindingArrayIndex = bindLoc.getResourceArrayIndex();
    gfxOffset.bindingRangeIndex = bindLoc.getResourceRangeIndex();
    gfxOffset.uniformOffset = bindLoc.getUniform().getByteOffset();
    return gfxOffset;
}

bool isSrvType(const ReflectionType* pType) {
    FALCOR_ASSERT(pType);
    auto resourceType = pType->unwrapArray()->asResourceType();
    if (!resourceType || resourceType->getType() == ReflectionResourceType::Type::Sampler || resourceType->getType() == ReflectionResourceType::Type::ConstantBuffer) {
        return false;
    }

    switch (resourceType->getShaderAccess()) {
        case ReflectionResourceType::ShaderAccess::Read:
            return true;
        case ReflectionResourceType::ShaderAccess::ReadWrite:
            return false;
        default:
            FALCOR_UNREACHABLE();
            return false;
    }
}

bool isUavType(const ReflectionType* pType) {
    FALCOR_ASSERT(pType);
    auto resourceType = pType->unwrapArray()->asResourceType();
    if (!resourceType || resourceType->getType() == ReflectionResourceType::Type::Sampler || resourceType->getType() == ReflectionResourceType::Type::ConstantBuffer) {
        return false;
    }

    switch (resourceType->getShaderAccess()) {
        case ReflectionResourceType::ShaderAccess::Read:
            return false;
        case ReflectionResourceType::ShaderAccess::ReadWrite:
            return true;
        default:
            FALCOR_UNREACHABLE();
            return false;
    }
}

/*
bool isCbvType(const ReflectionType::SharedConstPtr& pType) {
    auto resourceType = pType->unwrapArray()->asResourceType();
    if (resourceType->getType() == ReflectionResourceType::Type::ConstantBuffer) {
        assert(resourceType->getShaderAccess() == ReflectionResourceType::ShaderAccess::Read);
        return true;
    }
    return false;
}
*/

bool isSamplerType(const ReflectionType* pType) {
    FALCOR_ASSERT(pType);
    auto resourceType = pType->unwrapArray()->asResourceType();
    return (resourceType && resourceType->getType() == ReflectionResourceType::Type::Sampler);
}

bool isAccelerationStructureType(const ReflectionType* pType) {
    FALCOR_ASSERT(pType);
    auto resourceType = pType->unwrapArray()->asResourceType();
    return (resourceType && resourceType->getType() == ReflectionResourceType::Type::AccelerationStructure);
}

bool isParameterBlockType(const ReflectionType* pType) {
    FALCOR_ASSERT(pType);
    auto resourceType = pType->unwrapArray()->asResourceType();
    // Parameter blocks are currently classified as constant buffers.
    // See getResourceType() in ProgramReflection.cpp
    return (resourceType && resourceType->getType() == ReflectionResourceType::Type::ConstantBuffer);
}

bool isConstantBufferType(const ReflectionType* pType) {
    FALCOR_ASSERT(pType);
    auto resourceType = pType->unwrapArray()->asResourceType();
    return (resourceType && resourceType->getType() == ReflectionResourceType::Type::ConstantBuffer);
}

}  // namespace

ParameterBlock::~ParameterBlock() {}

ParameterBlock::ParameterBlock(Device::SharedPtr pDevice,  const ProgramReflection::SharedConstPtr& pReflector)
    : mpDevice(pDevice)
    , mpProgramVersion(pReflector->getProgramVersion())
    , mpReflector(pReflector->getDefaultParameterBlock()) {
    assert(pDevice);
    assert(pReflector);
    
    FALCOR_GFX_CALL(mpDevice->getApiHandle()->createMutableRootShaderObject(pReflector->getProgramVersion()->getKernels(mpDevice.get(), nullptr)->getGfxProgram(), mpShaderObject.writeRef()));
    createConstantBuffers(getRootVar());
}

ParameterBlock::ParameterBlock(Device::SharedPtr pDevice,
    const ProgramVersion::SharedConstPtr& pProgramVersion,
    const ParameterBlockReflection::SharedConstPtr& pReflection)
    : mpDevice(pDevice)
    , mpProgramVersion(pProgramVersion)
    , mpReflector(pReflection) {
    FALCOR_GFX_CALL(mpDevice->getApiHandle()->createMutableShaderObjectFromTypeLayout(
        pReflection->getElementType()->getSlangTypeLayout(),
        mpShaderObject.writeRef()));
    createConstantBuffers(getRootVar());
}

bool ParameterBlock::setBlob(const void* pSrc, UniformShaderVarOffset offset, size_t size) {
    gfx::ShaderOffset gfxOffset = getGFXShaderOffset(offset);
    return SLANG_SUCCEEDED(mpShaderObject->setData(gfxOffset, pSrc, size));
}

bool ParameterBlock::setBlob(const void* pSrc, size_t offset, size_t size) {
    gfx::ShaderOffset gfxOffset = {};
    gfxOffset.uniformOffset = offset;
    return SLANG_SUCCEEDED(mpShaderObject->setData(gfxOffset, pSrc, size));
}

void ParameterBlock::setBuffer(const std::string& name, const Buffer::SharedPtr& pBuffer) {
    auto var = getRootVar()[name];
    var.setBuffer(pBuffer);
}

void ParameterBlock::setBuffer(const BindLocation& bindLoc, const Buffer::SharedPtr& pResource) {
    gfx::ShaderOffset gfxOffset = getGFXShaderOffset(bindLoc);
    if (isUavType(bindLoc.getType())) {
        auto pUAV = pResource ? pResource->getUAV() : UnorderedAccessView::getNullView(mpDevice, ReflectionResourceType::Dimensions::Buffer);
        mpShaderObject->setResource(gfxOffset, pUAV->getApiHandle());
        mUAVs[gfxOffset] = pUAV;
    } else if (isSrvType(bindLoc.getType())) {
        auto pSRV = pResource ? pResource->getSRV() : ShaderResourceView::getNullView(mpDevice, ReflectionResourceType::Dimensions::Buffer);
        mpShaderObject->setResource(gfxOffset, pSRV->getApiHandle());
        mSRVs[gfxOffset] = pSRV;
    } else {
        FALCOR_THROW("Error trying to bind buffer to a non SRV/UAV variable.");
    }
}

Buffer::SharedPtr ParameterBlock::getBuffer(const std::string& name) const {
    auto var = getRootVar()[name];
    return var.getBuffer();
}

Buffer::SharedPtr ParameterBlock::getBuffer(const BindLocation& bindLoc) const {
    gfx::ShaderOffset gfxOffset = getGFXShaderOffset(bindLoc);
    if (isUavType(bindLoc.getType())) {
        auto iter = mUAVs.find(gfxOffset);
        if (iter == mUAVs.end()) return nullptr;
        return iter->second->getResource()->asBuffer();
    } else if (isSrvType(bindLoc.getType())) {
        auto iter = mSRVs.find(gfxOffset);
        if (iter == mSRVs.end()) return nullptr;
        return iter->second->getResource()->asBuffer();
    } else {
        LLOG_ERR << "Error trying to bind resource to non SRV/UAV variable. Ignoring call.";
        return nullptr;
    }
}

void ParameterBlock::setParameterBlock(const std::string& name, const ParameterBlock::SharedPtr& pBlock) {
    auto var = getRootVar()[name];
    var.setParameterBlock(pBlock);
}

void ParameterBlock::setParameterBlock(const BindLocation& bindLocation, const ParameterBlock::SharedPtr& pBlock) {
    if (isParameterBlockType(bindLocation.getType())) {
        auto gfxOffset = getGFXShaderOffset(bindLocation);
        mParameterBlocks[gfxOffset] = pBlock;
        FALCOR_GFX_CALL(mpShaderObject->setObject(gfxOffset, pBlock ? pBlock->mpShaderObject : nullptr));
    } else {
        FALCOR_THROW("Error trying to bind a parameter block to a non parameter block variable.");
    }
}

ParameterBlock::SharedPtr ParameterBlock::getParameterBlock(const std::string& name) const {
    auto var = getRootVar()[name];
    return var.getParameterBlock();
}

ParameterBlock::SharedPtr ParameterBlock::getParameterBlock(const BindLocation& bindLocation) const {
    auto gfxOffset = getGFXShaderOffset(bindLocation);
    auto iter = mParameterBlocks.find(gfxOffset);
    if (iter == mParameterBlocks.end()) return nullptr;
    return iter->second;
}

template<typename VarType>
bool ParameterBlock::setVariable(UniformShaderVarOffset offset, const VarType& value) {
    auto gfxOffset = getGFXShaderOffset(offset);
    return SLANG_SUCCEEDED(mpShaderObject->setData(gfxOffset, &value, sizeof(VarType)));
}

#define set_constant_by_offset(_t) template FALCOR_API bool ParameterBlock::setVariable(UniformShaderVarOffset offset, const _t& value)
set_constant_by_offset(uint32_t);
set_constant_by_offset(uint2);
set_constant_by_offset(uint3);
set_constant_by_offset(uint4);

set_constant_by_offset(int32_t);
set_constant_by_offset(int2);
set_constant_by_offset(int3);
set_constant_by_offset(int4);

set_constant_by_offset(float);
set_constant_by_offset(float2);
set_constant_by_offset(float3);
set_constant_by_offset(float4);

set_constant_by_offset(float16_t);
set_constant_by_offset(float16_t2);
set_constant_by_offset(float16_t3);
set_constant_by_offset(float16_t4);

set_constant_by_offset(glm::mat2);
set_constant_by_offset(glm::mat2x3);
set_constant_by_offset(glm::mat2x4);

set_constant_by_offset(glm::mat3);
set_constant_by_offset(glm::mat3x2);
set_constant_by_offset(glm::mat3x4);

set_constant_by_offset(glm::mat4);
set_constant_by_offset(glm::mat4x2);
set_constant_by_offset(glm::mat4x3);

set_constant_by_offset(uint64_t);

#undef set_constant_by_offset

void ParameterBlock::setTexture(const std::string& name, const Texture::SharedPtr& pTexture) {
    getRootVar()[name].setTexture(pTexture);
}

void ParameterBlock::setTexture(const BindLocation& bindLocation, const Texture::SharedPtr& pTexture) {
    gfx::ShaderOffset gfxOffset = getGFXShaderOffset(bindLocation);
    if (isUavType(bindLocation.getType())) {
        if (pTexture && !is_set(pTexture->getBindFlags(), ResourceBindFlags::UnorderedAccess)) {
            FALCOR_THROW("Trying to bind texture '{}' created without UnorderedAccess flag as a UAV.", pTexture->getName());
        }
        auto pUAV = pTexture ? pTexture->getUAV() : nullptr;
        mpShaderObject->setResource(gfxOffset, pUAV ? pUAV->getGfxResourceView() : nullptr);
        mUAVs[gfxOffset] = pUAV;
        mResources[gfxOffset] = pTexture;
    } else if (isSrvType(bindLocation.getType())) {
        if (pTexture && !is_set(pTexture->getBindFlags(), ResourceBindFlags::ShaderResource)) {
            FALCOR_THROW("Trying to bind texture '{}' created without ShaderResource flag as an SRV.", pTexture->getName());
        }
        auto pSRV = pTexture ? pTexture->getSRV() : nullptr;
        mpShaderObject->setResource(gfxOffset, pSRV ? pSRV->getGfxResourceView() : nullptr);
        mSRVs[gfxOffset] = pSRV;
        mResources[gfxOffset] = pTexture;
    } else {
        FALCOR_THROW("Error trying to bind texture to a non SRV/UAV variable.");
    }
}

Texture::SharedPtr ParameterBlock::getTexture(const std::string& name) const {
    getRootVar()[name].getTexture();
}

Texture::SharedPtr ParameterBlock::getTexture(const BindLocation& bindLocation) const {
    gfx::ShaderOffset gfxOffset = getGFXShaderOffset(bindLocation);
    if (isUavType(bindLocation.getType())) {
        auto iter = mUAVs.find(gfxOffset);
        if (iter == mUAVs.end()) return nullptr;
        return iter->second->getResource()->asTexture();
    } else if (isSrvType(bindLocation.getType())) {
        auto iter = mSRVs.find(gfxOffset);
        if (iter == mSRVs.end()) return nullptr;
        return iter->second->getResource()->asTexture();
    } else {
        LLOG_ERR << "Error trying to bind resource to non SRV/UAV variable. Ignoring call.";
        return nullptr;
    }
}

void ParameterBlock::setSrv(const BindLocation& bindLocation, const ShaderResourceView::SharedPtr& pSrv) {
    if (isSrvType(bindLocation.getType())) {
        gfx::ShaderOffset gfxOffset = getGFXShaderOffset(bindLocation);
        mpShaderObject->setResource(gfxOffset, pSrv ? pSrv->getGfxResourceView() : nullptr);
        mSRVs[gfxOffset] = pSrv;
        // Note: The resource view does not hold a strong reference to the resource, so we need to keep it alive here.
        mResources[gfxOffset] = Resource::SharedPtr(pSrv ? pSrv->getResource() : nullptr);
    } else {
        FALCOR_THROW("Error trying to bind an SRV to a non SRV variable.");
    }
}

void ParameterBlock::setUav(const BindLocation& bindLocation, const UnorderedAccessView::SharedPtr& pUav) {
    if (isUavType(bindLocation.getType())) {
        gfx::ShaderOffset gfxOffset = getGFXShaderOffset(bindLocation);
        mpShaderObject->setResource(gfxOffset, pUav ? pUav->getGfxResourceView() : nullptr);
        mUAVs[gfxOffset] = pUav;
        // Note: The resource view does not hold a strong reference to the resource, so we need to keep it alive here.
        mResources[gfxOffset] = Resource::SharedPtr(pUav ? pUav->getResource() : nullptr);
    } else {
        FALCOR_THROW("Error trying to bind a UAV to a non UAV variable.");
    }
}

void ParameterBlock::setAccelerationStructure(const BindLocation& bindLocation, const RtAccelerationStructure::SharedPtr& pAccl) {
    if (isAccelerationStructureType(bindLocation.getType())) {
        gfx::ShaderOffset gfxOffset = getGFXShaderOffset(bindLocation);
        mAccelerationStructures[gfxOffset] = pAccl;
        FALCOR_GFX_CALL(mpShaderObject->setResource(gfxOffset, pAccl ? pAccl->getGfxAccelerationStructure() : nullptr));
    } else {
        FALCOR_THROW("Error trying to bind an acceleration structure to a non acceleration structure variable.");
    }
}

ShaderResourceView::SharedPtr ParameterBlock::getSrv(const BindLocation& bindLocation) const {
    gfx::ShaderOffset gfxOffset = getGFXShaderOffset(bindLocation);
    auto iter = mSRVs.find(gfxOffset);
    if (iter == mSRVs.end()) return nullptr;
    return iter->second;
}

UnorderedAccessView::SharedPtr ParameterBlock::getUav(const BindLocation& bindLocation) const {
    gfx::ShaderOffset gfxOffset = getGFXShaderOffset(bindLocation);
    auto iter = mUAVs.find(gfxOffset);
    if (iter == mUAVs.end()) return nullptr;
    return iter->second;
}

RtAccelerationStructure::SharedPtr ParameterBlock::getAccelerationStructure(const BindLocation& bindLocation) const {
    gfx::ShaderOffset gfxOffset = getGFXShaderOffset(bindLocation);
    auto iter = mAccelerationStructures.find(gfxOffset);
    if (iter == mAccelerationStructures.end()) return nullptr;
    return iter->second;
}

void ParameterBlock::setSampler(const std::string& name, const Sampler::SharedPtr& pSampler) {
    getRootVar()[name].setSampler(pSampler);
}

void ParameterBlock::setSampler(const BindLocation& bindLocation, const Sampler::SharedPtr& pSampler) {
    if (isSamplerType(bindLocation.getType())) {
        gfx::ShaderOffset gfxOffset = getGFXShaderOffset(bindLocation);
        const Sampler::SharedPtr& pBoundSampler = pSampler ? pSampler : mpDevice->getDefaultSampler();
        mSamplers[gfxOffset] = pBoundSampler;
        FALCOR_GFX_CALL(mpShaderObject->setSampler(gfxOffset, pBoundSampler->getGfxSamplerState()));
    } else {
        FALCOR_THROW("Error trying to bind a sampler to a non sampler variable.");
    }
}

const Sampler::SharedPtr& ParameterBlock::getSampler(const BindLocation& bindLocation) const {
    static Sampler::SharedPtr pNull = nullptr;

    gfx::ShaderOffset gfxOffset = getGFXShaderOffset(bindLocation);
    auto iter = mSamplers.find(gfxOffset);
    if (iter == mSamplers.end()) {
        return pNull;
    }
    return iter->second;
}

Sampler::SharedPtr ParameterBlock::getSampler(const std::string& name) const {
    auto var = getRootVar()[name];
    return var.getSampler();
}

size_t ParameterBlock::getSize() const {
    return mpShaderObject->getSize();
}

bool ParameterBlock::updateSpecialization() const {
    return true;
}

bool ParameterBlock::prepareDescriptorSets(CopyContext* pCopyContext) {
    // Insert necessary resource barriers for bound resources.
    for (auto& srv : mSRVs) {
        prepareResource(pCopyContext, srv.second->getResource().get(), false);
    }
    for (auto& uav : mUAVs) {
        prepareResource(pCopyContext, uav.second->getResource().get(), true);
    }
    for (auto& subObj : this->mParameterBlocks) {
        subObj.second->prepareDescriptorSets(pCopyContext);
    }
    return true;
}

const ParameterBlock::SharedPtr& ParameterBlock::getParameterBlock(uint32_t resourceRangeIndex, uint32_t arrayIndex) const {
    static ParameterBlock::SharedPtr pNull = nullptr;

    gfx::ShaderOffset gfxOffset = {};
    gfxOffset.bindingRangeIndex = resourceRangeIndex;
    gfxOffset.bindingArrayIndex = arrayIndex;
    auto iter = mParameterBlocks.find(gfxOffset);
    if (iter == mParameterBlocks.end()) {
        return pNull;
    }
    return iter->second;
}

void ParameterBlock::collectSpecializationArgs(SpecializationArgs& ioArgs) const {}

void ParameterBlock::markUniformDataDirty() const {
    throw std::runtime_error("unimplemented");
}

void const* ParameterBlock::getRawData() const {
    return mpShaderObject->getRawData();
}

const Buffer::SharedPtr& ParameterBlock::getUnderlyingConstantBuffer() const {
    throw std::runtime_error("unimplemented");
}

}  // namespace Falcor
