#ifndef BGEO_PARSER_TYPES_H
#define BGEO_PARSER_TYPES_H

#include <vector>

#ifndef __SYS_Types__

/*
 * Get rid of houdini headers inclusion outside of bgeo reading lib
 */

namespace ika {
namespace bgeo {
namespace parser {

/*
 * Integer types
 */
typedef int             int32;
typedef unsigned int    uint32;

#if defined(WIN32)
    typedef __int64             int64;
    typedef unsigned __int64    uint64;
#elif defined(MBSD)
    // On MBSD, int64/uint64 are also defined in the system headers so we must
    // declare these in the same way or else we get conflicts.
    #include <stdint.h>
    typedef int64_t             int64;
    typedef uint64_t            uint64;
#elif defined(AMD64)
    typedef long                int64;
    typedef unsigned long       uint64;
#else
    typedef long long           int64;
    typedef unsigned long long  uint64;
#endif

} // namespace parser
} // namespace bgeo
} // namespace ika

#endif // houdini __SYS_Types__

namespace ika {
namespace bgeo {
namespace parser {

// ika::bgeo part
typedef std::vector<int32> VertexArray;

} // namespace parser
} // namespace bgeo
} // namespace ika

#endif // BGEO_PARSER_UTIL_H
