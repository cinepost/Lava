import Scene.Raster;
import Scene.Shading;
// import Utils.Helpers;
import Experimental.Scene.Material.TexLODHelpers;

import TexturesResolveCommon;


cbuffer PerFrameCB {
    float2 gRenderTargetDim;
    uint   materialsToResolveCount = 0;
    uint   resolvedTexturesCount = 0;
    int    numberOfMipCalibrationTextures = 0;
};

StructuredBuffer<MaterialResolveData> materialsResolveData;

Texture2D<float> mipCalibrationTexture;
Texture2D ltxCalibrationTexture;
Texture2D<float> mipCalibrationTextures[16];

RWByteAddressBuffer resolvedPagesBuff; // all virtual pages needed buffer, 0 means unused page

SamplerState gCalibrationSampler;
SamplerState gCalibrationMinSampler;
SamplerState gCalibrationMaxSampler;

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

float4 clacTextureMipLevelsIterative(float2 uv, SamplerState calibrationSampler, VirtualTextureData vtexData, float mipBias) {
    let lod = ImplicitLodTextureSampler();

    float2 tc = uv * float2((float)vtexData.width / 128.0f, (float)vtexData.height / 128.0f);
    
    float2 gradX = ddx_fine(tc) * 1.1f; // x1.1 should be enough with 16x anisotropic filtering, otherwise -> x2;
    float2 gradY = ddy_fine(tc) * 1.1f;

    int mipLevelLo = -1;
    int mipLevelHi = -1;

    float z_test = 0;

    for(int i = 0; i < numberOfMipCalibrationTextures; i++) {
        if (mipCalibrationTextures[i].SampleGrad(calibrationSampler, uv, gradX, gradY) > 0.0f) {
            mipLevelLo = (mipLevelLo == -1) ? i : min(mipLevelLo, i);
            mipLevelHi = (mipLevelHi == -1) ? i : max(mipLevelHi, i);
        }
       /* 
        if (mipCalibrationTextures[i].SampleGrad(gCalibrationMinSampler, uv, gradX, gradY).r > 0.0f) {
            mipLevelLo = (mipLevelLo == -1) ? i : min(mipLevelLo, i);
            mipLevelHi = (mipLevelHi == -1) ? i : max(mipLevelHi, i);
            //z_test += 1.0f;
        }
        if (mipCalibrationTextures[i].SampleGrad(gCalibrationMaxSampler, uv, gradX, gradY).r > 0.0f) {
            mipLevelLo = (mipLevelLo == -1) ? i : min(mipLevelLo, i);
            mipLevelHi = (mipLevelHi == -1) ? i : max(mipLevelHi, i);
            //z_test += 1.0f;
        }
        */
        
    }
    
    //z_test = mipCalibrationTextures[2].SampleGrad(calibrationSampler, uv, gradX, gradY).r;
    //z_test = lod.sampleTexture(mipCalibrationTexture, calibrationSampler, uv).r * 10.0f;
    //z_test = mipCalibrationTexture.Sample(calibrationSampler, uv) * 0.1;

    //if(uv.x > 0.5) {
    //z_test = mipCalibrationTextures[1].Sample(calibrationSampler, uv);
    //} else {
    //z_test = mipCalibrationTextures[3].SampleGrad(calibrationSampler, uv, gradX, gradY);
    //}

    if ( mipLevelLo == mipLevelHi ) {
        mipLevelLo -= 1; 
    }

    return float4(max(0, mipLevelLo), max(0, mipLevelHi), z_test, 255);
}

float4 clacTextureMipLevelsIterativeDBG(float2 uv, SamplerState calibrationSampler, VirtualTextureData vtexData, float mipBias) {
    let lod = ImplicitLodTextureSampler();

    float2 tc = float2(vtexData.width / 128.0, vtexData.height / 128.0);
    
    float2 gradX = ddx_fine(uv) * tc;
    float2 gradY = ddy_fine(uv) * tc;
    
    return float4(gradX.x, gradX.y, gradY.x, gradY.y) * 5.0f;
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
    
    float4 grad_test;

    for(uint v = 0; v < virtualTexturesCount; v++) {
        SamplerState currTextureSampler = gCalibrationSampler; // TODO: use actual texture sampler
        bool anisotropicFilteringRequired = true; // TODO: calculate maximum anisotropy level needed for given texture with the given sampler
        
        VirtualTextureData vtexData = matData.virtualTextures[v];
        uint32_t textureID = vtexData.textureID;

        if (!vtexData.empty) {
            float mipBias = -0.0; // TODO: use actual texture sampler bias

            //float3 mips = calcTextureMipLevels(vOut.texC, gCalibrationSampler, vtexData, mipBias);
            float4 mips = clacTextureMipLevelsIterative(vOut.texC, gCalibrationSampler, vtexData, mipBias);

            int loMipLevel = (int)mips.x; //max(0, (int)mips.x);
            int hiMipLevel = (int)mips.y; //max(0, (int)mips.y);

            debug_mip_level_lo = loMipLevel / 255.0f;
            debug_mip_level_hi = hiMipLevel / 255.0f;
            debug_mip_level_f  += mips.z;

            int loMipLevelTileID = calculateTileID2D(vOut.texC, loMipLevel, vtexData);
            int hiMipLevelTileID = calculateTileID2D(vOut.texC, hiMipLevel, vtexData);

            writeTileID(loMipLevelTileID);
            writeTileID(hiMipLevelTileID);

            //writeTileID(calculateTileID2D(vOut.texC, loMipLevel - 2, vtexData));
            //writeTileID(calculateTileID2D(vOut.texC, loMipLevel - 1, vtexData));
            //writeTileID(calculateTileID2D(vOut.texC, loMipLevel + 1, vtexData));
            //writeTileID(calculateTileID2D(vOut.texC, hiMipLevel + 1, vtexData));
            //writeTileID(calculateTileID2D(vOut.texC, hiMipLevel + 2, vtexData));

            debugTileID = loMipLevelTileID;

            //grad_test = clacTextureMipLevelsIterativeDBG(vOut.texC, gCalibrationSampler, vtexData, mipBias);
        }
    }

    //psOut.output = grad_test;
    psOut.output = float4(debug_mip_level_lo, debug_mip_level_hi, debug_mip_level_f, 1);
    return psOut;
}
