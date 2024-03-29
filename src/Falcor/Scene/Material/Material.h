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
#ifndef SRC_FALCOR_SCENE_MATERIAL_MATERIAL_H_ 
#define SRC_FALCOR_SCENE_MATERIAL_MATERIAL_H_

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Device.h"

#include "MaterialData.slang"
#include "TextureHandle.slang"
#include "Falcor/Scene/Transform.h"

#include "Falcor/Utils/Math/Float16.h"
#include "Falcor/Utils/Image/TextureAnalyzer.h"

namespace Falcor {

class MaterialSystem;
class BasicMaterial;

/** Abstract base class for materials.
*/
class dlldecl Material : public std::enable_shared_from_this<Material> {
	public:
		// While this is an abstract base class, we still need a holder type (shared_ptr)
		// for pybind11 bindings to work on inherited types.
		using SharedPtr = std::shared_ptr<Material>;

		/** Flags indicating if and what was updated in the material.
		*/
		enum class UpdateFlags : uint32_t {
			None                = 0x0,  ///< Nothing updated.
			DataChanged         = 0x1,  ///< Material data (parameters) changed.
			ResourcesChanged    = 0x2,  ///< Material resources (textures, samplers) changed.
			DisplacementChanged = 0x4,  ///< Displacement mapping parameters changed (only for materials that support displacement).
		};

		/** Texture slots available for use.
			A material does not need to expose/bind all slots.
		*/
		enum class TextureSlot {
			BaseColor,
			Metallic,
			Emissive,
			Roughness,
			Normal,
			Transmission,
			Displacement,

			Count // Must be last
		};

		struct TextureSlotInfo {
			std::string         name;                               ///< Name of texture slot.
			TextureChannelFlags mask = TextureChannelFlags::None;   ///< Mask of enabled texture channels.
			bool                srgb = false;                       ///< True if texture should be loaded in sRGB space.

			bool isEnabled() const { return mask != TextureChannelFlags::None; }
			bool hasChannel(TextureChannelFlags channel) const { return is_set(mask, channel); }
			bool operator==(const TextureSlotInfo& rhs) const { return name == rhs.name && mask == rhs.mask && srgb == rhs.srgb; }
			bool operator!=(const TextureSlotInfo& rhs) const { return !((*this) == rhs); }
		};

		struct TextureSlotData {
			Texture::SharedPtr  pTexture;                           ///< Texture bound to texture slot.

			bool hasData() const { return pTexture != nullptr; }
			bool operator==(const TextureSlotData& rhs) const { return pTexture == rhs.pTexture; }
			bool operator!=(const TextureSlotData& rhs) const { return !((*this) == rhs); }
		};

		struct TextureOptimizationStats {
			std::array<size_t, (size_t)TextureSlot::Count> texturesRemoved = {};
			size_t disabledAlpha = 0;
			size_t constantBaseColor = 0;
			size_t constantNormalMaps = 0;
		};

		virtual ~Material() = default;

		/** Update material. This prepares the material for rendering.
			\param[in] pOwner The material system that this material is used with.
			\return Updates since last call to update().
		*/
		virtual Material::UpdateFlags update(MaterialSystem* pOwner) = 0;

		/** Set the material name.
		*/
		virtual void setName(const std::string& name) { mName = name; }

		/** Get the material name.
		*/
		virtual const std::string& getName() const { return mName; }

		/** Get the material type.
		*/
		virtual MaterialType getType() const { return mHeader.getMaterialType(); }

		/** Returns true if the material is opaque.
		*/
		virtual bool isOpaque() const { return getAlphaMode() == AlphaMode::Opaque; }

		/** Returns true if the material has a displacement map.
		*/
		virtual bool isDisplaced() const { return false; }

		/** Returns true if the material is emissive.
		*/
		virtual bool isEmissive() const { return mHeader.isEmissive(); }

		/** Compares material to another material.
			\param[in] pOther Other material.
			\return true if all materials properties *except* the name are identical.
		*/
		virtual bool isEqual(const Material::SharedPtr& pOther) const = 0;

		/**
		*/
		virtual bool hasUDIMTextures() const { return false; }

