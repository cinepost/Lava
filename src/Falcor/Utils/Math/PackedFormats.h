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
#ifndef SRC_FALCOR_UTILS_MATH_PACKEDFORMATS_H_
#define SRC_FALCOR_UTILS_MATH_PACKEDFORMATS_H_

#include <cmath>
#include <algorithm>
#include "Falcor/Utils/Math/Vector.h"

/** Host-side utility functions for format conversion.

    The functions defined here should match the corresponding GPU-side
    functions, but numerical differences are possible.
*/

namespace Falcor {

/** Helper function to reflect the folds of the lower hemisphere
    over the diagonals in the octahedral map.
*/
inline float2 oct_wrap(float2 v) {
    return { (1.f - abs(v.y)) * (v.x >= 0.f ? 1.f : -1.f), (1.f - abs(v.x)) * (v.y >= 0.f ? 1.f : -1.f) };
}

/** Converts normalized direction to the octahedral map (non-equal area, signed normalized).
    \param[in] n Normalized direction.
    \return Position in octahedral map in [-1,1] for each component.
*/
inline float2 ndir_to_oct_snorm(float3 n) {
    // Project the sphere onto the octahedron (|x|+|y|+|z| = 1) and then onto the xy-plane.
    float2 p = float2(n.x, n.y) * (1.f / (abs(n.x) + abs(n.y) + abs(n.z)));
    p = (n.z < 0.f) ? oct_wrap(p) : p;
    return p;
}

/** Converts point in the octahedral map to normalized direction (non-equal area, signed normalized).
    \param[in] p Position in octahedral map in [-1,1] for each component.
    \return Normalized direction.
*/
inline float3 oct_to_ndir_snorm(float2 p) {
    float3 n = float3(p.x, p.y, 1.f - abs(p.x) - abs(p.y));
    float2 tmp = (n.z < 0.0) ? oct_wrap(float2(n.x, n.y)) : float2(n.x, n.y);
    n.x = tmp.x;
    n.y = tmp.y;
    return normalize(n);
}

/** Encode a normal packed as 2x 16-bit snorms in the octahedral mapping.
*/
inline uint encodeNormal2x16(float3 normal) {
    float2 octNormal = ndir_to_oct_snorm(normal);
    return glm::packSnorm2x16(octNormal);
}

/** Decode a normal packed as 2x 16-bit snorms in the octahedral mapping.
*/
inline float3 decodeNormal2x16(uint packedNormal) {
    float2 octNormal = glm::unpackSnorm2x16(packedNormal);
    return oct_to_ndir_snorm(octNormal);
}

// -------- From FormatConversion.slang

/** Convert float value to 8-bit unorm (unsafe version).
    \param[in] v Float value assumed to be in [0,1].
    \return 8-bit unorm in low bits, high bits all zeros.
*/
inline uint packUnorm8_unsafe(float v) {
    return (uint)std::trunc(v * 255.f + 0.5f);
}

/** Convert float value to 8-bit unorm.
    Values outside [0,1] are clamped and NaN is encoded as zero.
    \param[in] v Float value.
    \return 8-bit unorm in low bits, high bits all zeros.
*/
inline uint packUnorm8(float v) {
    v = std::isnan(v) ? 0.f : std::clamp(v, 0.0f, 1.0f);
    return packUnorm8_unsafe(v);
}

/** Pack three floats into 8-bit unorm values.
    Values outside [0,1] are clamped and NaN is encoded as zero.
    \param[in] v Three float values.
    \return Packed 8-bit unorm values in low bits, high bits all zeros.
*/
inline uint packUnorm3x8(float3 v) {
    return (packUnorm8(v.z) << 16) | (packUnorm8(v.y) << 8) | packUnorm8(v.x);
}

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_MATH_PACKEDFORMATS_H_

