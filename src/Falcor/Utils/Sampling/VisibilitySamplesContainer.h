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
#include "Falcor/Scene/HitInfo.h"


namespace Falcor {

class dlldecl VisibilitySamplesContainer {
	public:
		using SharedPtr = std::shared_ptr<VisibilitySamplesContainer>;
		using SharedConstPtr = std::shared_ptr<const VisibilitySamplesContainer>;

		/** Create a material system.
			\return New object, or throws an exception if creation failed.
		*/
		static SharedPtr create(Device::SharedPtr pDevice, uint2 resolution, uint transparentSamplesCount = 4);

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

		/** 
		*/
		bool hasTransparentSamples() const;

		/** Optimize samples.
			This function analyzes samples and sorts them to achieve better shading and cache coherency.
		*/
		void sort();

		void beginFrame();

		void beginFrame() const;

		void resize(uint width, uint height);

		void resize(uint width, uint height, uint maxTransparentSamplesCount);

		void setMaxTransparencySamplesCount(uint maxTransparentSamplesCount);

		const uint2& resolution() const { return mResolution; }

		uint opaqueSamplesCount() const;

		uint transparentSamplesCount() const;

	private:
		VisibilitySamplesContainer(Device::SharedPtr pDevice, uint2 resolution, uint maxTransparentSamplesCount = 4);

		void createParameterBlock();
		void uploadMaterial(const uint32_t materialID);

		void createBuffers();

		void readInfoBufferData() const;

		uint2 mResolution;
		uint 	mMaxTransparentSamplesCount;

		Device::SharedPtr mpDevice = nullptr;

		uint32_t mFlags;

		// GPU resources
		GpuFence::SharedPtr mpFence;
		ParameterBlock::SharedPtr mpParameterBlock;                 ///< Parameter block for binding all resources.
		ParameterBlock::SharedPtr mpParameterConstBlock;            ///< Parameter block for binding all resources as read only.

		Texture::SharedPtr  mpOpaqueSamplesBuffer;
		Texture::SharedPtr  mpOpaqueVisibilitySamplesPositionBuffer;
		Texture::SharedPtr  mpOpaqueVisibilitySamplesTransparentOffsetBuffer;

		Buffer::SharedPtr   mpInfoBuffer;
		Buffer::SharedPtr   mpTransparentVisibilitySamplesBuffer;

		mutable std::vector<uint32_t> mpInfoBufferData;

		ResourceFormat      mOpaqueSampleDataFormat = HitInfo::kDefaultFormat;
		ResourceFormat      mOpaqueSampleExtraDataFormat = ResourceFormat::RG32Uint;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_UTILS_VISIBILITYSAMPLESCONTAINER_H_