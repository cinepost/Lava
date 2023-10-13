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

/** Types of light sources. Used in LightData structure.
*/
enum class LightType
// TODO: Remove the ifdefs and the include when Slang supports enum type specifiers.
#ifdef HOST_CODE
    : uint32_t
#endif
{
    Point,          ///< Point light source, can be a spot light if its opening angle is < 2pi
    Directional,    ///< Directional light source
    Distant,        ///< Distant light that subtends a non-zero solid angle
    Rect,           ///< Quad shaped area light source
    Disc,           ///< Disc shaped area light source
    Sphere,         ///< Spherical area light source
    Env,            ///< Environment light source
    PhysSunSky,     ///< Physical Sun and Sky environment light source
    Count
};


/** Types of light shadows. Used in LightData structure.
*/
enum class LightShadowType
// TODO: Remove the ifdefs and the include when Slang supports enum type specifiers.
#ifdef HOST_CODE
    : uint32_t
#endif
{
    None,
    RayTraced,
    ShadowMap,
    Count
};

enum class LightDataFlags : uint32_t {
    None = 0x0,
    
    SingleSided = 0x1,
    ReversedDirection = 0x2,
    ContribureDirectDiffuse = 0x4,
    ContributeDirectSpecular = 0x8,
    
    ContributeIndirectDiffuse = 0x10,
    ContributeIndirectSpecular = 0x20,
    ContributeDirectSSS = 0x40,
    ContribureIndirectSSS = 0x80,

    OpaqueShadows = 0x100,
    DeltaPosition = 0x200, 
    DeltaDirection = 0x400,
    Area = 0x800,

    Infinite = 0x1000,
    HasTexture = 0x2000,

    VisibleToCamera = 0x4000,
};


/** This is a host/device structure that describes analytic light sources.
*/
struct LightData {
    uint32_t    flags               = uint(LightDataFlags::None);
    float       openingAngle        = float(M_PI);          ///< For point (spot) light: Opening half-angle of a spot light cut-off, pi by default (full sphere).
    float       cosOpeningAngle     = -1.f;                 ///< For point (spot) light: cos(openingAngle), -1 by default because openingAngle is pi by default
    float       cosSubtendedAngle   = 0.9999893f;           ///< For distant light; cosine of the half-angle subtended by the light. Default corresponds to the sun as viewed from earth

    float3      posW                = float3(0, 0, 0);      ///< World-space position of the center of a light source
    uint32_t    flags_ex            = 0x0;                  ///< Extra flags for different types, ids, etc.

    float3      dirW                = float3(0, -1, 0);     ///< World-space orientation of the light source (normalized).
    float       penumbraAngle       = 0.f;                  ///< For point (spot) light: Opening half-angle of penumbra region in radians, usually does not exceed openingAngle. 0.f by default
    
    float16_t3  directDiffuseIntensity    = float16_t3(1, 1, 1);  ///< Emitted diffuse radiance of a light source
    float16_t3  directSpecularIntensity   = float16_t3(1, 1, 1);  ///< Emitted specular radiance of a light source
    float       surfaceArea         = 1.f;                  ///< Surface area of a light shape
    
    float16_t3  shadowColor         = float16_t3(0, 0, 0);
    float16_t3  indirectDiffuseIntensity   = float16_t3(1.0, 1.0, 1.0); ///< Indirect diffuse light intensity
    uint32_t    _pad_0;

    // Extra parameters for analytic area lights
    float3   tangent                = float3(1, 0, 0);      ///< Tangent vector of the light shape
    float    radius                 = .0f;                  ///< Analytic light radius. Used mostly for soft shadowing

    float3   bitangent              = float3(0, 1 ,0);      ///< Bitangent vector of the light shape
    uint32_t extraDataByteOffest;

    float16_t3  indirectSpecularIntensity = float16_t3(1.0, 1.0, 1.0); ///< Indirect specular light intensity
    float16_t3  pad_1;
    uint32_t    pad_2;

    float4x4 transMat               = {};                   ///< Transformation matrix of the light shape, from local to world space.
    float4x4 transMatIT             = {};                   ///< Inverse-transpose of transformation matrix of the light shape
    float4x4 transMatInv            = {};                   ///< Inverse of transformation matrix of the light shape


