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
#ifndef SRC_FALCOR_UTILS_CRYPTOMATTE_CRYPTOMATTESYSTEM_H_ 
#define SRC_FALCOR_UTILS_CRYPTOMATTE_CRYPTOMATTESYSTEM_H_

#include <unordered_map>

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Device.h"

namespace Falcor {

class Scene;
class MaterialSystem;

/** This class represents a cryptomatte system.
	It holds a collection of material and instance hashes.
*/

class dlldecl CryptomatteSystem {
	public:
		using MaterialID = uint32_t;
		using InstanceID = uint32_t;
		using SharedPtr = std::shared_ptr<CryptomatteSystem>;

		/** Create a cryptomatte system.
			\return New object, or throws an exception if creation failed.
		*/
		static SharedPtr create(Device::SharedPtr pDevice);

		/** Get default shader defines.
			This is the minimal set of defines needed for a program to compile that imports the material system module.
			Note that the actual defines need to be set at runtime, call getDefines() to query them.
		*/
		static Shader::DefineList getDefaultDefines();

		/** Finalize material system before use.
			This function will be removed when unbounded descriptor arrays are supported (see #1321).
		*/
		void finalize();

		/** Update material system. This prepares all resources for rendering.
		*/
		Material::UpdateFlags update(bool forceUpdate);

		/** Get shader defines.
			These need to be set before binding the material system parameter block.
			\return List of shader defines.
		*/
		Shader::DefineList getDefines() const;

		/** Get the parameter block with all material resources.
			The update() function must have been called before calling this function.
		*/
		inline const ParameterBlock::SharedPtr& getParameterBlock() const { return mpMaterialsBlock; }

		/** Add and calculate material hash.
			If an identical material id exists, the material hash is not added.
			\param[in] name The material name.
			\param[in] materialID The material id used by MaterialSystem.
		*/
		void addMaterial(const std::string& name, uint32_t materialID);

		/** Add and calculate hashes for MaterialSystem materials.
			If an identical material id exists, the material hash is not added.
			\param[in] pMaterials The material system.
		*/
		void addMaterials(const MaterialSystem* pMaterials);

		/** Add and calculate instance hash.
			If an identical instance id exists, the instance hash is not added.
			\param[in] name The instance name.
			\param[in] instanceID The instance id used by Scene.
		*/
		void addInstance(const std::string& name, uint32_t instanceID);

		/** Add and calculate custom string attribute instance hash.
			If an identical instance id exists, the instance hash is not added.
			\param[in] name The instance name.
			\param[in] instanceID The instance id used by Scene.
		*/
		void addCustattr(const std::string& name, uint32_t instanceID);

	private:
		CryptomatteSystem(Device::SharedPtr pDevice);

		void createParameterBlock();

		Device::SharedPtr mpDevice = nullptr;

		std::vector<uint32_t> mMaterialHashes;  ///< Map of all material hashes.
		std::vector<uint32_t> mInstanceHashes;  ///< Map of all instance hashes.
		std::vector<uint32_t> mCustattrHashes;  ///< Map of all instance custom string attrubute hashes.
		
		bool mMaterialsChanged = false;                             ///< Flag indicating if materials were added/removed since last update. Per-material updates are tracked by each material's update flags.
		Material::UpdateFlags mMaterialUpdates = Material::UpdateFlags::None; ///< Material updates across all materials since last update.

		// GPU resources
		GpuFence::SharedPtr mpFence;
		ParameterBlock::SharedPtr mpMaterialsBlock;                 ///< Parameter block for binding all material resources.
		Buffer::SharedPtr mpMaterialHashesBuffer;                   ///< GPU buffer holding all material hashes data.
		Buffer::SharedPtr mpInstanceHashesBuffer;                   ///< GPU buffer holding all instance hashes data.
		
		friend class MaterialSystem;
		friend class Scene;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_CRYPTOMATTE_CRYPTOMATTESYSTEM_H_