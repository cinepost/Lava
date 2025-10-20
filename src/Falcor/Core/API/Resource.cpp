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
#include "Falcor/Core/API/Resource.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/Buffer.h"

#include <atomic>

namespace Falcor {

Resource::Resource(Device::SharedPtr pDevice, Type type, ResourceBindFlags bindFlags, uint64_t size) 
    : mType(type), 
    mBindFlags(bindFlags), 
    mSize(size), 
    mpDevice(pDevice), 
    mID(newResourceID++) 
{
}

Resource::~Resource() = default;

Device::SharedPtr Resource::getDevice() const {
    return mpDevice;
}

const std::string to_string(Resource::Type type) {
    #define type_2_string(a) case Resource::Type::a: return #a;
    switch (type) {
            type_2_string(Buffer);
            type_2_string(Texture1D);
            type_2_string(Texture2D);
            type_2_string(Texture3D);
            type_2_string(TextureCube);
            type_2_string(Texture2DMultisample);
        default:
            should_not_get_here();
            return "";
    }
#undef type_2_string
}

const std::string to_string(Resource::State state) {
    if (state == Resource::State::Common) {
        return "Common";
    }
    std::string s;
#define state_to_str(f_) if (state == Resource::State::f_) {return #f_; }

    state_to_str(Common);
    state_to_str(VertexBuffer);
    state_to_str(ConstantBuffer);
    state_to_str(IndexBuffer);
    state_to_str(RenderTarget);
    state_to_str(UnorderedAccess);
    state_to_str(DepthStencil);
    state_to_str(ShaderResource);
    state_to_str(StreamOut);
    state_to_str(IndirectArg);
    state_to_str(CopyDest);
    state_to_str(CopySource);
    state_to_str(ResolveDest);
    state_to_str(ResolveSource);
    state_to_str(Present);
    state_to_str(Predication);
    state_to_str(NonPixelShader);
    state_to_str(AccelerationStructure);
#undef state_to_str
    return s;
}

void Resource::invalidateViews() const {
    //logInfo("Invalidating resource views");
    mSrvs.clear();
    mUavs.clear();
    mRtvs.clear();
    mDsvs.clear();
}

Resource::State Resource::getGlobalState() const {
    if (mState.isGlobal == false) {
        LLOG_WRN << "Resource::getGlobalState() - the resource doesn't have a global state. The subresoruces are in a different state, use getSubResourceState() instead";
        return State::Undefined;
    }
    return mState.global;
}

Resource::State Resource::getSubresourceState(uint32_t arraySlice, uint32_t mipLevel) const {
    const Texture* pTexture = dynamic_cast<const Texture*>(this);
    if (pTexture) {
        uint32_t subResource = pTexture->getSubresourceIndex(arraySlice, mipLevel);
        return (mState.isGlobal) ? mState.global : mState.perSubresource[subResource];
    } else {
        LLOG_WRN << "Calling Resource::getSubresourceState() on an object that is not a texture. This call is invalid, use Resource::getGlobalState() instead";
        assert(mState.isGlobal);
        return mState.global;
    }
}

void Resource::setGlobalState(State newState) const {
    mState.isGlobal = true;
    mState.global = newState;
}

void Resource::setSubresourceState(uint32_t arraySlice, uint32_t mipLevel, State newState) const {
    const Texture* pTexture = dynamic_cast<const Texture*>(this);
    if (pTexture == nullptr) {
        LLOG_WRN << "Calling Resource::setSubresourceState() on an object that is not a texture. This is invalid. Ignoring call";
        return;
    }

    // If we are transitioning from a global to local state, initialize the subresource array
    if (mState.isGlobal) {
        std::fill(mState.perSubresource.begin(), mState.perSubresource.end(), mState.global);
    }
    mState.isGlobal = false;
    mState.perSubresource[pTexture->getSubresourceIndex(arraySlice, mipLevel)] = newState;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull-compare"

#pragma GCC push_options
#pragma GCC optimize ("O0")

Texture::SharedPtr Resource::asTexture() {
    assert(this);
    return Falcor::SharedPtr<Texture>(dynamic_cast<Texture*>(this));
}

Falcor::SharedPtr<const Texture> Resource::asTexture() const {
    assert(this);
    return Falcor::SharedPtr<const Texture>(dynamic_cast<const Texture*>(this));
}

Buffer::SharedPtr Resource::asBuffer() {
    assert(this);
    return Falcor::SharedPtr<Buffer>(dynamic_cast<Buffer*>(this));
}

#pragma GCC pop_options
#pragma GCC diagnostic pop

void Resource::breakStrongReferenceToDevice() {
    mpDevice.breakStrongReference();
}

std::atomic<size_t> Resource::newResourceID = 0;

gfx::ResourceState getGFXResourceState(Resource::State state) {
    switch (state) {
        case Resource::State::Undefined:
            return gfx::ResourceState::Undefined;
        case Resource::State::PreInitialized:
            return gfx::ResourceState::PreInitialized;
        case Resource::State::Common:
            return gfx::ResourceState::General;
        case Resource::State::VertexBuffer:
            return gfx::ResourceState::VertexBuffer;
        case Resource::State::ConstantBuffer:
            return gfx::ResourceState::ConstantBuffer;
        case Resource::State::IndexBuffer:
            return gfx::ResourceState::IndexBuffer;
        case Resource::State::RenderTarget:
            return gfx::ResourceState::RenderTarget;
        case Resource::State::UnorderedAccess:
            return gfx::ResourceState::UnorderedAccess;
        case Resource::State::DepthStencil:
            return gfx::ResourceState::DepthWrite;
        case Resource::State::ShaderResource:
            return gfx::ResourceState::ShaderResource;
        case Resource::State::StreamOut:
            return gfx::ResourceState::StreamOutput;
        case Resource::State::IndirectArg:
            return gfx::ResourceState::IndirectArgument;
        case Resource::State::CopyDest:
            return gfx::ResourceState::CopyDestination;
        case Resource::State::CopySource:
            return gfx::ResourceState::CopySource;
        case Resource::State::ResolveDest:
            return gfx::ResourceState::ResolveDestination;
        case Resource::State::ResolveSource:
            return gfx::ResourceState::ResolveSource;
        case Resource::State::Present:
            return gfx::ResourceState::Present;
        case Resource::State::GenericRead:
            return gfx::ResourceState::General;
        case Resource::State::Predication:
            return gfx::ResourceState::General;
        case Resource::State::PixelShader:
            return gfx::ResourceState::PixelShaderResource;
        case Resource::State::NonPixelShader:
            return gfx::ResourceState::NonPixelShaderResource;
        case Resource::State::AccelerationStructure:
            return gfx::ResourceState::AccelerationStructure;
        default:
            FALCOR_UNREACHABLE();
            return gfx::ResourceState::Undefined;
    }
}

void getGFXResourceState(ResourceBindFlags flags, gfx::ResourceState& defaultState, gfx::ResourceStateSet& allowedStates) {
    defaultState = gfx::ResourceState::General;
    allowedStates = gfx::ResourceStateSet(defaultState);

    // setting up the following flags requires Slang gfx resourece states to have integral type
    if (is_set(flags, ResourceBindFlags::UnorderedAccess)) {
        allowedStates.add(gfx::ResourceState::UnorderedAccess);
    }

    if (is_set(flags, ResourceBindFlags::ShaderResource)) {
        allowedStates.add(gfx::ResourceState::ShaderResource);
    }

    if (is_set(flags, ResourceBindFlags::RenderTarget)) {
        allowedStates.add(gfx::ResourceState::RenderTarget);
    }

    if (is_set(flags, ResourceBindFlags::DepthStencil)) {
        allowedStates.add(gfx::ResourceState::DepthWrite);
    }

    if (is_set(flags, ResourceBindFlags::Vertex)) {
        allowedStates.add(gfx::ResourceState::VertexBuffer);
        allowedStates.add(gfx::ResourceState::AccelerationStructureBuildInput);
    }

    if (is_set(flags, ResourceBindFlags::Index)) {
        allowedStates.add(gfx::ResourceState::IndexBuffer);
        allowedStates.add(gfx::ResourceState::AccelerationStructureBuildInput);
    }
    
    if (is_set(flags, ResourceBindFlags::IndirectArg)) {
        allowedStates.add(gfx::ResourceState::IndirectArgument);
    }

    if (is_set(flags, ResourceBindFlags::Constant)) {
        allowedStates.add(gfx::ResourceState::ConstantBuffer);
    }

    if (is_set(flags, ResourceBindFlags::AccelerationStructure)) {
        allowedStates.add(gfx::ResourceState::AccelerationStructure);
        allowedStates.add(gfx::ResourceState::ShaderResource);
        allowedStates.add(gfx::ResourceState::UnorderedAccess);
        defaultState = gfx::ResourceState::AccelerationStructure;
    }

    allowedStates.add(gfx::ResourceState::CopyDestination);
    allowedStates.add(gfx::ResourceState::CopySource);
}

Falcor::Resource::State toFalcorState(gfx::ResourceState state) {
    switch (state) {
        case gfx::ResourceState::Undefined:
            return Falcor::Resource::State::Undefined;
        
        case gfx::ResourceState::PreInitialized:
            return Falcor::Resource::State::PreInitialized;
        
        case gfx::ResourceState::General:
            return Falcor::Resource::State::Common;
        
        case gfx::ResourceState::VertexBuffer:
            return Falcor::Resource::State::VertexBuffer;
        
        case gfx::ResourceState::ConstantBuffer:
            return Falcor::Resource::State::ConstantBuffer;
        
        case gfx::ResourceState::IndexBuffer:
            return Falcor::Resource::State::IndexBuffer;
        
        case gfx::ResourceState::RenderTarget:
            return Falcor::Resource::State::RenderTarget;
        
        case gfx::ResourceState::UnorderedAccess:
            return Falcor::Resource::State::UnorderedAccess;
        
        case gfx::ResourceState::DepthWrite:
            return Falcor::Resource::State::DepthStencil;
        
        case gfx::ResourceState::ShaderResource:
            return Falcor::Resource::State::ShaderResource;
        
        case gfx::ResourceState::StreamOutput:
            return Falcor::Resource::State::StreamOut;
        
        case gfx::ResourceState::IndirectArgument:
            return Falcor::Resource::State::IndirectArg;
        
        case gfx::ResourceState::CopyDestination:
            return Falcor::Resource::State::CopyDest;
        
        case gfx::ResourceState::CopySource:
            return Falcor::Resource::State::CopySource;
        
        case gfx::ResourceState::ResolveDestination:
            return Falcor::Resource::State::ResolveDest;
        
        case gfx::ResourceState::ResolveSource:
            return Falcor::Resource::State::ResolveSource;
        
        case gfx::ResourceState::Present:
            return Falcor::Resource::State::Present;
        
        //case gfx::ResourceState::General:
        //    return Falcor::Resource::State::GenericRead;
        
        //case gfx::ResourceState::ShaderResource:
        //    return Falcor::Resource::State::PixelShader;
        
        //case gfx::Resource::State::NonPixelShader:
        //    return gfx::ResourceState::ShaderResource;
        
        case gfx::ResourceState::AccelerationStructure:
            return Falcor::Resource::State::AccelerationStructure;
        
        default:
            assert(false);
            return Falcor::Resource::State::Undefined;
    }
}

#ifdef SCRIPTING
SCRIPT_BINDING(Resource) {
    pybind11::class_<Resource, Resource::SharedPtr>(m, "Resource");
}
#endif

}  // namespace Falcor
