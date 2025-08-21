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
#pragma once
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef WIN32
#include <Windows.h>
#endif 

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Formats.h"

#include "lava_utils_lib/logging.h"

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include "gfx_lib/slang-gfx.h"
#include "gfx_lib/vulkan/vk-module.h"


#if defined (FALCOR_GFX_VK)
#define FALCOR_GFX
#endif

#if FALCOR_ENABLE_NVAPI
#define FALCOR_NVAPI_AVAILABLE 1
#else
#define FALCOR_NVAPI_AVAILABLE 0
#endif

#if FALCOR_GFX_VK
// If we are building Falcor with GFX backend + Vulkan support, define `FALCOR_VK_AVAILABLE` so users
// can know raw Vulkan API and helper classes are available.
#define FALCOR_VK_AVAILABLE 1

#ifndef WIN32
#define HWND void*
// #endif

#define HANDLE void*
#define HRESULT uint32_t
#endif
#endif

#include "gfx_lib/vulkan/vk-module.h"

#define FALCOR_GFX_CALL(a) {auto hr_ = a; if(SLANG_FAILED(hr_)) { reportError(#a); }}

template<typename BlobType>
inline std::string convertBlobToString(BlobType* pBlob)
{
    std::vector<char> infoLog(pBlob->GetBufferSize() + 1);
    memcpy(infoLog.data(), pBlob->GetBufferPointer(), pBlob->GetBufferSize());
    infoLog[pBlob->GetBufferSize()] = 0;
    return std::string(infoLog.data());
}

#define UNSUPPORTED_IN_GFX(msg_) {LLOG_WRN << msg_ << " is not supported in GFX. Ignoring call.";}

//#pragma comment(lib, "gfx.lib")

//TODO: (yhe) Figure out why this is still required.
//#pragma comment(lib, "comsuppw.lib")

namespace Falcor {

/** Flags passed to TraceRay(). These must match the device side.
*/
enum class RayFlags : uint32_t {
    None,
    ForceOpaque = 0x1,
    ForceNonOpaque = 0x2,
    AcceptFirstHitAndEndSearch = 0x4,
    SkipClosestHitShader = 0x8,
    CullBackFacingTriangles = 0x10,
    CullFrontFacingTriangles = 0x20,
    CullOpaque = 0x40,
    CullNonOpaque = 0x80,
    SkipTriangles = 0x100,
    SkipProceduralPrimitives = 0x200,
};
enum_class_operators(RayFlags);

// Maximum raytracing attribute size.
inline constexpr uint32_t getRaytracingMaxAttributeSize() { return 32; }

using ApiObjectHandle = Slang::ComPtr<ISlangUnknown>;

class Device;

using DeviceHandle = Slang::ComPtr<gfx::IDevice>;

using CommandListHandle = Slang::ComPtr<gfx::ICommandBuffer>;
using CommandQueueHandle = Slang::ComPtr<gfx::ICommandQueue>;
using ApiCommandQueueType = gfx::ICommandQueue::QueueType;
using CommandAllocatorHandle = Slang::ComPtr<gfx::ITransientResourceHeap>;
using CommandSignatureHandle = void*;
using FenceHandle = Slang::ComPtr<gfx::IFence>;
using ResourceHandle = Slang::ComPtr<gfx::IResource>;

using RtvHandle = Slang::ComPtr<gfx::IResourceView>;
using DsvHandle = Slang::ComPtr<gfx::IResourceView>;
using SrvHandle = Slang::ComPtr<gfx::IResourceView>;
using CbvHandle = Slang::ComPtr<gfx::IResourceView>;

using SamplerHandle = Slang::ComPtr<gfx::ISamplerState>;
using UavHandle = Slang::ComPtr<gfx::IResourceView>;
using AccelerationStructureHandle = Slang::ComPtr<gfx::IAccelerationStructure>;
using FboHandle = Slang::ComPtr<gfx::IFramebuffer>;
using GpuAddress = uint64_t;
using QueryHeapHandle = Slang::ComPtr<gfx::IQueryPool>;
using SharedResourceApiHandle = HANDLE;
using SharedFenceApiHandle = HANDLE;

using GraphicsStateHandle = Slang::ComPtr<gfx::IPipelineState>;
using ComputeStateHandle = Slang::ComPtr<gfx::IPipelineState>;
using RaytracingStateHandle = Slang::ComPtr<gfx::IPipelineState>;

using VaoHandle = Slang::ComPtr<gfx::IInputLayout>;

using ShaderHandle = Slang::ComPtr<slang::IComponentType>;

using VertexShaderHandle = void*;
using FragmentShaderHandle = void*;
using DomainShaderHandle = void*;
using HullShaderHandle = void*;
using GeometryShaderHandle = void*;
using ComputeShaderHandle = void*;
using ProgramHandle = Slang::ComPtr<gfx::IShaderProgram>;
using DepthStencilStateHandle = void*;
using RasterizerStateHandle = void*;
using BlendStateHandle = void*;

inline Falcor::uint3 alignedDivision(const VkExtent3D& extent, const VkExtent3D& granularity) {
    return {
        extent.width / granularity.width + ((extent.width % granularity.width) ? 1u : 0u),
        extent.height / granularity.height + ((extent.height % granularity.height) ? 1u : 0u),
        extent.depth / granularity.depth + ((extent.depth % granularity.depth) ? 1u : 0u)
    };
}

    /*! @} */
}  // namespace Falcor