		/** Set the double-sided flag. This flag doesn't affect the cull state, just the shading.
		*/
		virtual void setDoubleSided(bool doubleSided);

		/** Returns true if the material is double-sided.
		*/
		virtual bool isDoubleSided() const { return mHeader.isDoubleSided(); }

		/** Set the thin surface flag.
		*/
		virtual void setThinSurface(bool thinSurface);

		/** Returns true if the material is a thin surface.
		*/
		virtual bool isThinSurface() const { return mHeader.isThinSurface(); }

		/** Set the alpha mode.
		*/
		virtual void setAlphaMode(AlphaMode alphaMode);

		/** Get the alpha mode.
		*/
		virtual AlphaMode getAlphaMode() const { return mHeader.getAlphaMode(); }

		/** Set the alpha threshold. The threshold is only used when alpha mode != AlphaMode::Opaque.
		*/
		virtual void setAlphaThreshold(float alphaThreshold);

		/** Get the alpha threshold.
		*/
		virtual float getAlphaThreshold() const { return (float)mHeader.getAlphaThreshold(); }

		/** Set the nested priority used for nested dielectrics.
		*/
		virtual void setNestedPriority(uint32_t priority);

		/** Get the nested priority used for nested dielectrics.
			\return Nested priority, with 0 reserved for the highest possible priority.
		*/
		virtual uint32_t getNestedPriority() const { return mHeader.getNestedPriority(); }

		/** Set the index of refraction.
    */
    virtual void setIndexOfRefraction(float IoR);

    /** Get the index of refraction.
    */
    virtual float getIndexOfRefraction() const { return (float)mHeader.getIoR(); }

		/** Get information about a texture slot.
			\param[in] slot The texture slot.
			\return Info about the slot. If the slot doesn't exist isEnabled() returns false.
		*/
		virtual const TextureSlotInfo& getTextureSlotInfo(const TextureSlot slot) const;

		/** Check if material has a given texture slot.
			\param[in] slot The texture slot.
			\return True if the texture slot exists. Use getTexture() to check if a texture is bound.
		*/
		virtual bool hasTextureSlot(const TextureSlot slot) const { return getTextureSlotInfo(slot).isEnabled(); }

		/** Set one of the available texture slots.
			The call is ignored with a warning if the slot doesn't exist.
			\param[in] slot The texture slot.
			\param[in] pTexture The texture.
			\return True if the texture slot was changed, false otherwise.
		*/
		virtual bool setTexture(const TextureSlot slot, const Texture::SharedPtr& pTexture);

		/** Load one of the available texture slots.
			The call is ignored with a warning if the slot doesn't exist.
			\param[in] The texture slot.
			\param[in] path Path to load texture from.
			\param[in] useSrgb Load texture as sRGB format.
		*/
		virtual void loadTexture(const TextureSlot slot, const fs::path& path, bool useSrgb = true);

		/** Clear one of the available texture slots.
			The call is ignored with a warning if the slot doesn't exist.
			\param[in] The texture slot.
		*/
		virtual void clearTexture(const TextureSlot slot) { setTexture(slot, nullptr); }

		/** Get one of the available texture slots.
			\param[in] The texture slot.
			\return Texture object if bound, or nullptr if unbound or slot doesn't exist.
		*/
		virtual Texture::SharedPtr getTexture(const TextureSlot slot) const;

		/** Optimize texture usage for the given texture slot.
			This function may replace constant textures by uniform material parameters etc.
			\param[in] slot The texture slot.
			\param[in] texInfo Information about the texture bound to this slot.
			\param[out] stats Optimization stats passed back to the caller.
		*/
		virtual void optimizeTexture(const TextureSlot slot, const TextureAnalyzer::Result& texInfo, TextureOptimizationStats& stats) {}

		/** Return the maximum dimensions of the bound textures.
		*/
		virtual uint2 getMaxTextureDimensions() const;

		/** Set the default texture sampler for the material.
		*/
		virtual void setDefaultTextureSampler(const Sampler::SharedPtr& pSampler) {}

		/** Get the default texture sampler for the material.
		*/
		virtual Sampler::SharedPtr getDefaultTextureSampler() const { return nullptr; }

		/** Set the material texture transform.
		*/
		virtual void setTextureTransform(const Transform& texTransform);

