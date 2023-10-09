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
#include "Utils/HostDeviceShared.slangh"

#ifdef HOST_CODE
#include "Scene/Material/TextureHandle.slang"
#else
__exported import Scene.Material.TextureHandle;
__exported import Scene.Material.MaterialData;
#endif

BEGIN_NAMESPACE_FALCOR

/** This is a host/device structure that describes the parameters for a basic material.

    The layout ensures vector types are stored at 8B/16B aligned memory addresses.
    All color fields are in the range [0,1].

    If changing fields, do not forget to update the comparison operator on the host side.
*/
struct BasicMaterialData
{
    // MaterialHeader (8B) is stored just before this struct in memory.
    uint        flags                   = 0;                ///< Material flags and packed sampler IDs. See accessors below.
    float16_t   emissiveFactor          = float16_t(1.f);   ///< Multiplication factor for the emissive color to control light intensity. Range [0,inf).
    float16_t   bumpNormalFactor        = float16_t(1.f);

    float16_t3  baseColor               = float16_t3(1.f);  ///< Material base color (RGB) and opacity (A).
    float16_t   reflectivity            = float16_t(1.f);   ///< Material reflectivity.

    float16_t3  emissive                = float16_t3(0.f);  ///< Emissive color (RGB).
    float16_t   specularTransmission    = float16_t(0.f);   ///< Specular transmission. Range [0,1].
    float16_t3  transmission            = float16_t3(1.f);  ///< Transmission color.
    float16_t   diffuseTransmission     = float16_t(0.f);   ///< Diffuse transmission. Range [0,1].

    float16_t3  volumeScattering        = float16_t3(0.f);  ///< Volume scattering coefficient. Range [0,inf).
    float16_t   IoR                     = float16_t(1.5f);  ///< Index of refraction. Range [1,inf).
    float16_t3  volumeAbsorption        = float16_t3(0.f);  ///< Volume absorption coefficient. Range [0,inf).
    float16_t   volumeAnisotropy        = float16_t(0.f);   ///< Volume phase function anisotropy (g). Range [-1,1].

    float16_t   roughness               = float16_t(0.f);
    float16_t   metallic                = float16_t(0.f);

    float       displacementScale       = 0.f;              ///< Displacement scale value.
    float       displacementOffset      = 0.f;              ///< Displacement offset value.

    // Texture handles (4B each).
    TextureHandle texBaseColor;
    TextureHandle texMetallic;
    TextureHandle texEmissive;
    TextureHandle texRoughness;
    TextureHandle texNormalMap;
    TextureHandle texTransmission;
    TextureHandle texDisplacementMap;

    static const uint kShadingModelBits = 1;
    static const uint kNormalMapTypeBits = 2;
    static const uint kFlipNormalBits = 1;
    static const uint kNormalMapModeBits = 1;

    static const uint kShadingModelOffset = 0;
    static const uint kNormalMapTypeOffset = kShadingModelOffset + kShadingModelBits;
    static const uint kMinSamplerIDOffset = kNormalMapTypeOffset + kNormalMapTypeBits;
    static const uint kMaxSamplerIDOffset = kMinSamplerIDOffset + MaterialHeader::kSamplerIDBits;

    static const uint kFlipNormalXOffset = kMaxSamplerIDOffset + MaterialHeader::kSamplerIDBits;
    static const uint kFlipNormalYOffset = kFlipNormalXOffset + kFlipNormalBits;

    static const uint kNormalMapModeOffset = kFlipNormalYOffset + kFlipNormalBits;

    static const uint kTotalFlagsBits = kNormalMapModeOffset + kNormalMapModeBits;

    /** Set shading model. This is only used for the standard material.
    */
    SETTER_DECL void setShadingModel(ShadingModel model) { flags = PACK_BITS(kShadingModelBits, kShadingModelOffset, flags, (uint)model); }

    /** Get shading model. This is only used for the standard material.
    */
    ShadingModel getShadingModel() CONST_FUNCTION { return ShadingModel(EXTRACT_BITS(kShadingModelBits, kShadingModelOffset, flags)); }

    /** Set normal map type.
    */
    SETTER_DECL void setNormalMapType(NormalMapType type) { flags = PACK_BITS(kNormalMapTypeBits, kNormalMapTypeOffset, flags, (uint)type); }

    /** Get normal map type.
    */
    NormalMapType getNormalMapType() CONST_FUNCTION { return NormalMapType(EXTRACT_BITS(kNormalMapTypeBits, kNormalMapTypeOffset, flags)); }

    /** Set/get normal X flip flag.
    */
    SETTER_DECL void setNormalMapXFlip(bool flip) { flags = PACK_BITS(kFlipNormalBits, kFlipNormalXOffset, flags, flip ? (uint)1 : (uint)0 ); }
    bool isNormalMapXFlipped() CONST_FUNCTION { return EXTRACT_BITS(kFlipNormalBits, kFlipNormalXOffset, flags); }

    /** Set/get normal Y flip flag.
    */
    SETTER_DECL void setNormalMapYFlip(bool flip) { flags = PACK_BITS(kFlipNormalBits, kFlipNormalYOffset, flags, flip ? (uint)1 : (uint)0 ); }
    bool isNormalMapYFlipped() CONST_FUNCTION { return EXTRACT_BITS(kFlipNormalBits, kFlipNormalYOffset, flags); }

    /** Set/get normal map mode Normal or Bump.
    */
    SETTER_DECL void setNormalMapMode(NormalMapMode mode) { flags = PACK_BITS(kNormalMapModeBits, kNormalMapModeOffset, flags, (uint)mode); }
    NormalMapMode getNormalMapMode() CONST_FUNCTION { return NormalMapMode(EXTRACT_BITS(kNormalMapModeBits, kNormalMapModeOffset, flags)); }

    /** Set displacement map min/max sampler ID.
    */
    SETTER_DECL void setDisplacementMinSamplerID(uint samplerID) { flags = PACK_BITS(MaterialHeader::kSamplerIDBits, kMinSamplerIDOffset, flags, samplerID); }
    SETTER_DECL void setDisplacementMaxSamplerID(uint samplerID) { flags = PACK_BITS(MaterialHeader::kSamplerIDBits, kMaxSamplerIDOffset, flags, samplerID); }

    /** Get displacement map min/max sampler ID.
    */
    uint getDisplacementMinSamplerID() CONST_FUNCTION { return EXTRACT_BITS(MaterialHeader::kSamplerIDBits, kMinSamplerIDOffset, flags); }
    uint getDisplacementMaxSamplerID() CONST_FUNCTION { return EXTRACT_BITS(MaterialHeader::kSamplerIDBits, kMaxSamplerIDOffset, flags); }
};

END_NAMESPACE_FALCOR
