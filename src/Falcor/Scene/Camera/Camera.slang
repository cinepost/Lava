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
import Scene.Camera.CameraData;
__exported import Utils.Math.Ray;
import Utils.Math.MathHelpers;


CameraXformData lerp(CameraXformData lhs, CameraXformData rhs, float c) {
    const float k = clamp(c, 0.f, 1.f);
    const float ik = 1.f - k;
    CameraXformData result;

    result.viewInvMat = lhs.viewInvMat * k + rhs.viewInvMat * ik;
    result.viewProjMat = lhs.viewProjMat * k + rhs.viewProjMat * ik;
    result.cameraU = lhs.cameraU * k + rhs.cameraU * ik;
    result.cameraV = lhs.cameraV * k + rhs.cameraV * ik;
    result.cameraW = lhs.cameraW * k + rhs.cameraW * ik;

    return result;
}

struct Camera {
    CameraData data;
    CameraXformData xform;                              // Backward compatibility. Also used for prev frame magic. 
    StructuredBuffer<CameraXformData> xformListBuffer;  // Transofrm list. Used for inter frame motion blur rendering. Note that first elemrnt is equal to xform member.
    uint xformListBufferSize;

    // Time independent functions

    float3 getPosW() { return {xform.viewInvMat[3][0], xform.viewInvMat[3][1], xform.viewInvMat[3][2]}; }
    
    float4x4 getViewProjMat() { return xform.viewProjMat; }
    
    float4x4 getViewProjMatNoJitter() { return xform.viewProjMat; }
    
    float4x4 getPrevViewProjMat() { return data.prevViewProjMat; }

    // Time dependent functions

    CameraXformData getXformData(float tj) {
        const float k = clamp(tj, 0.f, 1.f);

        if(xformListBufferSize < 2 || k == 0.f) return xform;

        const float fidx = k * (xformListBufferSize - 1);
        uint idx_l = floor(fidx);
        uint idx_h = ceil(fidx);

        float c = fidx - float(idx_l);

        return lerp(xformListBuffer[idx_l], xformListBuffer[idx_h], c);
    }

    //float3 getPosW(float t) { return {xform.viewInvMat[3][0], xform.viewInvMat[3][1], xform.viewInvMat[3][2]}; }
    
    //float4x4 getViewProjMat(float t) { return getXformData(t).viewProjMat; }
    
    //float4x4 getViewProjMatNoJitter(float t) { return getXformData(t).viewProjMat; }

    float4 getBackgroundColor() { return data.backgroundColor; }

    /** Computes a camera ray for a given pixel assuming a pinhole camera model.
        The camera jitter is taken into account to compute the sample position on the image plane.
        \param[in] pixel Pixel coordinates with origin in top-left.
        \param[in] frameDim Image plane dimensions in pixels.
        \param[in] applyJitter true if jitter should be applied else false.
        \return Returns the camera ray.
    */
    Ray computeRayPinhole(uint2 pixel, uint2 frameDim, bool applyJitter = true) {
        const float2 rnd = applyJitter ? (float2(frameDim) * (float2(-data.jitterX, data.jitterY)) + float2(0.5)) : float2(0.5f, 0.5f);
        return computeRayPinhole(pixel, frameDim, rnd);
    }

    Ray computeRayPinhole(uint2 pixel, uint2 frameDim, float2 rnd) {
        Ray ray;

        // Compute the normalized ray direction assuming a pinhole camera.
        ray.origin = getPosW();
        ray.dir = normalize(computeNonNormalizedRayDirPinhole(pixel, frameDim, rnd));

        float invCos = 1.f / dot(normalize(xform.cameraW), ray.dir);
        ray.tMin = data.nearZ * invCos;
        ray.tMax = data.farZ * invCos;

        return ray;
    }

    /** Computes the primary ray's direction, non-normalized assuming pinhole camera model.
        The camera jitter is taken into account to compute the sample position on the image plane.
        \param[in] pixel Pixel coordinates with origin in top-left.
        \param[in] frameDim Image plane dimensions in pixels.
        \param[in] applyJitter True if jitter should be applied else false.
        \return Returns the non-normalized ray direction
    */
    float3 computeNonNormalizedRayDirPinhole(uint2 pixel, uint2 frameDim, bool applyJitter = true) {
        const float2 rnd = applyJitter ? (float2(frameDim) * (float2(-data.jitterX, data.jitterY)) + float2(0.5)) : float2(0.5f, 0.5f);
        return computeNonNormalizedRayDirPinhole(pixel, frameDim, rnd);
    }

    float3 computeNonNormalizedRayDirPinhole(uint2 pixel, uint2 frameDim, float2 rnd) {
        // Compute sample position in screen space in [0,1] with origin at the top-left corner.
        // Variable rnd [0,1] offsets the sample by +-0.5 pixels from the pixel center.
        float2 p = (float2(pixel) + rnd) / frameDim;
        float2 ndc = float2(2, -2) * p + float2(-1, 1);

        // Compute the non-normalized ray direction assuming a pinhole camera.
        return ndc.x * xform.cameraU + ndc.y * xform.cameraV + xform.cameraW;
    }

    /** Computes a camera ray for a given pixel assuming a thin-lens camera model.
        The camera jitter is taken into account to compute the sample position on the image plane.
        \param[in] pixel Pixel coordinates with origin in top-left.
        \param[in] frameDim Image plane dimensions in pixels.
        \param[in] u Uniform 2D sample.
        \return Returns the camera ray.
    */
    Ray computeRayThinlens(uint2 pixel, uint2 frameDim, float2 u, bool applyJitter = true) {
        const float2 rnd = applyJitter ? (float2(frameDim) * (float2(-data.jitterX, data.jitterY)) + float2(0.5)) : float2(0.5f, 0.5f);
        return computeRayThinlens(pixel, frameDim, u, rnd);
    }

