#include "Utils/Math/MathConstants.slangh"


/*
    Convert input to workspace colorspace.
*/
float3 applyIDT(float3 Image) {

    if (INPUT_COLORSPACE == 0) Image = cctf_decoding_sRGB(Image);
    if (INPUT_COLORSPACE == 1) Image = cctf_decoding_pow2_2(Image);

    float ImageLuma = powsafe(getLuminance(Image), INPUT_HIGHLIGHT_GAIN_GAMMA);
    Image += Image * ImageLuma.xxx * INPUT_HIGHLIGHT_GAIN;

    Image = saturation(Image, INPUT_SATURATION);
    Image = powsafe(Image, INPUT_GAMMA);
    Image *= powsafe(2.0, INPUT_EXPOSURE);
    return Image;
}


/*
    Prepare the data for display encoding. Converted to log domain.
*/
float3 applyAgXLog(float3 Image) {
    Image = max(0.0, Image); // clamp negatives
    Image = mul(agx_compressed_matrix, Image);

    if (DEBUG_A)
        Image = log2Transform(Image);
    else
        Image = convertOpenDomainToNormalizedLog2(Image, -10.0, 6.5);

    Image = clamp(Image, 0.0, 1.0);
    return Image;
}


/*
    Apply the AgX 1D curve on log encoded data.

    The output is similar to AgX Base which is considered
    sRGB - Display, but here we linearize it.

    -- ref[3] for LUT implementation
*/
float3 applyAgXLUT(float3 c) {

    float3 lut3D = c*(LUT_BLOCK_SIZE-1);

    float2 lut2D[2];
    // Front
    lut2D[0].x = floor(lut3D.z)*LUT_BLOCK_SIZE+lut3D.x;
    lut2D[0].y = lut3D.y;
    // Back
    lut2D[1].x = ceil(lut3D.z)*LUT_BLOCK_SIZE+lut3D.x;
    lut2D[1].y = lut3D.y;

    // Convert from texel to texture coords
    lut2D[0] = (lut2D[0]+0.5)*LUT_PIXEL_SIZE;
    lut2D[1] = (lut2D[1]+0.5)*LUT_PIXEL_SIZE;

    // Bicubic LUT interpolation
    c = lerp(
        tex2D(LUTSampler, lut2D[0]).rgb, // Front Z
        tex2D(LUTSampler, lut2D[1]).rgb, // Back Z
        frac(lut3D.z)
    );
    // LUT apply the transfer function so we remove it to keep working on linear data.
    c = cctf_decoding_pow2_2(c);
    return c;
}


/*
    Outset is the inverse of the inset applied during `applyAgXLog` 
    and restore chroma.
*/
float3 applyOutset(float3 c) {
    c = mul(agx_compressed_matrix_inverse, c);
    return c;
}


/*
   Apply Agx to display conversion.
   For now hardcoded to sRGB display.
   :param color: linear - sRGB data.
*/
float3 applyODT(float3 c) {
    c = cctf_encoding_pow2_2(c);
    return c;
}


/*
    Applies the post "Punchy" look to display-encoded data.
    Input is expected to be in a display-state.
*/
float3 applyLookPunchy(float3 c) {
    c = powsafe(c, PUNCH_GAMMA);
    c = saturation(c, PUNCH_SATURATION);
    c *= powsafe(2.0, PUNCH_EXPOSURE);  // not part of initial cdl
    return c;
}