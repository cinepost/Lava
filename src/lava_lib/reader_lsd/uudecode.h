/*
 * txt2bin [input]
 *
 * decode the specified text file and recreate the original binary file 
 * encoded with encode.
 */
# include <stdlib.h>
# include <stdio.h>
# include <string.h>

namespace lava { 

namespace lsd {

namespace uu {
 
bool decodeUU ( FILE *in, FILE *out, bool checkCRC = false );


}  // namespace uu

}  // namespace lsd

}  // namespace lava
