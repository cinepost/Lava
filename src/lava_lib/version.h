#ifndef LAVA_LIB_VERSION_H_
#define LAVA_LIB_VERSION_H_

#include <string>

#include "lava_dll.h"

namespace lava {

const char* liblib_git_version(void);
const char* liblib_git_revision(void);
const char* liblib_git_branch(void);

std::string LAVA_API versionString(void);

} // namespace lava

#endif // LAVA_LIB_VERSION_H_