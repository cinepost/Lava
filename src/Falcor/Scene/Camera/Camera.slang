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
    float _c = saturate(c);
    CameraXformData result;

    result.viewProjMat = lhs.viewProjMat * c + rhs.viewProjMat * (1.0 - c);

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
    
    float4x4 getViewProjMatJittered() {
        float4x4 jitterMat = { 
            float4(1.0f, 0.0f, 0.0f, 0.0f),
            float4(0.0f, 1.0f, 0.0f, 0.0f),
            float4(0.0f, 0.0f, 1.0f, 0.0f),
            float4(2.0f * data.jitterX, 2.0f * data.jitterY, 0.0f, 1.0f)
        };
        float4x4 jitterMat1 = { 
            float4(1.0f, 1.0f, 1.0f, 1.0f),
            float4(1.0f, 1.0f, 1.0f, 1.0f),
            float4(1.0f, 1.0f, 1.0f, 1.0f),
            float4(2.0f * data.jitterX, 2.0f * data.jitterY, 0.0f, 1.0f)
        };
        float4x4 m = {
            xform.viewProjMat[0] + float4(data.jitterX * 0.f, data.jitterY * 20.f, 0, 0),
            xform.viewProjMat[1] + float4(data.jitterX * 0.f, data.jitterY * 20.f, 0, 0),
            xform.viewProjMat[2] + float4(data.jitterX * 0.f, data.jitterY * 20.f, 0, 0),
            xform.viewProjMat[3] + float4(data.jitterX * 0.f, data.jitterY * 20.f, 0, 0),
        }; 
        return (jitterMat * data.projMat) * xform.viewMat; 
    }
    
    float4x4 getViewProjMatNoJitter() { return xform.viewProjMat; }
    
    float4x4 getPrevViewProjMat() { return data.prevViewProjMat; }

    // Time dependent functions

    CameraXformData getXformData(float t) {
        if(xformListBufferSize == 0) return xform;
        
        float _t = saturate(t);
        float fidx = _t * (xformListBufferSize - 1);
        uint idx_l = floor(fidx);
        uint idx_h = ceil(fidx);
        return lerp(xformListBuffer[idx_l], xformListBuffer[idx_h], _t);
    }

    float3 getPosW(float t) { return {xform.viewInvMat[3][0], xform.viewInvMat[3][1], xform.viewInvMat[3][2]}; }
    
    float4x4 getViewProjMat(float t) { return getXformData(t).viewProjMat; }

    float4x4 getViewProjMatJittered(float t) { return getViewProjMatJittered(); }
    
    float4x4 getViewProjMatNoJitter(float t) { return getXformData(t).viewProjMat; }

    float4 getBackgroundColor() { return data.backgroundColor; }

    /** Computes a camera ray for a given pixel assuming a pinhole camera model.
        The camera jitter is taken into account to compute the sample position on the image plane.
        \param[in] pixel Pixel coordinates with origin in top-left.
        \param[in] frameDim Image plane dimensions in pixels.
        \param[in] applyJitter true if jitter should be applied else false.
        \return Returns the camera ray.
    */
    Ray computeRayPinhole(uint2 pixel, uint2 frameDim, bool applyJitter = true) {
        Ray ray;

        // Compute the normalized ray direction assuming a pinhole camera.
        ray.origin = getPosW();
        ray.dir = normalize(computeNonNormalizedRayDirPinhole(pixel, frameDim, applyJitter));

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
        // Compute sample position in screen space in [0,1] with origin at the top-left corner.
        // The camera jitter offsets the sample by +-0.5 pixels from the pixel center.
        float2 p = (pixel + float2(0.5f, 0.5f)) / frameDim;
        if (applyJitter) p += float2(-data.jitterX, data.jitterY);
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
    Ray computeRayThinlens(uint2 pixel, uint2 frameDim, float2 u) {
        Ray ray;

        // Sample position in screen space in [0,1] with origin at the top-left corner.
        // The camera jitter offsets the sample by +-0.5 pixels from the pixel center.
        float2 p = (pixel + float2(0.5f, 0.5f)) / frameDim + float2(-data.jitterX, data.jitterY);
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
};
