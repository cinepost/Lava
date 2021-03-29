#include "TexturesResolveData.slangh"

// Generic helpers

uint divMod(int x, int y) {
    return x - y * floor(float(x) / float(y));
};

// Virtual texturing debug helpers

float4 randomColor(int tileID) {
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

float calculateBaseMipMapLevel2D(float2 texCoord, float2 texDims, float mipBias) {
    // Modified The OpenGL Graphics System: A Specification 4.2 - chapter 3.9.11, equation 3.21

    // bias scale
    float bias = pow(2, mipBias);

    float2 dx_vtc = ddx(texCoord * texDims) * bias; // texDims.x;
    float2 dy_vtc = ddy(texCoord * texDims) * bias; // texDims.y;
    float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));

    return max(0.0, 0.5 * log2(delta_max_sqr));
}

float calculateAnisoMipMapLevel2D(float2 texCoord, float2 texDims, float mipBias) {
    // bias scale
    float bias = pow(2, mipBias);

    float2 tc = texCoord * texDims;

    float2 dx_vtc = ddx_fine(tc) * bias; // * texDims.x;
    float2 dy_vtc = ddy_fine(tc) * bias; // * texDims.y;
    
    float p_x = sqrt(dot(dx_vtc, dx_vtc));
    float p_y = sqrt(dot(dy_vtc, dy_vtc));

    float p_min = min(p_x, p_y);
    float p_max = max(p_x, p_y);

    float max_anisotropy = 16;
    float n = min(p_max/p_min, max_anisotropy);

    float p = p_max / n;
    p = p_min * n;

    return max(0.0, log2(p));
}

float _calculateAnisoMipMapLevel2D(float2 texCoord, float2 texDims, float mipBias) {
    // bias scale
    float bias = pow(2, mipBias);

    float2 tc = texCoord * texDims;

    float2 dx_vtc = ddx_fine(tc) * bias; // * texDims.x;
    float2 dy_vtc = ddy_fine(tc) * bias; // * texDims.y;
    
    float p_x = sqrt(dot(dx_vtc, dx_vtc));
    float p_y = sqrt(dot(dy_vtc, dy_vtc));

    float p_min = min(p_x, p_y);
    float p_max = max(p_x, p_y);

    float max_anisotropy = 16;
    float n = min(p_max/p_min, max_anisotropy);

    return p_max - p_min;

    float p = p_max / n;
    p = p_min * n;

    return max(0.0, log2(p));
}


float2 calculateAnisoMipMapLevel2D2(float2 texCoord, float2 texDims, float mipBias) {
    // bias scale
    float bias = pow(2, mipBias);

    float2 dx_vtc = ddx_fine(texCoord) * bias * texDims.x;
    float2 dy_vtc = ddy_fine(texCoord) * bias * texDims.y;
    
    float p_x = sqrt(dot(dx_vtc, dx_vtc));
    float p_y = sqrt(dot(dy_vtc, dy_vtc));

    float p_min = min(p_x, p_y);
    float p_max = max(p_x, p_y);

    float max_anisotropy = 16;
    float n = min(p_max/p_min, max_anisotropy);

    float p1 = p_max / n;
    float p2 = p_min / n;

    return float2(max(0.0, log2(p1)), max(0.0, log2(p2)));
}


// computes tile ID for a given mipLevel of a virtual texture at given tex_coord
int calculateTileID2D(float2 texCoord, int mipLevel, VirtualTextureData vtexData) {

    // all mip levels greater or equal to mipTailStart stored in last page block
    if( mipLevel >= vtexData.mipTailStart) {
        return 0; // safety measure
        //return vtexData.mipBases[vtexData.mipTailStart] + (mipLevel - vtexData.mipTailStart);
    }

    float currentMipTextureWidth = vtexData.width / pow(2, mipLevel);
    float currentMipTextureHeight = vtexData.height / pow(2, mipLevel);

    uint cellsNumX = uint(ceil(currentMipTextureWidth / vtexData.pageSizeW)); // number of pages including partial last page on this mip level(mipLevel)
    uint cellsNumY = uint(ceil(currentMipTextureHeight / vtexData.pageSizeH)); // number of pages including partial last page on this mip level(mipLevel)

    float k_u = currentMipTextureWidth / (vtexData.pageSizeW * cellsNumX); // texture coordinate renormalization coefficient (addresses partial tiles)
    float k_v = currentMipTextureHeight / (vtexData.pageSizeH * cellsNumY); // texture coordinate renormalization coefficient (addresses partial tiles)

    int tX = int(texCoord.x * k_u * cellsNumX); //column index in current level of detail
    int tY = int(texCoord.y * k_v * cellsNumY); //row index in current level of detail

    return vtexData.mipBases[mipLevel] + tX + tY * cellsNumX + vtexData.pagesStartOffset;
}

// computes anisotropy factor. 0 - no anisotropy, 1 - maximum anisotropy
float _calculateAnisotropyFactor(float2 texCoord, float2 texDims) {
    float2 dx_vtc = ddx(texCoord) * texDims.x;
    float2 dy_vtc = ddy(texCoord) * texDims.y;

    float dx = sqrt(dot(dx_vtc, dx_vtc));
    float dy = sqrt(dot(dy_vtc, dy_vtc));

    return 1.0 - min(dx, dy) / max(dx, dy);
}

// compute number of additional lower mip levels required for anisotropic filtering
int calculateAdditionalAnisotropicMipLevel(float2 texCoord, float2 texDims, int currMipLevel) {
    float _af = _calculateAnisotropyFactor(texCoord, texDims);
    if(_af < 0.001) {
        return 0;
    }

    int mipShift = floor(log2(1.0 / _af));

    return max(0, currMipLevel - mipShift);  
}