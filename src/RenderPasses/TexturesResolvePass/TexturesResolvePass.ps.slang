import Scene.Raster;
import Scene.Shading;
import Utils.Helpers;

#include "TexturesResolveData.slangh"

cbuffer PerFrameCB {
    float2 gRenderTargetDim;
    uint   materialsResolveDataSize = 0;
};

StructuredBuffer<MaterialResolveData> materialsResolveData;

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

float calcMipMapLevel(float2 tec_coord) {
    // The OpenGL Graphics System: A Specification 4.2
    //  - chapter 3.9.11, equation 3.21

    float2 dx_vtc = ddx(tec_coord);
    float2 dy_vtc = ddy(tec_coord);
    float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));

    return 0.5 * log2(delta_max_sqr);
}

PsOut psMain(VSOut vOut, uint triangleIndex : SV_PrimitiveID) {

    const int lodBase[8] = { 0, 1, 5, 21, 85, 341, 1365, 5461};

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
    
    uint materialID = vOut.materialID;
    MaterialResolveData matData = materialsResolveData[materialID];

    int debugTileID = 0;

    for(uint i=0; i < materialsResolveDataSize; i++) {
        uint virtualTexturesCount = matData.virtualTexturesCount;
        for(uint v = 0; v < virtualTexturesCount; v++) {
            VirtualTextureData vtexData = matData.virtualTextures[v];

            float textureWidth = vtexData.width;
            float textureHeight = vtexData.height;

            float lodW = fwidth(vOut.texC.x);
            float lodH = fwidth(vOut.texC.y);

            float lodX = textureWidth * lodW - 1.0f;
            float lodY = textureHeight * lodH - 1.0f;

            int iLod = int(clamp(floor(min(lodX, lodY)), 0, vtexData.mipLevelsCount));
            
            float currentMipTextureWidth = textureWidth / pow(2, iLod);
            float currentMipTextureHeight = textureHeight / pow(2, iLod);

            uint cellsNumX = uint(ceil(currentMipTextureWidth / vtexData.pageSizeW)); // number of pages including partial last page on this mip level(iLod)
            uint cellsNumY = uint(ceil(currentMipTextureHeight / vtexData.pageSizeH)); // number of pages including partial last page on this mip level(iLod) 

            float k_u = currentMipTextureWidth / (vtexData.pageSizeW * cellsNumX); // texture coordinate renormalization coefficient (addresses partial tiles)
            float k_v = currentMipTextureHeight / (vtexData.pageSizeH * cellsNumY); // texture coordinate renormalization coefficient (addresses partial tiles)

            int tX = int(vOut.texC.x * k_u * cellsNumX); //column index in current level of detail
            int tY = int(vOut.texC.y * k_v * cellsNumY); //row index in current level of detail

            int tileID = lodBase[iLod] + tX + tY * cellsNumX; //global tile index
            debugTileID = tileID;
        }
    }

    // ---------------------------------
    psOut.debugColor = float4(0, 0, 0, 0);
    psOut.debugColor = dbgColors[divMod(debugTileID, 16)];
    psOut.debugColor = float4(ddx(vOut.texC.x), 0, 0, 1);
    // ---------------------------------

    return psOut;
}

/*
PsOut psMain(VSOut vOut, uint triangleIndex : SV_PrimitiveID) {

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
    //psOut.debugColor = float4( materialResources.baseColor.SampleLevel(gSampler, vOut.texC, 0).rgb, 1);

    //psOut.debugColor = materialsResolveData[materialID].virtualTextures[0].testColor;

    psOut.debugColor = float4(0, 0, 0, 0);
    psOut.debugColor.g = 0.1 * iLod;

    float4 sp_color = float4(0, 0, 0, 0);
    float inLodBias = 0.0;

    //int residencyCode = sparseTexture(gSampler, vOut.texC, sp_color, inLodBias);

    return psOut;
}
*/

/*

uint txw, txh;
        t.GetDimensions(txw, txh);

*/