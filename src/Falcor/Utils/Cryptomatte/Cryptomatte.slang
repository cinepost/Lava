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

bool cryptomatte_can_merge_sample(float2 dst, float2 src) {
  if (dst == float2(0.0, 0.0)) {
    return true;
  }
  if (dst.x == src.x) {
    return true;
  }
  return false;
}

float2 cryptomatte_merge_sample(float2 dst, float2 src) {
  return float2(src.x, dst.y + src.y);
}

void cryptomatte_clear_samples(FilmSample dst) {
  int layer_len = imageSize(cryptomatte_img).z;
  for (int i = 0; i < layer_len; i++) {
    imageStore(cryptomatte_img, int3(dst.texel, i), float44(0.0));
  }
}

void cryptomatte_store_film_sample(FilmSample dst, int cryptomatte_layer_id, float2 crypto_sample, out float4 out_color) {
  if (crypto_sample.y == 0.0) {
    return;
  }

  for (int i = 0; i < film_buf.cryptomatte_samples_len / 2; i++) {
    int3 img_co = int3(dst.texel, cryptomatte_layer_id + i);
    vec4 sample_pair = imageLoad(cryptomatte_img, img_co);
    if (cryptomatte_can_merge_sample(sample_pair.xy, crypto_sample)) {
      sample_pair.xy = cryptomatte_merge_sample(sample_pair.xy, crypto_sample);
      /* In viewport only one layer is active. */
      /* TODO(jbakker):  we are displaying the first sample, but we should display the highest
       * weighted one. */
      if (cryptomatte_layer_id + i == 0) {
        out_color = cryptomatte_false_color(sample_pair.x);
      }
    }
    else if (cryptomatte_can_merge_sample(sample_pair.zw, crypto_sample)) {
      sample_pair.zw = cryptomatte_merge_sample(sample_pair.zw, crypto_sample);
    }
    else if (i == film_buf.cryptomatte_samples_len / 2 - 1) {
      /* TODO(jbakker): New hash detected, but there is no space left to store it. Currently we
       * will ignore this sample, but ideally we could replace a sample with a lowest weight. */
      continue;
    }
    else {
      continue;
    }
    imageStore(cryptomatte_img, img_co, sample_pair);
    break;
  }
}