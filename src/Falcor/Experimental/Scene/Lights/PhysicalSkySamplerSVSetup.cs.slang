/** Compute shader for building a hierarchical importance map from an
    environment map. The result is used by EnvMapSampler.slang for sampling.
*/

#include "PhysicalSkySampler.common.slangh"

import Utils.Math.MathHelpers;
import Utils.Color.ColorHelpers;

#ifndef SUN_MODE
#define SUN_MODE 0 // 0 - using sun direction, 1 - using time
#endif

cbuffer CB {
    uint2   stMapDim;            // Resolution of the sun transmittance map (LUT) in texels.
    uint2   msMapDim;            // Resolution of the multiple scatter map (LUT) in texels.
    uint2   svMapDim;            // Resolution of the sky view map (LUT) in texels.
    float   groundRadiusMM = 6.360;
    float   atmosphereRadiusMM = 6.460;
    float3  viewPos;
    float3  sunDir;
    float   time;
};

SamplerState gLUTSampler;

Texture2D<float4>     gSunTransmittanceMap;
Texture2D<float4>     gMultiScatterMap;
RWTexture2D<float4>   gSkyViewMap;

// Buffer C calculates the actual sky-view! It's a lat-long map (or maybe altitude-azimuth is the better term),
// but the latitude/altitude is non-linear to get more resolution near the horizon.

const int numScatteringSteps = 32;

float3 raymarchScattering(float3 pos,  float3 rayDir, float3 sunDir, float tMax, float numSteps) {
    float cosTheta = dot(rayDir, sunDir);
    
    float miePhaseValue = getMiePhase(cosTheta);
    float rayleighPhaseValue = getRayleighPhase(-cosTheta);
    
    float3 lum = float3(0.0);
    float3 transmittance = float3(1.0);
    float t = 0.0;
    for (float i = 0.0; i < numSteps; i += 1.0) {
        float newT = ((i + 0.3)/numSteps)*tMax;
        float dt = newT - t;
        t = newT;
        
        float3 newPos = pos + t*rayDir;
        
        float3 rayleighScattering, extinction;
        float mieScattering;
        getScatteringValues(newPos, rayleighScattering, mieScattering, extinction);
        
        float3 sampleTransmittance = exp(-dt*extinction);

        float3 sunTransmittance = getValFromSTLUT(gLUTSampler, gSunTransmittanceMap, float2(stMapDim), float2(svMapDim), newPos, sunDir);
        float3 psiMS = getValFromMSLUT(gLUTSampler, gMultiScatterMap, float2(msMapDim), float2(svMapDim), newPos, sunDir);
        
        float3 rayleighInScattering = rayleighScattering*(rayleighPhaseValue*sunTransmittance + psiMS);
        float3 mieInScattering = mieScattering*(miePhaseValue*sunTransmittance + psiMS);
        float3 inScattering = (rayleighInScattering + mieInScattering);

        // Integrated scattering within path segment.
        float3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / extinction;

        lum += scatteringIntegral*transmittance;
        
        transmittance *= sampleTransmittance;
    }
    return lum;
}


[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint2 pixel = dispatchThreadID.xy;
    if (any(pixel >= svMapDim)) return;

    float u = clamp(pixel.x, 0.0, svMapDim.x-1.0)/svMapDim.x;
    float v = clamp(pixel.y, 0.0, svMapDim.y-1.0)/svMapDim.y;
    
    float azimuthAngle = (u - 0.5) * 2.0 * M_PI;
    // Non-linear mapping of altitude. See Section 5.3 of the paper.
    float adjV;
    if (v < 0.5) {
        float coord = 1.0 - 2.0*v;
        adjV = -coord*coord;
    } else {
        float coord = v*2.0 - 1.0;
        adjV = coord*coord;
    }
    
    float height = length(viewPos);
    float3 up = viewPos / height;
    float horizonAngle = safeacos(sqrt(height * height - groundRadiusMM * groundRadiusMM) / height) - (0.5 * M_PI);
    float altitudeAngle = (adjV * 0.5 * M_PI) - horizonAngle;
    
    float cosAltitude = cos(altitudeAngle);
    float3 rayDir = float3(cosAltitude*sin(azimuthAngle), sin(altitudeAngle), -cosAltitude*cos(azimuthAngle));
    
    #if SUN_MODE == 0
    float3 _sunDir = sunDir;
    #else
    float sunAltitude = (0.5 * M_PI) - acos(dot(getSunDir(iTime), up));
    float3 _sunDir = float3(0.0, sin(sunAltitude), -cos(sunAltitude));
    #endif

    float atmoDist = rayIntersectSphere(viewPos, rayDir, atmosphereRadiusMM);
    float groundDist = rayIntersectSphere(viewPos, rayDir, groundRadiusMM);
    float tMax = (groundDist < 0.0) ? atmoDist : groundDist;

    float3 lum = raymarchScattering(viewPos, rayDir, _sunDir, tMax, float(numScatteringSteps));
    gSkyViewMap[pixel] = float4(lum, 1.0);
}
