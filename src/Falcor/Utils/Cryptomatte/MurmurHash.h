//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#ifndef SRC_FALCOR_UTILS_CRYPTOMATTE_MURMURHASH_H_
#define SRC_FALCOR_UTILS_CRYPTOMATTE_MURMURHASH_H_

#include <stdint.h>

namespace Falcor {

uint32_t util_murmur_hash3(const void *key, int len, uint32_t seed);
float util_hash_to_float(uint32_t hash);

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_CRYPTOMATTE_MURMURHASH_H_