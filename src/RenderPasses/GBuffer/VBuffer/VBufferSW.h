/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
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
#ifndef SRC_FALCOR_RENDERPASSES_GBUFFER_VBUFFER_VBUFFERSW_H_
#define SRC_FALCOR_RENDERPASSES_GBUFFER_VBUFFER_VBUFFERSW_H_

#include "../GBufferBase.h"
#include "Falcor/Core/API/RasterizerState.h"
#include "Utils/Sampling/SampleGenerator.h"
#include "Falcor/Scene/SceneTypes.slang"

#include "VBufferSW.Meshlet.slangh"


using namespace Falcor;


/** Software rasterized V-buffer pass.
*/
class PASS_API VBufferSW : public GBufferBase {
	public:
		using SharedPtr = std::shared_ptr<VBufferSW>;
		using VerticesList = std::vector<uint32_t>;
		using TrianglesList = std::vector<uint32_t>;

		static const Info kInfo;

		static const size_t kMaxGroupThreads;
		static const size_t kMeshletMaxVertices;
		static const size_t kMeshletMaxTriangles;

		static SharedPtr create(RenderContext* pRenderContext, const Dictionary& dict);

		RenderPassReflection reflect(const CompileData& compileData) override;
		void compile(RenderContext* pRenderContext, const CompileData& compileData) override;
		void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
		Dictionary getScriptingDictionary() override;
		void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;

		virtual void setCullMode(RasterizerState::CullMode mode) override;
		void setCullMode(const std::string& mode_str);
		void setPerPixelJitter(bool value);
		void setMaxSubdivLevel(uint level);
		void setMinScreenEdgeLen(float len);

		void setHighpDepth(bool state);
		void enableSubdivisions(bool value);
		void enableDisplacement(bool value);
		void enableDepthOfField(bool value);
		void enableMotionBlur(bool value);

	private:
		void executeCompute(RenderContext* pRenderContext, const RenderData& renderData);

		void preProcessMeshlets();

		void createBuffers();
		void createJitterTexture();
		void createMeshSubdivDataBuffer();
		void createPrograms();
		void createMeshletDrawList();
		void createMicroTrianglesBuffer();

		VBufferSW(Device::SharedPtr pDevice, const Dictionary& dict);
		void parseDictionary(const Dictionary& dict) override;


		Camera::SharedPtr mpCamera;

		// Internal state
		SampleGenerator::SharedPtr mpSampleGenerator;
		CPUSampleGenerator::SharedPtr mpRandomSampleGenerator;
		uint32_t mSampleNumber = 0;

		bool mUsePerPixelJitter = true;
		bool mUseD64 = false;
		bool mUseCompute = true;
		bool mUseDOF = true;                						///< Option for enabling depth-of-field when camera's aperture radius is nonzero.
		bool mUseMotionBlur = false;
		bool mUseSubdivisions = false;
		bool mUseDisplacement = false;

		bool mSubdivDataReady = false;

		float mMinScreenEdgeLen = 4.0f;

		uint mSubgroupSize;
		uint mMaxLOD = 0;
		uint mMaxMicroTrianglesPerThread = 1;

		ComputePass::SharedPtr 	mpComputeSubdivDataBuilderPass;
		ComputePass::SharedPtr 	mpComputeMeshletsBuilderPass;
		ComputePass::SharedPtr 	mpComputeFrustumCullingPass;
		ComputePass::SharedPtr 	mpComputeTesselatorPass;
		ComputePass::SharedPtr 	mpComputeRasterizerPass;
		ComputePass::SharedPtr 	mpComputeReconstructPass;
		ComputePass::SharedPtr 	mpComputeJitterPass;

		// Local buffers
		Buffer::SharedPtr      	mpLocalDepthBuffer;  ///< Local depth-primitiveID buffer
		Buffer::SharedPtr      	mpHiZBuffer;
		Buffer::SharedPtr      	mpMicroTrianglesBuffer;
		std::vector<Buffer::SharedPtr> mMicroTriangleBuffers;

		// Subdiv data buffers
		Buffer::SharedPtr       mpMeshSubdivDataBuffer;
		Buffer::SharedPtr       mpOddVeticesIndexVBuffer;
		Buffer::SharedPtr       mpEvenVerticesIndexBuffer;
		Buffer::SharedPtr       mpEvenVerticesValenceBuffer;

		// Tesselator buffers
		Buffer::SharedPtr    		mpIndicesBuffer;
		Buffer::SharedPtr    		mpPrimIndicesBuffer; 
		Buffer::SharedPtr      	mpPositionsBuffer;
		Buffer::SharedPtr       mpCocsBuffer;

		// Local textures
		Texture::SharedPtr     	mpJitterTexture;
		Sampler::SharedPtr     	mpJitterSampler;

		// Meshlets part
		Buffer::SharedPtr      	mpMeshletDrawListBuffer;
};

#endif   // SRC_FALCOR_RENDERPASSES_GBUFFER_VBUFFER_VBUFFERSW_H_