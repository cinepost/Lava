#include "Utils/Math/MathConstants.slangh"
import Utils.Math.MathHelpers;

struct FalseColorGenerator {
    float4 getColor(uint idx) {
        return colorsBuffer[idx % numColors];
    }

    StructuredBuffer<float4>    colorsBuffer;
    uint                        numColors;
};