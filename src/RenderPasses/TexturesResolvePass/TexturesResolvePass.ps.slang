import Scene.Raster;
import Scene.Shading;
// import Utils.Helpers;
import Experimental.Scene.Materials.TexLODHelpers;

import TexturesResolveCommon;


cbuffer PerFrameCB {
    float2 gRenderTargetDim;
    uint   materialsToResolveCount = 0;
    uint   resolvedTexturesCount = 0;
    int    numberOfMipCalibrationTextures = 0;
};

StructuredBuffer<VirtualTextureData>  virtualTextureDataBuffer;
StructuredBuffer<MaterialResolveData> materialsResolveDataBuffer;
Buffer<uint32_t> textureIdToVtexDataIdBuffer;

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

struct gl_TextureFootprint2DNV {
    uint2 anchor;
    uint2 offset;
    uint2 mask;
    uint lod;
    uint granularity;
};

//    SpvOpImageSparseSampleImplicitLod = 305,


// OpImageSampleFootprintNV
[[vk::spirv_instruction(5283)]]
bool textureFootprintGradNV(out gl_TextureFootprint2DNV footprint, Sampler2D sampler, float2 uv, int granularity, bool coarse, float2 dx, float2 dy);

float2 clacTextureMipLevelsIterative(float2 uv, SamplerState calibrationSampler, VirtualTextureData vtexData, float mipBias) {

    float2 tc = uv * float2((float)vtexData.width / 128.0f, (float)vtexData.height / 128.0f);
    float c = 1.1f;  // x1.1 should be enough with 16x anisotropic filtering, otherwise -> x2;

    float2 gradX = ddx_fine(tc) * c;
    float2 gradY = ddy_fine(tc) * c;

    gl_TextureFootprint2DNV footprint;
    //Sampler2D<vector<float,4>> tst(mipCalibrationTextures[0], gCalibrationSampler);
    //bool _res = textureFootprintGradNV(footprint, tst , uv,  1, false, ddx_fine(tc), ddy_fine(tc));

    //float _mip = ltxCalibrationTexture.CalculateLevelOfDetail(gCalibrationSampler, uv);

    int mipLevelLo = -1;
    int mipLevelHi = -1;

    for(int i = 0; i < numberOfMipCalibrationTextures; i++) {
        if (mipCalibrationTextures[i].SampleGrad(calibrationSampler, uv, gradX, gradY) > 0.0f) {
            mipLevelLo = (mipLevelLo == -1) ? i : min(mipLevelLo, i);
            mipLevelHi = (mipLevelHi == -1) ? i : max(mipLevelHi, i);
        }   
    }

    if ( mipLevelLo == mipLevelHi ) {
        mipLevelLo -= 1; 
    }

    return float2(max(0, mipLevelLo), max(0, mipLevelHi));
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

#if _MAX_RAY_LEVEL == 0
//[earlydepthstencil]
#endif
PsOut psMain(VSOut vOut, uint triangleIndex : SV_PrimitiveID, float2 screenTexC : TEXCOORD) {

    PsOut psOut;
    
    uint materialID = vOut.materialID;
    MaterialResolveData matData = materialsResolveDataBuffer[materialID];

    int debugTileID = 0;
    float debug_mip_level_lo = 0.0;
    float debug_mip_level_hi = 0.0;
    float debug_mip_level_f  = 0.0;
    
    float4 grad_test;

    [loop]
    for(uint v = 0; v < matData.virtualTexturesCount; v++) {
        SamplerState currTextureSampler = gCalibrationSampler; // TODO: use actual texture sampler
        
        float2 uv = vOut.texC;
        uint32_t vtexDataID = matData.virtualTextureDataIDs[v];
        VirtualTextureData vtexData = virtualTextureDataBuffer[vtexDataID];

        switch (vtexData.textureHandle.getMode()) {
            case TextureHandle::Mode::UDIM_Texture:
                uint2 udim_idx = uint2((uint)floor(uv.x), (uint)floor(uv.y));
                uv = float2(uv.x - (float)udim_idx.x, 1.0 - (uv.y - (float)udim_idx.y));
                uint tileID =  gScene.materials.udimTextureTilesTable[vtexData.textureHandle.udimID * 100 + udim_idx.x + udim_idx.y * 10];
                vtexDataID = textureIdToVtexDataIdBuffer[tileID];
                vtexData = virtualTextureDataBuffer[vtexDataID];
                break;
            case TextureHandle::Mode::Texture:
                uv = float2(uv.x , 1.0 - uv.y);
                break;
            default:
                continue;
        }

        if (!vtexData.empty) {
            float mipBias = -0.0; // TODO: use actual texture sampler bias

            float2 mips = clacTextureMipLevelsIterative(uv, gCalibrationSampler, vtexData, mipBias);

            int loMipLevel = (int)mips.x;
            int hiMipLevel = (int)mips.y;

            debug_mip_level_lo = loMipLevel / 255.0f;
            debug_mip_level_hi = hiMipLevel / 255.0f;

            for( int m = loMipLevel; m <= hiMipLevel; m++) {
                if(m < vtexData.mipTailStart) writeTileID(calculateTileID2D(uv, m, vtexData));
            }

            //grad_test = clacTextureMipLevelsIterativeDBG(uv, gCalibrationSampler, vtexData, mipBias);
        }
    }


#ifdef _OUTPUT_DEBUG_IMAGE
    //psOut.output = grad_test;
    psOut.output = float4(debug_mip_level_lo, debug_mip_level_hi, 0, 1);
    return psOut;
#else
    discard;
#endif
}
