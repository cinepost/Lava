#ifndef LAVA_UTILS_UT_FSYS_H_
#define LAVA_UTILS_UT_FSYS_H_

#include <string>

namespace lava { namespace ut { namespace fsys {

/*
 * Get File extension from File path or File Name
 */
std::string getFileExtension(std::string file_path);

}}} // namespace lava::ut::fsys

#endif // LAVA_UTILS_UT_FSYS_H_