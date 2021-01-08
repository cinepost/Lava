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

// Vertex

static VSOut vsData;

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
    uint virtualTexturesCount = matData.virtualTexturesCount;
    
    for(uint v = 0; v < virtualTexturesCount; v++) {
        
        SamplerState currTextureSampler = gSampler; // TODO: use actual texture sampler
        bool anisotropicFileteringRequired = false; // TODO: calculate maximum anisotropy level needed for given texture with the given sampler
        
        VirtualTextureData vtexData = matData.virtualTextures[v];
        uint32_t textureID = vtexData.textureID;

        if (!vtexData.empty) {
            float fMipLevel = calculateMaxMipMapLevel2D(vOut.texC, float2(vtexData.width, vtexData.height));
            
            int loMipLevelTileID = calculateTileID2D(vOut.texC, floor(fMipLevel), vtexData);
            int hiMipLevelTileID = calculateTileID2D(vOut.texC, ceil(fMipLevel), vtexData);
            
            writeTileID(loMipLevelTileID);
            writeTileID(hiMipLevelTileID);

            debugTileID = loMipLevelTileID;

            // If required we need to analytically ro resolve lower mip level tiles needed for anisotropic filtering.
            if (anisotropicFileteringRequired) {
                // TODO: implement (using sampler feedback)
            }

        }
    }


    // ---------------------------------
    psOut.debugColor = float4(0, 0, 0, 0);

    //float3 viewDir = normalize(gScene.camera.getPosition() - vOut.posW);
    //psOut.debugColor.r = 1.0 - dot(viewDir, vOut.normalW);
    //psOut.debugColor = randomTileColor(debugTileID);
    // ---------------------------------

    return psOut;
}

/*

uint txw, txh;
        t.GetDimensions(txw, txh);

*/