#ifndef SRC_FALCOR_CORE_FALCORPLATFORM_H_
#define SRC_FALCOR_CORE_FALCORPLATFORM_H_

/**
 * Compilers.
 */
#define FALCOR_COMPILER_MSVC 1
#define FALCOR_COMPILER_CLANG 2
#define FALCOR_COMPILER_GCC 3

/**
 * Determine the compiler in use.
 * http://sourceforge.net/p/predef/wiki/Compilers/
 */
#ifndef FALCOR_COMPILER
#if defined(_MSC_VER)
#define FALCOR_COMPILER FALCOR_COMPILER_MSVC
#elif defined(__clang__)
#define FALCOR_COMPILER FALCOR_COMPILER_CLANG
#elif defined(__GNUC__)
#define FALCOR_COMPILER FALCOR_COMPILER_GCC
#else
#error "Unsupported compiler"
#endif
#endif // FALCOR_COMPILER

#define FALCOR_MSVC (FALCOR_COMPILER == FALCOR_COMPILER_MSVC)
#define FALCOR_CLANG (FALCOR_COMPILER == FALCOR_COMPILER_CLANG)
#define FALCOR_GCC (FALCOR_COMPILER == FALCOR_COMPILER_GCC)

/**
 * Platforms.
 */
#define FALCOR_PLATFORM_WINDOWS 1
#define FALCOR_PLATFORM_LINUX 2

/**
 * Determine the target platform in use.
 * http://sourceforge.net/p/predef/wiki/OperatingSystems/
 */
#ifndef FALCOR_PLATFORM
#if defined(_WIN64)
#define FALCOR_PLATFORM FALCOR_PLATFORM_WINDOWS
#elif defined(__linux__)
#define FALCOR_PLATFORM FALCOR_PLATFORM_LINUX
#else
#error "Unsupported target platform"
#endif
#endif // FALCOR_PLATFORM

#define FALCOR_WINDOWS (FALCOR_PLATFORM == FALCOR_PLATFORM_WINDOWS)
#define FALCOR_LINUX (FALCOR_PLATFORM == FALCOR_PLATFORM_LINUX)

#endif  // SRC_FALCOR_CORE_FALCORPLATFORM_H_