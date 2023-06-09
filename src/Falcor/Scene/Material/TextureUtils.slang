//#include "VirtualTextureData.slangh"
import Scene.Material.VirtualTextureData;


// Calculates texture page ID for a given mipLevel of a virtual texture at given tex_coord
int calculatePageID2D(float2 texCoord, int mipLevel, const VirtualTextureData vtexData) {

    int _mipLevel = max(0, mipLevel);

    //if( _mipLevel >= vtexData.mipTailStart) {
    //    return vtexData.mipBases[_mipLevel] + vtexData.pagesStartOffset;
    //}

    // Put texture coordinates in 0-1 range
    float2 tx = float2(texCoord.x - floor(texCoord.x), texCoord.y - floor(texCoord.y));

    // Now map them to texels
    tx = float2(((float)vtexData.width - 1) * (tx.x / (float)vtexData.width), (float)(vtexData.height - 1) * (tx.y / (float)vtexData.height));

    float currentMipTextureWidth = vtexData.width / pow(2, _mipLevel);
    float currentMipTextureHeight = vtexData.height / pow(2, _mipLevel);

    uint cellsNumX = uint(ceil(currentMipTextureWidth / (float)vtexData.pageSizeW)); // number of pages including partial last page on this mip level(_mipLevel)
    uint cellsNumY = uint(ceil(currentMipTextureHeight / (float)vtexData.pageSizeH)); // number of pages including partial last page on this mip level(_mipLevel)

    float k_u = currentMipTextureWidth / ((float)vtexData.pageSizeW * (float)cellsNumX); // texture coordinate renormalization coefficient (addresses partial tiles)
    float k_v = currentMipTextureHeight / ((float)vtexData.pageSizeH * (float)cellsNumY); // texture coordinate renormalization coefficient (addresses partial tiles)

    int tX = int(tx.x * k_u * cellsNumX); //column index in current level of detail
    int tY = int(tx.y * k_v * cellsNumY); //row index in current level of detail

    return vtexData.mipBases[_mipLevel] + tX + tY * cellsNumX + vtexData.pagesStartOffset;
}

void ellips(in float2x2 M, out float2 A0, out float2 A1, out float r0, out float r1) {
    float2 I = M[0], J = M[1];                      // ellipse: P = cos(u).I + sin(u).J 
    float a = ( dot(J,J) - dot(I,I) ) / dot (I,J),  // -> search for extrema of length(P) 
          D = sqrt(a*a+4.),                         // get tan(u) -> s, c, axis*radii
          t0 = (a+D)/2., t1 = (a-D)/2.;
    A0 = (I + t0*J) / sqrt(1.+t0*t0);               // c = 1/sqrt(1+t²), s = t/sqrt(1+t²)
    A1 = (I + t1*J) / sqrt(1.+t1*t1);               // axis also encodes radius
    r0 = length(A0); A0 /= r0;
    r1 = length(A1); A1 /= r1;
    if (r1>r0) a=r0,r0=r1,r1=a, I=A0,A0=A1,A1=I;    // sort eigenV
}

/*
float mip_map_level(in float2 dUVdx, dUVdy) {
    float delta_max_sqr = max(dot(dUVdx, dUVdx), dot(dUVdy, dUVdy));
    float mml = 0.5 * log2(delta_max_sqr);
    return max( 0, mml ); // Thanks @Nims
}

float4 sampleAnisotropicTexture2D(Texture2D t, float2 dUVdx, float2 dUVdy) {
    float2x2 J = float2x2(dUVdx, dUVdy);                  // pixel footprint in texture space
    float2 A,a; float M,m,level;
    ellips(J, A,a,M,m); 
    A *= M;
    level = log2( m * R.y );                        // MIPmap level corresponding to min radius
    if ((M/m)>16.) level = log2(M/16.*R.y);         // optional      
    float4 result = float4(0);

    // sample x16 along main axis at LOD min-radius
    for (float i = -7.5; i<8.; i++) {                
        result += t.SampleLevel(p+(i/16.)*A, level);
    }

    return result/16.;
}
*/

float4 sampleAnisotropicTexture2D(Texture2D t, SamplerState s, float2 uv, float2 dUVdx, float2 dUVdy) {
    float2x2 J = float2x2(dUVdx, dUVdy);                  // pixel footprint in texture space
    float2 A,a; float M,m;
    ellips(J, A,a,M,m); 
    A *= M;

    uint2 dim;
    t.GetDimensions(dim.x, dim.y);

    // Sharper, but alias sometimes for sharp edges textures.
    const float2 duvdx = dUVdx * dim.x;
    const float2 duvdy = dUVdy * dim.y;

    //const float level = 0.5f * log2(max(dot(duvdx, duvdx), dot(duvdy, duvdy)));

    const float filterWidth = 2.f * max(dim.x * max(abs(dUVdx.x), abs(dUVdy.x)), dim.y * max(abs(dUVdx.y), abs(dUVdy.y)));
    const float level = log2(filterWidth);

    //if ((M/m)>16.) level = log2(M/16.*R.y);         // optional      
    
    float4 result = float4(0);

    // sample x16 along main axis at LOD min-radius
    for (float i = -7.5; i<8.; i++) {                
        result += t.SampleLevel(s, uv+(i/16.)*A, level);
    }

    //return float4(level);

    return result/16.;
}