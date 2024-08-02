/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
#include "Utils/Noise/STBNGenerator.slangh"

import Utils.Math.MathHelpers;

/** Struct for accessing noise texture.
*/
struct STBNGenerator {
    Texture3D<float4>   noiseTexture;     ///< Noise texture.
    SamplerState        noiseSampler;     ///< Noise texture sampler.

    uint3               dims;

    /** Returns the dimensions of the env map texture.
    */
    uint3 getDimensions() {
        return dims;
    }

    /** Evaluates noise at a given texel.
    */
    float4 fetch(uint3 coord) {

        return noiseTexture[uint3(coord.x % dims.x, coord.y % dims.y, coord.z % dims.z)].rgba;
    }

    float4 sample(float3 coord) {
        return noiseTexture.Sample(noiseSampler, coord);
    }
};
