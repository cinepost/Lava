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
    float3  groundAlbedo;
    float   mulScattSteps;
    int     sqrtSamples;
};

SamplerState gLUTSampler;

Texture2D<float4>       gSunTransmittanceMap;
RWTexture2D<float4>     gMultiScatterMap;

float3 getSphericalDir(float theta, float phi) {
     float cosPhi = cos(phi);
     float sinPhi = sin(phi);
     float cosTheta = cos(theta);
     float sinTheta = sin(theta);
     return float3(sinPhi*sinTheta, cosPhi, sinPhi*cosTheta);
}

// Calculates Equation (5) and (7) from the paper.
void getMulScattValues(float3 pos, float3 sunDir, out float3 lumTotal, out float3 fms) {
    lumTotal = float3(0.0);
    fms = float3(0.0);
    
    float invSamples = 1.0/float(sqrtSamples*sqrtSamples);
    for (int i = 0; i < sqrtSamples; i++) {
        for (int j = 0; j < sqrtSamples; j++) {
            // This integral is symmetric about theta = 0 (or theta = M_PI), so we
            // only need to integrate from zero to M_PI, not zero to 2*M_PI.
            float theta = M_PI * (float(i) + 0.5) / float(sqrtSamples);
            float phi = safeacos(1.0 - 2.0*(float(j) + 0.5) / float(sqrtSamples));
            float3 rayDir = getSphericalDir(theta, phi);
            
            float atmoDist = rayIntersectSphere(pos, rayDir, atmosphereRadiusMM);
            float groundDist = rayIntersectSphere(pos, rayDir, groundRadiusMM);
            float tMax = atmoDist;
            if (groundDist > 0.0) {
                tMax = groundDist;
            }
            
            float cosTheta = dot(rayDir, sunDir);
    
            float miePhaseValue = getMiePhase(cosTheta);
            float rayleighPhaseValue = getRayleighPhase(-cosTheta);
            
            float3 lum = float3(0.0), lumFactor = float3(0.0), transmittance = float3(1.0);
            float t = 0.0;
            for (float stepI = 0.0; stepI < mulScattSteps; stepI += 1.0) {
                float newT = ((stepI + 0.3)/mulScattSteps)*tMax;
                float dt = newT - t;
                t = newT;

                float3 newPos = pos + t*rayDir;

                float3 rayleighScattering, extinction;
                float mieScattering;
                getScatteringValues(newPos, rayleighScattering, mieScattering, extinction);

                float3 sampleTransmittance = exp(-dt*extinction);
                
                // Integrate within each segment.
                float3 scatteringNoPhase = rayleighScattering + mieScattering;
                float3 scatteringF = (scatteringNoPhase - scatteringNoPhase * sampleTransmittance) / extinction;
                lumFactor += transmittance*scatteringF;
                
                // This is slightly different from the paper, but I think the paper has a mistake?
                // In equation (6), I think S(x,w_s) should be S(x-tv,w_s).
                float3 sunTransmittance = getValFromSTLUT(gLUTSampler, gSunTransmittanceMap, float2(stMapDim), float2(msMapDim), newPos, sunDir);
                fms += sunTransmittance;

                float3 rayleighInScattering = rayleighScattering*rayleighPhaseValue;
                float mieInScattering = mieScattering*miePhaseValue;
                float3 inScattering = (rayleighInScattering + mieInScattering)*sunTransmittance;

                // Integrated scattering within path segment.
                float3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / extinction;

                lum += scatteringIntegral * transmittance;
                transmittance *= sampleTransmittance;
            }
            
            if (groundDist > 0.0) {
                float3 hitPos = pos + groundDist*rayDir;
                if (dot(pos, sunDir) > 0.0) {
                    hitPos = normalize(hitPos) * groundRadiusMM;
                    lum += transmittance * groundAlbedo * getValFromSTLUT(gLUTSampler, gSunTransmittanceMap, float2(stMapDim), float2(msMapDim), hitPos, sunDir);
                }
            }
            
            //fms += lumFactor*invSamples;
            lumTotal += lum*invSamples;
        }
    }
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint2 pixel = dispatchThreadID.xy;
    if (any(pixel >= msMapDim)) return;

    float u = pixel.x / ((float)msMapDim.x - 1.0);
    float v = pixel.y / ((float)msMapDim.y - 1.0);
    
    float sunCosTheta = 2.0*u - 1.0;
    float sunTheta = safeacos(sunCosTheta);
    float height = lerp(groundRadiusMM, atmosphereRadiusMM, v);
    
    float3 pos = float3(0.0, height, 0.0); 
    float3 sunDir = normalize(float3(0.0, sunCosTheta, -sin(sunTheta)));
    
    float3 lum, f_ms;
    getMulScattValues(pos, sunDir, lum, f_ms);
    
    // Equation 10 from the paper.
    float3 psi = lum  / (1.0 - f_ms); 
    //gMultiScatterMap[pixel] = float4(psi, 1.0);
    gMultiScatterMap[pixel] = float4(f_ms * 0.001, 1.0);
}
