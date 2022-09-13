#ifndef LAVA_LIB_VERSION_H_
#define LAVA_LIB_VERSION_H_

#include <string>

namespace lava {

const char* liblib_git_version(void);
const char* liblib_git_revision(void);
const char* liblib_git_branch(void);

std::string versionString(void);

} // namespace lava

#endif // LAVA_LIB_VERSION_H_