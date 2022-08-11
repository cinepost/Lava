import Scene.Raster;
import Scene.Shading;
// import Utils.Helpers;
import Experimental.Scene.Material.TexLODHelpers;

import TexturesResolveCommon;


cbuffer PerFrameCB {
    float2 gRenderTargetDim;
    uint   materialsToResolveCount = 0;
    uint   resolvedTexturesCount = 0;
};

StructuredBuffer<MaterialResolveData> materialsResolveData;

Texture2D mipCalibrationTexture;
Texture2D ltxCalibrationTexture;

RWByteAddressBuffer resolvedPagesBuff; // all virtual pages needed buffer, 0 means unused page

SamplerState gSampler;

// Vertex

static VSOut vsData;

[shader("vertex")]
VSOut vsMain(VSIn vIn) {
    VSOut vsOut;
    vsOut = defaultVS(vIn);
    return vsOut;
}

// Pixel 
struct PsOut {
    float4 output   : SV_TARGET0;
};

// set corresponding byte to 1
void writeTileID(int tileID) {
    uint byte4_addr = int(floor(float(tileID) / 4.0) * 4);
    uint byte_num = tileID - byte4_addr;
    resolvedPagesBuff.InterlockedOr(byte4_addr, 1 << (byte_num * 8));
}

// calculate required texture mip levels using calibration texture
float3 calcTextureMipLevels(float2 uv, SamplerState calibrationSampler, VirtualTextureData vtexData, float mipBias) {
    // bias scale
    float bias = pow(2, mipBias);

    float2 vtexSize = float2(vtexData.width, vtexData.height);

    float2 tc = uv * float2(vtexData.width / 128.0, vtexData.height / 128.0);

    float  mc = float(vtexData.mipLevelsCount) / 8.0f;
    float2 gradX = ddx_fine(tc);
    float2 gradY = ddy_fine(tc);

    float fMipLevel = mipCalibrationTexture.SampleGrad(calibrationSampler, float2(0.5, 0.5), gradX, gradY).r * 255.0;
    
    //TODO: Sample additional mips (9+ levels)
    //float fMipLevel2 = mipCalibrationTexture.SampleGrad(calibrationSampler, float2(0.5, 0.5), gradX*0.5, gradY*0.5).r * 255.0;
    
    float f1 = floor(fMipLevel);
    float f2 = ceil(fMipLevel);

    return float3(f1, f2, fMipLevel);
}

[earlydepthstencil]
PsOut psMain(VSOut vOut, uint triangleIndex : SV_PrimitiveID, float2 screenTexC : TEXCOORD) {

    PsOut psOut;
    
    uint materialID = vOut.materialID;
    MaterialResolveData matData = materialsResolveData[materialID];

    int debugTileID = 0;
    float debug_mip_level_lo = 0.0;
    float debug_mip_level_hi = 0.0;
    float debug_mip_level_f  = 0.0;
    uint virtualTexturesCount = matData.virtualTexturesCount;
    
    for(uint v = 0; v < virtualTexturesCount; v++) {
        SamplerState currTextureSampler = gSampler; // TODO: use actual texture sampler
        bool anisotropicFilteringRequired = true; // TODO: calculate maximum anisotropy level needed for given texture with the given sampler
        
        VirtualTextureData vtexData = matData.virtualTextures[v];
        uint32_t textureID = vtexData.textureID;

        if (!vtexData.empty) {
            float mipBias = -0.0; // TODO: use actual texture sampler bias

            float3 mips = calcTextureMipLevels(vOut.texC, gSampler, vtexData, mipBias);

            int loMipLevel = max(0, (int)mips.x);
            int hiMipLevel = max(0, (int)mips.y);

            debug_mip_level_lo = loMipLevel * 0.1;
            debug_mip_level_hi = hiMipLevel * 0.1;
            debug_mip_level_f  = mips.z * 0.1;

            int loMipLevelTileID = calculateTileID2D(vOut.texC, loMipLevel, vtexData);
            int hiMipLevelTileID = calculateTileID2D(vOut.texC, hiMipLevel, vtexData);

            writeTileID(loMipLevelTileID);
            writeTileID(hiMipLevelTileID);

            debugTileID = loMipLevelTileID;
        }
    }

    psOut.output = float4(debug_mip_level_lo, debug_mip_level_hi, debug_mip_level_f, 1);
    return psOut;
}
