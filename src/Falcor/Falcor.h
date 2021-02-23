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
#ifndef FALCOR_SRC_FALCOR_FALCOR_H_
#define FALCOR_SRC_FALCOR_FALCOR_H_

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define _USE_MATH_DEFINES
#include <math.h>

#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Core/Framework.h"

// Core/API
#include "Falcor/Core/API/BlendState.h"
#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Core/API/ComputeContext.h"
#include "Falcor/Core/API/ComputeStateObject.h"
#include "Falcor/Core/API/CopyContext.h"
#include "Falcor/Core/API/DepthStencilState.h"
#include "Falcor/Core/API/DescriptorPool.h"
#include "Falcor/Core/API/DescriptorSet.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/FBO.h"
#include "Falcor/Core/API/FencedPool.h"
#include "Falcor/Core/API/Formats.h"
#include "Falcor/Core/API/GpuFence.h"
#include "Falcor/Core/API/GpuTimer.h"
#include "Falcor/Core/API/GraphicsStateObject.h"
#include "Falcor/Core/API/LowLevelContextData.h"
#include "Falcor/Core/API/QueryHeap.h"
#include "Falcor/Core/API/RasterizerState.h"
#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/Resource.h"
#include "Falcor/Core/API/GpuMemoryHeap.h"
#include "Falcor/Core/API/ResourceViews.h"
#include "Falcor/Core/API/RootSignature.h"
#include "Falcor/Core/API/Sampler.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/VAO.h"
#include "Falcor/Core/API/VertexLayout.h"

// Core/BufferTypes
#include "Falcor/Core/BufferTypes/ParameterBlock.h"

// Core/Platform
#include "Falcor/Core/Platform/OS.h"
#include "Falcor/Core/Platform/ProgressBar.h"

// Core/Program
#include "Falcor/Core/Program/ComputeProgram.h"
#include "Falcor/Core/Program/GraphicsProgram.h"
#include "Falcor/Core/Program/Program.h"
#include "Falcor/Core/Program/ProgramReflection.h"
#include "Falcor/Core/Program/ProgramVars.h"
#include "Falcor/Core/Program/ProgramVersion.h"
#include "Falcor/Core/Program/ShaderLibrary.h"

// Core/State
#include "Falcor/Core/State/ComputeState.h"
#include "Falcor/Core/State/GraphicsState.h"

// RenderGraph
#include "Falcor/RenderGraph/RenderGraph.h"
#include "Falcor/RenderGraph/RenderGraphImportExport.h"
#include "Falcor/RenderGraph/RenderGraphIR.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"
#include "Falcor/RenderGraph/RenderPassReflection.h"
#include "Falcor/RenderGraph/RenderPassStandardFlags.h"
#include "Falcor/RenderGraph/ResourceCache.h"
#include "Falcor/RenderGraph/BasePasses/ComputePass.h"
#include "Falcor/RenderGraph/BasePasses/RasterPass.h"
#include "Falcor/RenderGraph/BasePasses/RasterScenePass.h"
#include "Falcor/RenderGraph/BasePasses/FullScreenPass.h"

// Scene
#include "Falcor/Scene/Scene.h"
#include "Falcor/Scene/Importer.h"
#include "Falcor/Scene/Camera/Camera.h"
#include "Falcor/Scene/Camera/CameraController.h"
#include "Falcor/Scene/Lights/Light.h"
#include "Falcor/Scene/Lights/LightProbe.h"
#include "Falcor/Scene/Material/Material.h"
#include "Falcor/Scene/Animation/Animation.h"
#include "Falcor/Scene/Animation/AnimationController.h"
#include "Falcor/Scene/ParticleSystem/ParticleSystem.h"

// Utils
#include "Falcor/Utils/Math/AABB.h"
#include "Falcor/Utils/BinaryFileStream.h"
#include "Falcor/Utils/Logger.h"
#include "Falcor/Utils/StringUtils.h"
#include "Falcor/Utils/TermColor.h"
#include "Falcor/Utils/Threading.h"
#include "Falcor/Utils/Algorithm/DirectedGraph.h"
#include "Falcor/Utils/Algorithm/DirectedGraphTraversal.h"
#include "Falcor/Utils/Algorithm/ParallelReduction.h"
#include "Falcor/Utils/Image/Bitmap.h"
#include "Falcor/Utils/Math/CubicSpline.h"
#include "Falcor/Utils/Math/FalcorMath.h"
#include "Falcor/Utils/Scripting/Dictionary.h"
#include "Falcor/Utils/Perception/Experiment.h"
#include "Falcor/Utils/Perception/SingleThresholdMeasurement.h"
#include "Falcor/Utils/SampleGenerators/DxSamplePattern.h"
#include "Falcor/Utils/SampleGenerators/HaltonSamplePattern.h"
#include "Falcor/Utils/SampleGenerators/StratifiedSamplePattern.h"
#include "Falcor/Utils/SampleGenerators/CPUSampleGenerator.h"
#include "Falcor/Utils/Scripting/Scripting.h"
#include "Falcor/Utils/Timing/CpuTimer.h"
#include "Falcor/Utils/Timing/Clock.h"
#include "Falcor/Utils/Timing/FrameRate.h"
#include "Falcor/Utils/Timing/Profiler.h"
#include "Falcor/Utils/Timing/TimeReport.h"
#include "Falcor/Utils/Debug/DebugConsole.h"
#include "Falcor/Utils/Debug/PixelDebug.h"

// #ifdef FALCOR_D3D12
// #include "Falcor/Raytracing/RtProgramVars.h"
// #include "Falcor/Raytracing/RtStateObject.h"
// #include "Falcor/Raytracing/RtProgram/RtProgram.h"
// #endif

#define FALCOR_MAJOR_VERSION 4
#define FALCOR_REVISION 1
#define FALCOR_VERSION_STRING "4.1"

#endif  // FALCOR_SRC_FALCOR_FALCOR_H_