    //
    static const uint kLightTypeBits = uint(LightType::Count);
    static const uint kLightTypeOffset = 0;

    static const uint kLightShadowTypeBits = uint(LightShadowType::Count);
    static const uint kLightShadowTypeOffset = kLightTypeBits;

    static const uint kLightSamplerIdBits = 4;              ///< 16 distinct samplers per light type. More than enough...
    static const uint kLightSamplerIdOffset = kLightShadowTypeBits + kLightShadowTypeOffset;

    /** Set light type.
    */
    SETTER_DECL void setLightType(LightType type) { flags_ex = PACK_BITS(kLightTypeBits, kLightTypeOffset, flags_ex, (uint)type); }

    /** Get light type.
    */
    LightType getLightType() CONST_FUNCTION { return LightType(EXTRACT_BITS(kLightTypeBits, kLightTypeOffset, flags_ex)); }

    /** Set shadow type.
    */
    SETTER_DECL void setShadowType(LightShadowType type) { flags_ex = PACK_BITS(kLightShadowTypeBits, kLightShadowTypeOffset, flags_ex, (uint)type); }

    /** Get shadow type.
    */
    LightShadowType getShadowType() CONST_FUNCTION { return LightShadowType(EXTRACT_BITS(kLightShadowTypeBits, kLightShadowTypeOffset, flags_ex)); }

    /** Set shadow type.
    */
    SETTER_DECL void setLightSamplerID(uint id) { flags_ex = PACK_BITS(kLightSamplerIdBits, kLightSamplerIdOffset, flags_ex, id); }

    /** Get shadow type.
    */
    uint getLightSamplerID() CONST_FUNCTION { return uint(EXTRACT_BITS(kLightSamplerIdBits, kLightSamplerIdOffset, flags_ex)); }


    bool isSingleSided() CONST_FUNCTION  {
        return (flags & (uint)LightDataFlags::SingleSided) != 0;
    }

    bool isReversed() CONST_FUNCTION {
        return (flags & (uint)LightDataFlags::ReversedDirection) != 0;
    }

    bool isOpaque() CONST_FUNCTION {
        return (flags & (uint)LightDataFlags::OpaqueShadows) != 0;
    }

    bool isAreaLight() CONST_FUNCTION {
        return (flags & (uint)LightDataFlags::Area) != 0;
    }

    bool isInfiniteLight() CONST_FUNCTION {
        return (flags & (uint)LightDataFlags::Infinite) != 0;
    }

    bool isDeltaLight() CONST_FUNCTION {
        return (flags & (uint)LightDataFlags::DeltaPosition) ||
               (flags & (uint)LightDataFlags::DeltaDirection); 
    }

    bool contributesDirectIllumination() CONST_FUNCTION {
        return (flags & (uint)LightDataFlags::ContribureDirectDiffuse) ||
               (flags & (uint)LightDataFlags::ContributeDirectSpecular); 
    }

    bool contributesDirectDiffuseIllumination() CONST_FUNCTION {
        return flags & (uint)LightDataFlags::ContribureDirectDiffuse; 
    }

    bool contributesDirectSpecularIllumination() CONST_FUNCTION {
        return flags & (uint)LightDataFlags::ContributeDirectSpecular; 
    }

    bool contributesIndirectIllumination() CONST_FUNCTION {
        return (flags & (uint)LightDataFlags::ContributeIndirectDiffuse) ||
               (flags & (uint)LightDataFlags::ContributeIndirectSpecular); 
    }

    bool contributesIndirectDiffuseIllumination() CONST_FUNCTION {
        return flags & (uint)LightDataFlags::ContributeIndirectDiffuse; 
    }

    bool contributesIndirectSpecularIllumination() CONST_FUNCTION {
        return flags & (uint)LightDataFlags::ContributeIndirectSpecular; 
    }

    bool hasTexture() CONST_FUNCTION {
        return flags & (uint)LightDataFlags::HasTexture; 
    }

    bool visibleToCamera() CONST_FUNCTION {
        return flags & (uint)LightDataFlags::VisibleToCamera; 
    }

};

END_NAMESPACE_FALCOR