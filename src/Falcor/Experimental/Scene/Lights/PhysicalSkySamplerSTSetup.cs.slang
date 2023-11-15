/** Compute shader for building a hierarchical importance map from an
    environment map. The result is used by EnvMapSampler.slang for sampling.
*/

#include "PhysicalSkySampler.common.slangh"

import Utils.Math.MathHelpers;
import Utils.Color.ColorHelpers;

cbuffer CB {
    uint2   stMapDim;            // Resolution of the sun transmittance map (LUT) in texels.
    uint2   msMapDim;            // Resolution of the multiple scatter map (LUT) in texels.
    uint2   svMapDim;            // Resolution of the sky view map (LUT) in texels.
    float   groundRadiusMM;
    float   atmosphereRadiusMM;
    float   sunTransmittanceSteps;
};

RWTexture2D<float4>     gSunTransmittanceMap;


float3 getSunTransmittance(float3 pos, float3 sunDir) {
    if (rayIntersectSphere(pos, sunDir, groundRadiusMM) > 0.0) {
        return float3(0.f);
    }

    float atmoDist = rayIntersectSphere(pos, sunDir, atmosphereRadiusMM);
    float t = 0.0;
    
    float3 transmittance = float3(1.0);
    for (float i = 0.0; i < sunTransmittanceSteps; i += 1.0) {
        float newT = ((i + 0.3)/sunTransmittanceSteps)*atmoDist;
        float dt = newT - t;
        t = newT;
        
        float3 newPos = pos + t*sunDir;
        
        float3 rayleighScattering, extinction;
        float mieScattering;
        getScatteringValues(newPos, rayleighScattering, mieScattering, extinction);
        
        transmittance *= exp(-dt*extinction);
    }
    return transmittance;
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint2 pixel = dispatchThreadID.xy;
    if (any(pixel >= stMapDim)) return;

    float u = pixel.x / ((float)stMapDim.x - 1.0);
    float v = pixel.y / ((float)stMapDim.y - 1.0);
    
    float sunCosTheta = 2.0*u - 1.0;
    float sunTheta = safeacos(sunCosTheta);
    float height = lerp(groundRadiusMM, atmosphereRadiusMM, v);
    
    float3 pos = float3(0.0, height, 0.0); 
    float3 sunDir = normalize(float3(0.0, sunCosTheta, -sin(sunTheta)));
    
    gSunTransmittanceMap[pixel] = float4(getSunTransmittance(pos, sunDir), 1.0);
    gSunTransmittanceMap[pixel] = float4(u, v, 0.0, 1.0);
}
