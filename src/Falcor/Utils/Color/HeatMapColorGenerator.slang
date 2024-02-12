#include "Utils/Math/MathConstants.slangh"
import Utils.Math.MathHelpers;

struct HeatMapColorGenerator {
    float4 getColor(float temp) {
        float u = halfTexelSize + temp * uvScale; 
        return colorsTexture.SampleLevel(sampler, u, 0);
    }

    float                       uvScale;
    float                       halfTexelSize;
    uint                        numColors;
    Texture1D<float4>           colorsTexture;
    SamplerState                sampler;
};