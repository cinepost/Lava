execute_process(COMMAND git log --pretty=format:'%h' -n 1
                OUTPUT_VARIABLE GIT_REV
                ERROR_QUIET)

# Check whether we got any revision (which isn't
# always the case, e.g. when someone downloaded a zip
# file from Github instead of a checkout)
if ("${GIT_REV}" STREQUAL "")
    set(GIT_REV "N/A")
    set(GIT_DIFF "")
    set(GIT_TAG "N/A")
    set(GIT_BRANCH "N/A")
else()
    execute_process(
        COMMAND bash -c "git diff --quiet --exit-code || echo +"
        OUTPUT_VARIABLE GIT_DIFF)
    execute_process(
        COMMAND git describe --exact-match --tags
        OUTPUT_VARIABLE GIT_TAG ERROR_QUIET)
    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE GIT_BRANCH)

    string(STRIP "${GIT_REV}" GIT_REV)
    string(SUBSTRING "${GIT_REV}" 1 7 GIT_REV)
    string(STRIP "${GIT_DIFF}" GIT_DIFF)
    string(STRIP "${GIT_TAG}" GIT_TAG)
    string(STRIP "${GIT_BRANCH}" GIT_BRANCH)
endif()

set(CPP_VERSION "extern \"C\" {
    extern const char* LAVA_GIT_REV=\"${GIT_REV}${GIT_DIFF}\";
    extern const char* LAVA_GIT_TAG=\"${GIT_TAG}\";
    extern const char* LAVA_GIT_BRANCH=\"${GIT_BRANCH}\";
}")

if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/src/version.cpp)
    file(READ ${CMAKE_CURRENT_BINARY_DIR}/src/version.cpp CPP_VERSION_)
else()
    set(CPP_VERSION_ "")
endif()

if (NOT "${CPP_VERSION}" STREQUAL "${CPP_VERSION_}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/src/version.cpp "${CPP_VERSION}")
endif()