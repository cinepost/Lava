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
#pragma once
#include "Utils/HostDeviceShared.slangh"
#include "Utils/Math/MathConstants.slangh"

BEGIN_NAMESPACE_FALCOR

/** Struct representing a material texture handle on the GPU.

    A texture handle can be in different modes:
    - 'Uniform' handle refers to a constant value.
    - 'Texture' handle refers to a traditional texture.

    Procedural texture identified by a unique ID.
*/
struct TextureHandle {
    uint32_t packedData = 0;

    enum class Mode {
        Uniform,
        Texture,
        Procefural,
        Count // Must be last
    };

    static constexpr uint kTextureIDBits = 28;
    static constexpr uint kModeBits = 2;
    static constexpr uint kUdimTextureBits = 1;
    static constexpr uint kSparseTextureBits = 1;

    static constexpr uint kModeOffset = kTextureIDBits;
    static constexpr uint kUdimTextureOffset = kModeOffset + kModeBits;
    static constexpr uint kSparseTextureOffset = kUdimTextureOffset + kUdimTextureBits;

#ifdef HOST_CODE
    TextureHandle() = default;
    explicit TextureHandle(uint32_t packedData_) : packedData(packedData_) {}
#else
    __init(uint32_t packedData) { this.packedData = packedData; }
#endif

    /** Set mode.
    */
    SETTER_DECL void setMode(Mode mode) { packedData = PACK_BITS(kModeBits, kModeOffset, packedData, (uint)mode); }

    /** Get mode.
    */
    Mode getMode() CONST_FUNCTION { return Mode(EXTRACT_BITS(kModeBits, kModeOffset, packedData)); }

    /** Set texture ID. This sets mode to Mode::Texture.
     */
    SETTER_DECL void setTextureID(uint texID) { setMode(Mode::Texture); packedData = PACK_BITS(kTextureIDBits, 0, packedData, texID); }

    /** Get texture ID. This operation is only valid if mode is Mode::Texture.
    */
    uint getTextureID() CONST_FUNCTION { return EXTRACT_BITS(kTextureIDBits, 0, packedData); }

    /** Set whether the texture uses udim or not.
     */
    SETTER_DECL void setUDIMTexture(bool udimTexture) { packedData = PACK_BITS(kUdimTextureBits, kUdimTextureOffset, packedData, udimTexture ? 1 : 0); }

    /** Set whether the texture uses udim or not.
     */
    bool isUDIMTexture() CONST_FUNCTION { return IS_BIT_SET(packedData, kUdimTextureOffset); }

    /**
     */
    SETTER_DECL void setSparseTexture(bool sparseTexture) { packedData = PACK_BITS(kSparseTextureBits, kSparseTextureOffset, packedData, sparseTexture ? 1 : 0); }

    /**
     */
    bool isSparseTexture() CONST_FUNCTION { return IS_BIT_SET(packedData, kSparseTextureOffset); }

    /**
     */
    bool isUniformTexture() CONST_FUNCTION { return getMode() == Mode::Uniform; }


#ifdef HOST_CODE
    bool operator==(const TextureHandle& rhs) const { return packedData == rhs.packedData; }
    bool operator!=(const TextureHandle& rhs) const { return packedData != rhs.packedData; }
#endif
};

/** Struct storing information about a material texture.
*/
struct TextureInfo {
    uint width;
    uint height;
    uint depth;
    uint mipLevels;
};

END_NAMESPACE_FALCOR
