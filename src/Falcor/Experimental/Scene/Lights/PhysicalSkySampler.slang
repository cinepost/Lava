/** Utility functions for physical sky sampling.

    Use the class PhysicalSkySampler on the host to load and prepare the env map.
    The class builds an hierarchical importance map, which is used here
    for importance sampling.
*/

#include "Utils/Math/MathConstants.slangh"
import Utils.Math.MathHelpers;

/** Struct returned from the sampling functions.
*/
struct PhysicalSkySample
{
    float3 dir;         ///< Sampled direction towards the light in world space.
    float pdf;          ///< Probability density function for the sampled direction with respect to solid angle.
    float3 Le;          ///< Emitted radiance.
};

/** Struct for sampling and evaluating an environment map.
*/
struct PhysicalSkySampler {
    /** Evaluates the radiance coming from world space direction 'dir'.
    */
    float3 eval(float3 dir, float lod = 0.f) {
        //return gScene.envMap.eval(dir, lod);
        return float3(0, 0, 0);
    }

    /** Importance sampling of the environment map.
    */
    bool sample(const float2 rnd, out PhysicalSkySample result) {
        float2 p = rnd;     // Random sample in [0,1)^2.
        uint2 pos = 0;      // Top-left texel pos of current 2x2 region.

        // Iterate over mips of 2x2...NxN resolution.
        for (int mip = importanceBaseMip - 1; mip >= 0; mip--) {
            // Scale position to current mip.
            pos *= 2;

            // Load the four texels at the current position.
            float w[4];
            w[0] = importanceMap.Load(int3(pos, mip));
            w[1] = importanceMap.Load(int3(pos + uint2(1, 0), mip));
            w[2] = importanceMap.Load(int3(pos + uint2(0, 1), mip));
            w[3] = importanceMap.Load(int3(pos + uint2(1, 1), mip));

            float q[2];
            q[0] = w[0] + w[2];
            q[1] = w[1] + w[3];

            uint2 off;

            // Horizontal warp.
            float d = q[0] / (q[0] + q[1]);   // TODO: Do we need to guard against div-by-zero. We should ensure we never go down a path that has p=0.

            if (p.x < d) // left
            {
                off.x = 0;
                p.x = p.x / d;
            }
            else // right
            {
                off.x = 1;
                p.x = (p.x - d) / (1.f - d);
            }

            // Vertical warp.
            float e = w[off.x] / q[off.x];

            if (p.y < e) // bottom
            {
                off.y = 0;
                p.y = p.y / e;
            }
            else // top
            {
                off.y = 1;
                p.y = (p.y - e) / (1.f - e);
            }

            pos += off;
        }

        // At this point, we have chosen a texel 'pos' in the range [0,dimension) for each component.
        // The 2D sample point 'p' has been warped along the way, and is in the range [0,1) representing sub-texel location.

        // Compute final sample position and map to direction.
        float2 uv = ((float2)pos + p) * importanceInvDim;     // Final sample in [0,1)^2.
        float3 dir = oct_to_ndir_equal_area_unorm(uv);

        // Compute final pdf.
        // We sample exactly according to the intensity of where the final samples lies in the octahedral map, normalized to its average intensity.
        float avg_w = importanceMap.Load(int3(0, 0, importanceBaseMip)); // 1x1 mip holds integral over importance map. TODO: Replace by constant or rescale in setup so that the integral is 1.0
        float pdf = importanceMap[pos] / avg_w;

        result.dir = dir;
        result.pdf = pdf * M_1_4PI;
        result.Le = float3(0, 0, 0); //gScene.envMap.eval(result.dir);

        return true;
    }

    /** Evaluates the probability density function for a specific direction.
        Note that the sample() function already returns the pdf for the sampled location.
        But, in some cases we need to evaluate the pdf for other directions (e.g. for MIS).

        \param[in] dir World space direction (normalized).
        \return Probability density function evaluated for direction 'dir'.
    */
    float evalPdf(float3 dir) {
        float2 uv = ndir_to_oct_equal_area_unorm(dir);
        float avg_w = importanceMap.Load(int3(0, 0, importanceBaseMip)); // 1x1 mip holds integral over importance map. TODO: Replace by constant or rescale in setup so that the integral is 1.0
        float pdf = importanceMap.SampleLevel(importanceSampler, uv, 0) / avg_w;
        return pdf * (1.f / M_4PI);
    }

    SamplerState        importanceSampler;      ///< Point sampling with clamp to edge.

    Texture2D<float>    importanceMap;          ///< Hierarchical importance map (entire mip chain).
    Texture2D<float3>   sunTransmittanceLUT;
    Texture2D<float3>   multipleScatteringLUT;
    Texture2D<float3>   skyViewLUT;

    float2              importanceInvDim;       ///< 1.0 / dimension.
    uint                importanceBaseMip;      ///< Mip level for 1x1 resolution.
    uint                _pad;
    // TODO: Add scalar value for total integrated intensity, i.e., same as 1x1 mip
};
