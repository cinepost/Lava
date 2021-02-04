#ifndef SRC_UTILS_MATH_COMMON_H_
#define SRC_UTILS_MATH_COMMON_H_

#include <stdint.h>

namespace Falcor {

inline bool isPowerOfTwo(uint32_t n) {
    return (n > 0 && ((n & (n - 1)) == 0)) ? true : false;
};

}  // namespace Falcor

#endif  // SRC_