//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#ifndef SRC_FALCOR_UTILS_CRYPTOMATTE_MURMURHASH_H_
#define SRC_FALCOR_UTILS_CRYPTOMATTE_MURMURHASH_H_

#include <stdint.h>
#include <algorithm>

#include "Falcor/Utils/Math/Vector.h"

namespace Falcor {

namespace {

	#if defined(_MSC_VER)
	#define FORCE_INLINE  __forceinline
	#define ROTL32(x, y) _rotl(x, y)
	#define ROTL64(x, y) _rotl64(x, y)
	#define BIG_CONSTANT(x) (x)
	#else
	#define FORCE_INLINE inline __attribute__((always_inline))

	inline uint32_t rotl32(uint32_t x, int8_t r) {
	  return (x << r) | (x >> (32 - r));
	}

	#define ROTL32(x, y) rotl32(x, y)
	#define BIG_CONSTANT(x) (x##LLU)
	#endif

	#if defined(__GNUC__) && __GNUC__ >= 7
	 #define ATTR_FALLTHROUGH __attribute__ ((fallthrough))
	#else
	 #define ATTR_FALLTHROUGH ((void)0)
	#endif /* __GNUC__ >= 7 */

	/* Block read - if your platform needs to do endian-swapping or can only
	 * handle aligned reads, do the conversion here. */
	FORCE_INLINE uint32_t mm_hash_getblock32(const uint32_t *p, int i) {
	  return p[i];
	}

	/* Finalization mix - force all bits of a hash block to avalanche */
	FORCE_INLINE uint32_t mm_hash_fmix32(uint32_t h) {
	  h ^= h >> 16;
	  h *= 0x85ebca6b;
	  h ^= h >> 13;
	  h *= 0xc2b2ae35;
	  h ^= h >> 16;
	  return h;
	}

  constexpr uint32_t _UINT32_MAX = std::numeric_limits<uint32_t>::max();

}  // namespace

inline uint32_t util_murmur_hash3(const void *key, int len, uint32_t seed = 0u) {
	const uint8_t *data = (const uint8_t *)key;
  const int nblocks = len / 4;

  uint32_t h1 = seed;

  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  const uint32_t *blocks = (const uint32_t *)(data + nblocks * 4);

  for (int i = -nblocks; i; i++) {
    uint32_t k1 = mm_hash_getblock32(blocks, i);

    k1 *= c1;
    k1 = ROTL32(k1, 15);
    k1 *= c2;

    h1 ^= k1;
    h1 = ROTL32(h1, 13);
    h1 = h1 * 5 + 0xe6546b64;
  }

  const uint8_t *tail = (const uint8_t *)(data + nblocks * 4);

  uint32_t k1 = 0;

  switch (len & 3) {
    case 3:
      k1 ^= tail[2] << 16;
      ATTR_FALLTHROUGH;
    case 2:
      k1 ^= tail[1] << 8;
      ATTR_FALLTHROUGH;
    case 1:
      k1 ^= tail[0];
      k1 *= c1;
      k1 = ROTL32(k1, 15);
      k1 *= c2;
      h1 ^= k1;
  }

  h1 ^= len;
  h1 = mm_hash_fmix32(h1);
  return h1;
}

inline float util_hash_to_float(uint32_t hash) {
	// if all exponent bits are 0 (subnormals, +zero, -zero) set exponent to 1
	// if all exponent bits are 1 (NaNs, +inf, -inf) set exponent to 254
	uint32_t exponent = hash >> 23 & 255; // extract exponent (8 bits)
	
	if (exponent == 0 || exponent == 255) {
	  hash ^= 1 << 23; // toggle bit
	}
	
	float f;
	std::memcpy(&f, &hash, 4);
	return f;
}

inline float3 util_hash_to_rgb(uint32_t hash = 0u) {
	float r = util_hash_to_float(hash);
  float g = ((float) ((hash << 8)) / (float) _UINT32_MAX);
  float b = ((float) ((hash << 16)) / (float) _UINT32_MAX);
  return {r, g, b};
}

inline float3 util_hash_name_to_rgb(const unsigned char *name) {
	size_t str_len = 0;
  while (name[str_len] != '\0') str_len++;
  uint32_t m3hash = util_murmur_hash3(static_cast<const void *>(name), str_len, 0);
  return util_hash_to_rgb(m3hash);
}

inline std::string hash_float_to_hexidecimal(float hash) {
  uint32_t float_bits;
  char hex_chars[9];
  std::memcpy(&float_bits, &hash, 4);
  sprintf(hex_chars, "%08x", float_bits);
  return std::string(hex_chars);
}

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_CRYPTOMATTE_MURMURHASH_H_