    Ray computeRayThinlens(uint2 pixel, uint2 frameDim, float2 u, float2 rnd) {
        Ray ray;

        // Sample position in screen space in [0,1] with origin at the top-left corner.
        // Variable rnd [0,1] offsets the sample by +-0.5 pixels from the pixel center.
        float2 p = (float2(pixel) + rnd) / frameDim;
        float2 ndc = float2(2, -2) * p + float2(-1, 1);

        // Compute the normalized ray direction assuming a thin-lens camera.
        ray.origin = getPosW();
        ray.dir = ndc.x * xform.cameraU + ndc.y * xform.cameraV + xform.cameraW;
        float2 apertureSample = sample_disk(u); // Sample lies in the unit disk [-1,1]^2
        float3 rayTarget = ray.origin + ray.dir;
        ray.origin += data.apertureRadius * (apertureSample.x * normalize(xform.cameraU) + apertureSample.y * normalize(xform.cameraV));
        ray.dir = normalize(rayTarget - ray.origin);

        float invCos = 1.f / dot(normalize(xform.cameraW), ray.dir);
        ray.tMin = data.nearZ * invCos;
        ray.tMax = data.farZ * invCos;

        return ray;
    }

    // Time jittered versions

    Ray computeRayPinholeMB(uint2 pixel, uint2 frameDim, float tj, bool applyJitter = true) {
        const float2 rnd = applyJitter ? (float2(frameDim) * (float2(-data.jitterX, data.jitterY)) + float2(0.5)) : float2(0.5f, 0.5f);
        return computeRayPinholeMB(pixel, frameDim, rnd, tj);
    }

    Ray computeRayPinholeMB(uint2 pixel, uint2 frameDim, float2 rnd, float tj) {
        Ray ray;

        CameraXformData xform_tj = getXformData(tj);

        // Compute the normalized ray direction assuming a pinhole camera.
        ray.origin = xform_tj.getPosW();

        float2 p = (float2(pixel) + rnd) / frameDim;
        float2 ndc = float2(2, -2) * p + float2(-1, 1);
        ray.dir = ndc.x * xform_tj.cameraU + ndc.y * xform_tj.cameraV + xform_tj.cameraW;

        float invCos = 1.f / dot(normalize(xform_tj.cameraW), ray.dir);
        ray.tMin = data.nearZ * invCos;
        ray.tMax = data.farZ * invCos;

        return ray;
    }

    float3 computeNonNormalizedRayDirPinholeMB(uint2 pixel, uint2 frameDim, float tj, bool applyJitter = true) {
        const float2 rnd = applyJitter ? (float2(frameDim) * (float2(-data.jitterX, data.jitterY)) + float2(0.5)) : float2(0.5f, 0.5f);
        return computeNonNormalizedRayDirPinholeMB(pixel, frameDim, rnd, tj);
    }

    float3 computeNonNormalizedRayDirPinholeMB(uint2 pixel, uint2 frameDim, float2 rnd, float tj) {
        // Compute sample position in screen space in [0,1] with origin at the top-left corner.
        // Variable rnd [0,1] offsets the sample by +-0.5 pixels from the pixel center.
        float2 p = (float2(pixel) + rnd) / frameDim;
        float2 ndc = float2(2, -2) * p + float2(-1, 1);

        CameraXformData xform_tj = getXformData(tj);

        // Compute the non-normalized ray direction assuming a pinhole camera.
        return ndc.x * xform_tj.cameraU + ndc.y * xform_tj.cameraV + xform_tj.cameraW;
    }

    Ray computeRayThinlensMB(uint2 pixel, uint2 frameDim, float2 u, float tj, bool applyJitter = true) {
        const float2 rnd = applyJitter ? (float2(frameDim) * (float2(-data.jitterX, data.jitterY)) + float2(0.5)) : float2(0.5f, 0.5f);
        return computeRayThinlensMB(pixel, frameDim, u, rnd, tj);
    }

    Ray computeRayThinlensMB(uint2 pixel, uint2 frameDim, float2 u, float2 rnd, float tj) {
        Ray ray;

        // Sample position in screen space in [0,1] with origin at the top-left corner.
        // Variable rnd [0,1] offsets the sample by +-0.5 pixels from the pixel center.
        float2 p = (float2(pixel) + rnd) / frameDim;
        float2 ndc = float2(2, -2) * p + float2(-1, 1);

        CameraXformData xform_tj = getXformData(tj);

        // Compute the normalized ray direction assuming a thin-lens camera.
        ray.origin = xform_tj.getPosW();
        ray.dir = ndc.x * xform_tj.cameraU + ndc.y * xform_tj.cameraV + xform_tj.cameraW;
        float2 apertureSample = sample_disk(u); // Sample lies in the unit disk [-1,1]^2
        float3 rayTarget = ray.origin + ray.dir;
        ray.origin += data.apertureRadius * (apertureSample.x * normalize(xform_tj.cameraU) + apertureSample.y * normalize(xform_tj.cameraV));
        ray.dir = normalize(rayTarget - ray.origin);

        float invCos = 1.f / dot(normalize(xform_tj.cameraW), ray.dir);
        ray.tMin = data.nearZ * invCos;
        ray.tMax = data.farZ * invCos;

        return ray;
    }
};
