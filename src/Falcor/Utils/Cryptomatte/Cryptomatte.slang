float32_t m3hashToFloat32(uint32_t hash) {
  uint32_t exponent = hash >> 23 & 255;
  if( exponent == 0 || exponent == 255)
    hash ^= 1 << 23;

  return asfloat(hash);
}

/** Convert hash to 32-bit human readable RGB.
    \return 8-bit snorm value in low bits, high bits are all zeros or ones depending on sign.
*/
vector<float32_t, 3> m3hashToRGB32F(uint32_t hash) {
    vector<float32_t, 3> result;

    result.r = m3hashToFloat32(hash);
    result.g = ((float) ((result.r << 8)) / (float) UINT32_MAX);
    result.b = ((float) ((result.r << 16)) / (float) UINT32_MAX);

    return result;
}
