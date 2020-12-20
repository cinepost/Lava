#include "TexturesResolveData.slangh"

// Generic helpers

uint divMod(int x, int y) {
    return x - y * floor(float(x) / float(y));
};

// Virtual texturing debug helpers

float4 randomTileColor(int tileID) {
    const float4 dbgColors[17] = {
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
        {0.5, 0.5, 0.5, 1},
        {0.25,0.75,0, 1}
    };

    if (tileID < 0) return float4(0, 0, 0, 0);

    return dbgColors[divMod(tileID, 17)];

}

// Virtual textureing resolve pass helper functions

// computes mip level at texCoord for a tecture with texDims
// the current pixel
float calculateMipMapLevel2D(float2 texCoord, float2 texDims) {
    // The OpenGL Graphics System: A Specification 4.2
    //  - chapter 3.9.11, equation 3.21

    float2 dx_vtc = ddx(texCoord) * texDims.x;
    float2 dy_vtc = ddy(texCoord) * texDims.y;
    float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));

    return max(0.0, 0.5 * log2(delta_max_sqr));
}

// computes tile ID for a given mipLevel of a virtual texture at given tex_coord
int calculateTileID2D(float2 texCoord, int mipLevel, VirtualTextureData vtexData) {

    // all mip levels greater or equal to mipTailStart stored in last page block
    if( mipLevel >= vtexData.mipTailStart) {
        return vtexData.mipBases[vtexData.mipTailStart];
    }

    float currentMipTextureWidth = vtexData.width / pow(2, mipLevel);
    float currentMipTextureHeight = vtexData.height / pow(2, mipLevel);

    uint cellsNumX = uint(ceil(currentMipTextureWidth / vtexData.pageSizeW)); // number of pages including partial last page on this mip level(mipLevel)
    uint cellsNumY = uint(ceil(currentMipTextureHeight / vtexData.pageSizeH)); // number of pages including partial last page on this mip level(mipLevel)

    float k_u = currentMipTextureWidth / (vtexData.pageSizeW * cellsNumX); // texture coordinate renormalization coefficient (addresses partial tiles)
    float k_v = currentMipTextureHeight / (vtexData.pageSizeH * cellsNumY); // texture coordinate renormalization coefficient (addresses partial tiles)

    int tX = int(texCoord.x * k_u * cellsNumX); //column index in current level of detail
    int tY = int(texCoord.y * k_v * cellsNumY); //row index in current level of detail

    return vtexData.mipBases[mipLevel] + tX + tY * cellsNumX;
}