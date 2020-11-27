//#extension GL_ARB_sparse_texture2 : enable
//#extension GL_ARB_sparse_texture_clamp : enable

import Scene.Raster;
import Scene.Shading;
import Utils.Helpers;

#include "TexturesResolveData.slangh"

cbuffer PerFrameCB {
    float2 gRenderTargetDim;
};

static int lodBase[8] = { 0, 1, 5, 21, 85, 341, 1365, 5461};

/*
static float4 dbgColors[16] = {
    {1, 0, 0, 1},
    float4(0, 1, 0, 1),
    float4(0, 0, 1, 1),
    float4(1, 1, 0, 1),
    
    float4(0, 1, 1, 1),
    float4(1, 0, 1, 1),

    float4(1, 0.5, 0, 1),
    float4(0.5, 1, 0, 1),
    
    float4(0, 0.5, 1, 1),
    float4(1, 1, 0.5, 1),
    float4(0.5, 1, 1, 1),
    float4(1, 0.5, 1, 1),

    float4(0, 0.5, 0, 1),
    float4(0, 0, 0.5, 1),
    float4(0.5, 0, 0, 1),
    float4(0.5, 0.5, 0.5, 1)
};
*/

SamplerState gSampler;

// Vertex

static VSOut vsData;

VSOut vsMain(VSIn vIn) {
    VSOut vsOut;
    vsOut = defaultVS(vIn);
    return vsOut;
}

// Pixel 

struct PsOut {
    uint3  tileData     : SV_TARGET0;
    float4 debugColor   : SV_TARGET1;
};


uint divMod(int x, int y) {
    return x - y * floor(float(x) / float(y));
};

PsOut psMain(VSOut vOut, uint triangleIndex : SV_PrimitiveID) {

    const float4 dbgColors[16] = {
        {1, 0, 0, 1},
        {0, 1, 0, 1},
        {0, 0, 1, 1},
        {1, 1, 0, 1},
        {0, 1, 1, 1},
        {1, 0, 1, 1},
        {1, 0.5, 0, 1},
        {0.5, 1, 0, 1},
        {0, 0.5, 1, 1},
        {1, 1, 0.5, 1},
        {0.5, 1, 1, 1},
        {1, 0.5, 1, 1},
        {0, 0.5, 0, 1},
        {0, 0, 0.5, 1},
        {0.5, 0, 0, 1},
        {0.5, 0.5, 0.5, 1}
    };

    PsOut psOut;
    
    float texPageW = 512.0f;
    float texPageH = 512.0f;
    
    uint materialID = vOut.materialID;
    MaterialResources materialResources = gScene.materialResources[materialID];

    uint2 texDims;
    materialResources.baseColor.GetDimensions(texDims.x, texDims.y);

    float lodW = fwidth(vOut.texC.x);
    float lodH = fwidth(vOut.texC.y);


    //float lodX = log2(1.0f / (texDims.x * lodW));
    //float lodY = log2(1.0f / (texDims.y * lodH));

    float lodX = texDims.x * lodW - 1.0f;
    float lodY = texDims.y * lodH - 1.0f;

    int iLod = int(clamp(floor(max(lodX, lodY)), 0, 7));
    int cellsSize = int(pow(2,iLod));

    int tX = int(vOut.texC.x * cellsSize); //column index in current level of detail
    int tY = int(vOut.texC.y * cellsSize); //row index in current level of detail

    int tileID = lodBase[iLod] + tX + tY * cellsSize; //global tile index

    psOut.tileData = uint3(1, 1, 1);
    //psOut.debugColor = float4(vOut.texC.x, vOut.texC.y, 0, 1);

    //float4 color = dbgColors[divMod(tileID, 16)];
    float4 color = dbgColors[iLod];

    psOut.debugColor = color; //float4(lodW, lodH, 0, 1);
    //psOut.debugColor = float4(vOut.texC.y, 0, 0, 1);

    psOut.debugColor = float4( materialResources.baseColor.Sample(gSampler, vOut.texC).rgb, 1);

    float4 sp_color = float4(0, 0, 0, 0);
    float inLodBias = 0.0;

    //int residencyCode = sparseTexture(gSampler, vOut.texC, sp_color, inLodBias);

    return psOut;
}

/*

uint txw, txh;
        t.GetDimensions(txw, txh);

*/