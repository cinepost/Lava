import Scene.Raster;
import Scene.Shading;
import Utils.Helpers;
import TexturesResolveCommon;


cbuffer PerFrameCB {
    float2 gRenderTargetDim;
    uint   materialsToResolveCount = 0;
    uint   resolvedTexturesCount = 0;
};

StructuredBuffer<MaterialResolveData> materialsResolveData;

RWByteAddressBuffer resolvedPagesBuff; // all virtual pages needed buffer, 0 means unused page
//RWByteAddressBuffer resolvedPagesBitsetBuff; // all virtual pages needed buffer, unset bits means unused page

SamplerState gSampler;
Texture2DArray <uint4>  g_tTex2du4a;

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
    float4 debugColor   : SV_TARGET0;
};

// set corresponding byte to 1
void writeTileID(int tileID) {
    uint byte4_addr = int(floor(float(tileID) / 4.0) * 4);
    uint byte_num = tileID - byte4_addr;
    resolvedPagesBuff.InterlockedOr(byte4_addr, 1 << (byte_num * 8));
}

[earlydepthstencil]
PsOut psMain(VSOut vOut, uint triangleIndex : SV_PrimitiveID, float2 screenTexC : TEXCOORD) {

    PsOut psOut;
    
    uint materialID = vOut.materialID;
    MaterialResolveData matData = materialsResolveData[materialID];

    int debugTileID = 0;
    float debug_mip_level = 0.0;
    uint virtualTexturesCount = matData.virtualTexturesCount;
    
    for(uint v = 0; v < virtualTexturesCount; v++) {
        
        SamplerState currTextureSampler = gSampler; // TODO: use actual texture sampler
        bool anisotropicFilteringRequired = true; // TODO: calculate maximum anisotropy level needed for given texture with the given sampler
        
        VirtualTextureData vtexData = matData.virtualTextures[v];
        uint32_t textureID = vtexData.textureID;

        if (!vtexData.empty) {
            float mipBias = -0.25; // TODO: use actual texture sampler bias
            mipBias = 0.0;
            
            float fMipLevel = 0.0;
            if (anisotropicFilteringRequired) { 
                fMipLevel = calculateAnisoMipMapLevel2D(vOut.texC, float2(vtexData.width, vtexData.height), mipBias);
            } else {
                fMipLevel = calculateBaseMipMapLevel2D(vOut.texC, float2(vtexData.width, vtexData.height), mipBias);
            }
            
            debug_mip_level = fMipLevel;

            int loMipLevel = floor(fMipLevel);
            int hiMipLevel = ceil(fMipLevel);
            
            int loMipLevelTileID = calculateTileID2D(vOut.texC, loMipLevel, vtexData);
            int hiMipLevelTileID = calculateTileID2D(vOut.texC, hiMipLevel, vtexData);

            writeTileID(loMipLevelTileID);
            writeTileID(hiMipLevelTileID);

            writeTileID(calculateTileID2D(vOut.texC, max(0.0, loMipLevel - 1), vtexData));

            debugTileID = loMipLevelTileID;
        }
    }


    // ---------------------------------
    psOut.debugColor = float4(0, 0, 0, 0);

    //float3 viewDir = normalize(gScene.camera.getPosition() - vOut.posW);
    psOut.debugColor = randomColor(debugTileID);
    psOut.debugColor = randomColor(floor(debug_mip_level));
    // ---------------------------------

    return psOut;
}
