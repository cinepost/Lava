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

RWByteAddressBuffer resolvedPagesBuff; // all virtual pages needed buffer, -1 means unused page
RWByteAddressBuffer resolvedPagesBitsetBuff; // all virtual pages needed buffer, unset bits means unused page

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
    float4 debugColor   : SV_TARGET1;
};

[earlydepthstencil]
PsOut psMain(VSOut vOut, uint triangleIndex : SV_PrimitiveID, float2 screenTexC : TEXCOORD) {

    PsOut psOut;
    
    uint materialID = vOut.materialID;
    MaterialResolveData matData = materialsResolveData[materialID];

    int debugTileID = 0;
    //float debugMipLevel = 0.0;
        
    uint virtualTexturesCount = matData.virtualTexturesCount;
    
    for(uint v = 0; v < virtualTexturesCount; v++) {
        
        VirtualTextureData vtexData = matData.virtualTextures[v];
        uint32_t textureID = vtexData.textureID;

        if (!vtexData.empty) {
            float fMipLevel = calculateMipMapLevel2D(vOut.texC, float2(vtexData.width, vtexData.height));
            //debugMipLevel = fMipLevel;

            int loMipLevelTileID = calculateTileID2D(vOut.texC, floor(fMipLevel), vtexData) + vtexData.pagesStartOffset;
            int hiMipLevelTileID = calculateTileID2D(vOut.texC, ceil(fMipLevel), vtexData) + vtexData.pagesStartOffset;

            // set corresponding bytes

            uint byte4_addr = int(floor(float(loMipLevelTileID) / 4.0) * 4);
            uint byte_num = loMipLevelTileID - byte4_addr;

            resolvedPagesBuff.InterlockedOr(byte4_addr, 1 << (byte_num * 8));

            byte4_addr = int(floor(float(hiMipLevelTileID) / 4.0) * 4);
            byte_num = hiMipLevelTileID - byte4_addr;

            resolvedPagesBuff.InterlockedOr(byte4_addr, 1 << (byte_num * 8));

            debugTileID = loMipLevelTileID;
        }
    }


    // ---------------------------------
    psOut.debugColor = float4(0, 0, 0, 0);
    psOut.debugColor = randomTileColor(debugTileID);
    // ---------------------------------

    // ------- test depth tex --------
    //float depthTexVal  = gDepthTex.SampleLevel(gSampler, screenTexC, 0).r * 1000;
    //psOut.debugColor = float4(depthTexVal, 0, 0, 0);
    // -------------------------------

    return psOut;
}

/*

uint txw, txh;
        t.GetDimensions(txw, txh);

*/