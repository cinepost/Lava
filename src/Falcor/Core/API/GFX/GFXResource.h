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
#ifndef SRC_FALCOR_CORE_API_GFX_GFXRESOURCEHANDLE_H_
#define SRC_FALCOR_CORE_API_GFX_GFXRESOURCEHANDLE_H_

#include "gfx_lib/slang-gfx.h"

#include "Falcor/Core/API/Resource.h"


namespace Falcor {
    gfx::ResourceState getGFXResourceState(Resource::State state);
    void getGFXResourceState(Resource::BindFlags flags, gfx::ResourceState& defaultState, gfx::ResourceStateSet& allowedStates);


inline Falcor::Resource::State toFalcorState(gfx::ResourceState state) {
    switch (state) {
        case ResourceState::Undefined:
            return Falcor::Resource::State::Undefined;
        
        case ResourceState::PreInitialized:
            return Falcor::Resource::State::PreInitialized;
        
        case ResourceState::General:
            return Falcor::Resource::State::Common;
        
        case ResourceState::VertexBuffer:
            return Falcor::Resource::State::VertexBuffer;
        
        case ResourceState::ConstantBuffer:
            return Falcor::Resource::State::ConstantBuffer;
        
        case ResourceState::IndexBuffer:
            return Falcor::Resource::State::IndexBuffer;
        
        case ResourceState::RenderTarget:
            return Falcor::Resource::State::RenderTarget;
        
        case ResourceState::UnorderedAccess:
            return Falcor::Resource::State::UnorderedAccess;
        
        case ResourceState::DepthWrite:
            return Falcor::Resource::State::DepthStencil;
        
        case ResourceState::ShaderResource:
            return Falcor::Resource::State::ShaderResource;
        
        case ResourceState::StreamOutput:
            return Falcor::Resource::State::StreamOut;
        
        case ResourceState::IndirectArgument:
            return Falcor::Resource::State::IndirectArg;
        
        case ResourceState::CopyDestination:
            return Falcor::Resource::State::CopyDest;
        
        case ResourceState::CopySource:
            return Falcor::Resource::State::CopySource;
        
        case ResourceState::ResolveDestination:
            return Falcor::Resource::State::ResolveDest;
        
        case ResourceState::ResolveSource:
            return Falcor::Resource::State::ResolveSource;
        
        case ResourceState::Present:
            return Falcor::Resource::State::Present;
        
        //case ResourceState::General:
        //    return Falcor::Resource::State::GenericRead;
        
        //case ResourceState::ShaderResource:
        //    return Falcor::Resource::State::PixelShader;
        
        //case Resource::State::NonPixelShader:
        //    return gfx::ResourceState::ShaderResource;
        
        case ResourceState::AccelerationStructure:
            return Falcor::Resource::State::AccelerationStructure;
        
        default:
            assert(false);
            return Falcor::Resource::State::Undefined;
    }
}


} // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_GFX_GFXRESOURCEHANDLE_H_