		/** Get a reference to the material texture transform.
		*/
		virtual Transform& getTextureTransform() { return mTextureTransform; }

		/** Get the material texture transform.
		*/
		virtual const Transform& getTextureTransform() const { return mTextureTransform; }

		/** Get a reference to the material header data.
		*/
		virtual const MaterialHeader& getHeader() const { return mHeader; }

		/** Get the material data blob for uploading to the GPU.
		*/
		virtual MaterialDataBlob getDataBlob() const = 0;

		// Temporary convenience function to downcast Material to BasicMaterial.
		// This is because a large portion of the interface hasn't been ported to the Material base class yet.
		// TODO: Remove this helper later
		std::shared_ptr<BasicMaterial> toBasicMaterial();

		/** Size of the material instance the material produces.
        Used to set `anyValueSize` on `IMaterialInstance` above the default (128B), for exceptionally large materials.
        Large material instances can have a singificant performance impact.
    */
    virtual size_t getMaterialInstanceByteSize() { return 128; }

		inline Device::SharedPtr device() const { return mpDevice; }

		size_t getTextureCount() const;

		std::vector<Texture::SharedPtr> getTextures() const;

		void getTextures(std::vector<Texture::SharedPtr>& textures, bool append = true) const;

	protected:
		Material(Device::SharedPtr pDevice, const std::string& name, MaterialType type);

		using UpdateCallback = std::function<void(Material::UpdateFlags)>;
		void registerUpdateCallback(const UpdateCallback& updateCallback) { mUpdateCallback = updateCallback; }
		void markUpdates(UpdateFlags updates);
		bool hasTextureSlotData(const TextureSlot slot) const;
		void updateTextureHandle(MaterialSystem* pOwner, const Texture::SharedPtr& pTexture, TextureHandle& handle);
		void updateTextureHandle(MaterialSystem* pOwner, const TextureSlot slot, TextureHandle& handle);
		void updateDefaultTextureSamplerID(MaterialSystem* pOwner, const Sampler::SharedPtr& pSampler);
		bool isBaseEqual(const Material& other) const;

		template<typename T>
		MaterialDataBlob prepareDataBlob(const T& data) const {
			MaterialDataBlob blob = {};
			blob.header = mHeader;
			static_assert(sizeof(mHeader) + sizeof(data) <= sizeof(blob));
			static_assert(offsetof(MaterialDataBlob, payload) == sizeof(MaterialHeader));
			std::memcpy(&blob.payload, &data, sizeof(data));
			return blob;
		}

		Device::SharedPtr mpDevice = nullptr;

		std::string mName;                          ///< Name of the material.
		MaterialHeader mHeader;                     ///< Material header data available in all material types.
		Transform mTextureTransform;                ///< Texture transform. This is currently applied at load time by pre-transforming the texture coordinates.

		std::array<TextureSlotInfo, (size_t)TextureSlot::Count> mTextureSlotInfo;   ///< Information about texture slots.
		std::array<TextureSlotData, (size_t)TextureSlot::Count> mTextureSlotData;   ///< Data bound to texture slots. Only enabled slots can have any data.

		mutable UpdateFlags mUpdates = UpdateFlags::None;
		UpdateCallback mUpdateCallback;             ///< Callback to track updates with the material system this material is used with.

		friend class MaterialSystem;
		friend class SceneCache;
};

inline std::string to_string(MaterialType type) {
	switch (type) {
#define tostr(t_) case MaterialType::t_: return #t_;
		tostr(Standard);
		tostr(Cloth);
		tostr(Hair);
#undef tostr
	default:
		throw std::runtime_error("Invalid material type");
	}
}

inline std::string to_string(Material::TextureSlot slot) {
	switch (slot) {
#define tostr(a) case Material::TextureSlot::a: return #a;
		tostr(BaseColor);
		tostr(Metallic);
		tostr(Emissive);
		tostr(Normal);
		tostr(Roughness);
		tostr(Transmission);
		tostr(Displacement);
#undef tostr
	default:
		throw std::runtime_error("Invalid texture slot");
	}
}

enum_class_operators(Material::UpdateFlags);

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_MATERIAL_MATERIAL_H_