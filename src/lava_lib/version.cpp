#include "version.h"

extern "C" {
    extern const char* LAVA_GIT_TAG;
    extern const char* LAVA_GIT_REV;
    extern const char* LAVA_GIT_BRANCH;
}

namespace lava {

const char* liblava_git_version(void) {
    return LAVA_GIT_TAG;
}

const char* liblava_git_revision(void) {
    return LAVA_GIT_REV;
}

const char* liblava_git_branch(void) {
    return LAVA_GIT_BRANCH;
}

std::string versionString(void) {
    return "v" + std::string(liblava_git_version()) + "." + std::string(liblava_git_revision()) + "." + std::string(liblava_git_branch());
}

} // namespace lava
