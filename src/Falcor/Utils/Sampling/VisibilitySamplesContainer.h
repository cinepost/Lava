#ifndef SRC_FALCOR_SCENE_UTILS_VISIBILITYSAMPLESCONTAINER_H_ 
#define SRC_FALCOR_SCENE_UTILS_VISIBILITYSAMPLESCONTAINER_H_

#include <set>
#include <mutex>
#include <algorithm>

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Formats.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Core/API/Texture.h"

#include "Falcor/Scene/Scene.h"
#include "Falcor/Scene/HitInfo.h"

#include "VisibilitySamplesContainer.slangh"


namespace Falcor {

class dlldecl VisibilitySamplesContainer {
	public:
		static const bool kDefaultStoreNormals = false;
		static const bool kDefaultLimitTransparentSamplesCountPP = false;

		using SharedPtr = std::shared_ptr<VisibilitySamplesContainer>;
		using SharedConstPtr = std::shared_ptr<const VisibilitySamplesContainer>;

		/** Create a material system.
			\return New object, or throws an exception if creation failed.
		*/
		static SharedPtr create(Device::SharedPtr pDevice, uint2 resolution, uint maxTransparentSamplesCountPP = 0);

		/** Get default shader defines.
			This is the minimal set of defines needed for a program to compile that imports the material system module.
			Note that the actual defines need to be set at runtime, call getDefines() to query them.
		*/
		static Shader::DefineList getDefaultDefines();


		/** Get shader defines.
			These need to be set before binding the material system parameter block.
			\return List of shader defines.
		*/
		Shader::DefineList getDefines() const;

		/** Get the parameter block with all material resources.
			The update() function must have been called before calling this function.
		*/
		inline const ParameterBlock::SharedPtr& getParameterBlock() const { return mpParameterBlock; }

		void setScene(const Scene::SharedPtr& pScene);

		/** 
		*/
		bool hasTransparentSamples() const;


		void setExternalOpaqueSamplesTexture(const Texture::SharedPtr& pTexture);
		void setExternalOpaqueCombinedNormalsTexture(const Texture::SharedPtr& pTexture);
		void setExternalOpaqueDepthTexture(const Texture::SharedPtr& pTexture);
		void setExternalOpaqueDepthBuffer(const Buffer::SharedPtr& pBuffer);

		/** Optimize samples.
			This function analyzes samples and sorts them to achieve better shading and cache coherency.
		*/
		void sort();

		void beginFrame();

		void beginFrame() const;

		void endFrame();

		void endFrame() const;

		void resize(uint width, uint height);

		void resize(uint width, uint height, uint maxTransparentSamplesCountPP);

		void setMaxTransparencySamplesCountPP(uint maxTransparentSamplesCountPP);

		const uint2& resolution() const { return mResolution; }

		uint opaqueSamplesCount() const;

		uint transparentSamplesCount() const;
		uint transparentListsCount() const;

		uint maxTransparentLayersCount() const;

		void setLimitTransparentSamplesCountPP(bool limit);

		void printStats() const;

		uint reservedTransparentSamplesCount() const { return mTransparentSamplesBufferSize; };

		const Buffer::SharedPtr& getOpaquePassIndirectionArgsBuffer() const { return mpOpaquePassIndirectionArgsBuffer; };

		const Buffer::SharedPtr& getTransparentPassIndirectionArgsBuffer() const { return mpTransparentPassIndirectionArgsBuffer; };

		const uint3& getShadingThreadGroupSize() const { return mShadingThreadGroupSize; }

		void  enableSorting(bool enabled);
		void  enableSortingPP(bool enabled);
		void  storeCombinedNormals(bool enabled);

		bool  hasCombinedNormals() const;

		~VisibilitySamplesContainer();

	private:
		VisibilitySamplesContainer(Device::SharedPtr pDevice, uint2 resolution, uint maxTransparentSamplesCountPP = 1);

		void createParameterBlock();
		void createBuffers();

		void readInfoBufferData() const;

		void sortOpaqueSamples(RenderContext* pRenderContext);
		void sortTransparentSamplesRoots(RenderContext* pRenderContext);
		void sortTransparentSamplesOrder(RenderContext* pRenderContext);
		void sortFinalizeIndirectArgs(RenderContext* pRenderContext);

		// Internal state

		uint2 mResolution;
		uint 	mMaxTransparentSamplesCountPP;
		uint 	mTransparentSamplesBufferSize;
		uint  mResolution1D;
		bool  mSortingEnabled = true;
		bool  mSortingEnabledPP = true;
		bool  mStoreCombinedNormals = kDefaultStoreNormals;

		uint3 mShadingThreadGroupSize;

		float mAlphaThresholdMin;
    float mAlphaThresholdMax;
    bool  mLimitTransparentSamplesCountPP = kDefaultLimitTransparentSamplesCountPP;

		Device::SharedPtr mpDevice = nullptr;
		Scene::SharedPtr  mpScene;

		VisibilitySamplesContainerFlags mFlags;

		// GPU resources
		GpuFence::SharedPtr mpFence;
		mutable ParameterBlock::SharedPtr mpParameterBlock;                 ///< Parameter block for binding all resources.
		mutable ParameterBlock::SharedPtr mpParameterConstBlock;            ///< Parameter block for binding all resources as read only.

		Buffer::SharedPtr  	mpOpaqueSamplesBuffer;
		Buffer::SharedPtr   mpOpaqueCombinedNormalsBuffer;
		Buffer::SharedPtr   mpOpaqueVisibilitySamplesPositionBufferPP;
		Buffer::SharedPtr  	mpRootTransparentSampleOffsetBufferPP;

		Buffer::SharedPtr  	mpTransparentVisibilitySamplesCountBufferPP;
		Buffer::SharedPtr   mpTransparentVisibilitySamplesPositionBufferPP;
		Buffer::SharedPtr   mpTransparentVisibilitySamplesBuffer;
		Buffer::SharedPtr   mpTransparentCombinedNormalsBuffer;
		Buffer::SharedPtr   mpInfoBuffer;

		// Optional external resources
		Texture::SharedPtr  mpOpaqueSamplesExternalTexture;
		Texture::SharedPtr  mpOpaqueCombinedNormalsExternalTexture;
		Texture::SharedPtr  mpOpaqueDepthExternalTexture;
		Buffer::SharedPtr   mpOpaqueDepthExternalBuffer;

		//Scratch data
		Buffer::SharedPtr 	mpOpaquePassIndirectionArgsBuffer;
		Buffer::SharedPtr 	mpTransparentPassIndirectionArgsBuffer;
		
		// 

		mutable std::vector<uint32_t> mpInfoBufferData;

		ComputePass::SharedPtr 	mpOpaqueSortingPass;
		ComputePass::SharedPtr 	mpTransparentRootsSortingPass;
		ComputePass::SharedPtr 	mpTransparentOrderSortingPass;
		ComputePass::SharedPtr 	mpFinalizeSortingPass;

		ResourceFormat      		mHitInfoFormat = HitInfo::kDefaultFormat;
};

enum_class_operators(VisibilitySamplesContainerFlags);

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_UTILS_VISIBILITYSAMPLESCONTAINER_H_