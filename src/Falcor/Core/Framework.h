/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#ifndef SRC_FALCOR_CORE_FRAMEWORK_H_
#define SRC_FALCOR_CORE_FRAMEWORK_H_

#include "FalcorPlatform.h"
#include "Enum.h"

#include <fstd/source_location.h> // TODO C++20: Replace with <source_location>

#define FMT_HEADER_ONLY
//#include <fmt/format.h>
#include <OpenImageIO/detail/fmt/format.h>

#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;

#if FALCOR_GCC
// save compiler switches
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING

#include "Falcor/Core/Macros.h"
#include "Falcor/Core/ErrorHandling.h"

#include <stdint.h>
#include <iostream>
#include <locale>
#include <codecvt>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <utility>
#include <memory>
#include <type_traits>


#include "boost/format.hpp"
#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;

#include "Falcor/Core/FalcorConfig.h"
#include "Falcor/Utils/Math/Vector.h"
#include "lava_utils_lib/logging.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef arraysize
#define arraysize(a) (sizeof(a)/sizeof(a[0]))
#endif
#ifndef offsetof
#define offsetof(s, m) (size_t)( (ptrdiff_t)&reinterpret_cast<const volatile char&>((((s *)0)->m)) )
#endif

#ifdef assert
#undef assert
#endif

#ifndef __FUNCTION_NAME__
    #ifdef WIN32   //WINDOWS
        #define __FUNCTION_NAME__   __FUNCTION__  
    #else          //*NIX
        #define __FUNCTION_NAME__   __func__ 
    #endif
#endif

#ifdef _DEBUG
#define assert(a) \
    if (!(a)) { \
        std::string str = "assertion failed(" + std::string(#a) + ")\nFile " + __FILE__ + ", line " + std::to_string(__LINE__);\
        LLOG_FTL << str;\
        std::abort();\
    }

#define should_not_get_here() assert(false);

#else  // _DEBUG

#ifdef _AUTOTESTING
#define assert(a) if (!(a)) throw std::runtime_error("Assertion Failure");
#else  // _AUTOTESTING
#define assert(a) ((void)(a))
#endif  // _AUTOTESTING

#ifdef _MSC_VER
#define should_not_get_here() __assume(0)
#else  // _MSC_VER
#define should_not_get_here() __builtin_unreachable()
#endif  // _MSC_VER

#endif  // _DEBUG

#ifdef _DEBUG

#define FALCOR_ASSERT(a)\
    if (!(a)) {\
        std::string s = boost::str(boost::format("assertion failed( %1% )\n%2%(%3%)") % #a % __FILE__ % __LINE__); \
        Falcor::reportFatalError(s);\
    }
#define FALCOR_ASSERT_MSG(a, msg)\
    if (!(a)) {\
        std::string s = boost::str(boost::format("assertion failed( %1% ): %2%\n%3%(%4%)") % #a % msg % __FILE__ % __LINE__); \
        Falcor::reportFatalError(s); \
    }
#define FALCOR_ASSERT_OP(a, b, OP)\
    if (!(a OP b)) {\
        std::string s = boost::str(boost::format("assertion failed( %1% %2% %3% )\n%4%(%5%)") % #a % #OP % #b % __FILE__ % __LINE__); \
        Falcor::reportFatalError(s); \
    }


#define FALCOR_ASSERT_EQ(a, b) FALCOR_ASSERT_OP(a, b, == )
#define FALCOR_ASSERT_NE(a, b) FALCOR_ASSERT_OP(a, b, != )
#define FALCOR_ASSERT_GE(a, b) FALCOR_ASSERT_OP(a, b, >= )
#define FALCOR_ASSERT_GT(a, b) FALCOR_ASSERT_OP(a, b, > )
#define FALCOR_ASSERT_LE(a, b) FALCOR_ASSERT_OP(a, b, <= )
#define FALCOR_ASSERT_LT(a, b) FALCOR_ASSERT_OP(a, b, < )


#else // _DEBUG

#define FALCOR_ASSERT(a) {}
#define FALCOR_ASSERT_MSG(a, msg) {}
#define FALCOR_ASSERT_OP(a, b, OP) {}
#define FALCOR_ASSERT_EQ(a, b) {}
#define FALCOR_ASSERT_NE(a, b) {}
#define FALCOR_ASSERT_GE(a, b) {}
#define FALCOR_ASSERT_GT(a, b) {}
#define FALCOR_ASSERT_LE(a, b) {}
#define FALCOR_ASSERT_LT(a, b) {}

#endif // _DEBUG

#define FALCOR_UNREACHABLE() assert(false)

#define safe_delete(_a) {delete _a; _a = nullptr;}
#define safe_delete_array(_a) {delete[] _a; _a = nullptr;}
#define stringize(a) #a
#define concat_strings_(a, b) a##b
#define concat_strings(a, b) concat_strings_(a, b)

namespace Falcor {

#define enum_class_operators(e_) \
    inline e_ operator& (e_ a, e_ b) { return static_cast<e_>(static_cast<int>(a)& static_cast<int>(b)); } \
    inline e_ operator| (e_ a, e_ b) { return static_cast<e_>(static_cast<int>(a)| static_cast<int>(b)); } \
    inline e_& operator|= (e_& a, e_ b) { a = a | b; return a; } \
    inline e_& operator&= (e_& a, e_ b) { a = a & b; return a; } \
    inline e_  operator~ (e_ a) { return static_cast<e_>(~static_cast<int>(a)); } \
    inline bool is_set(e_ val, e_ flag) { return (val & flag) != static_cast<e_>(0); } \
    inline void flip_bit(e_& val, e_ flag) { val = is_set(val, flag) ? (val & (~flag)) : (val | flag); }

/*!
*  \addtogroup Falcor
*  @{
*/

enum class ShaderModel : uint32_t {
    Unknown = 0,
    SM6_0 = 60,
    SM6_1 = 61,
    SM6_2 = 62,
    SM6_3 = 63,
    SM6_4 = 64,
    SM6_5 = 65,
    SM6_6 = 66,
    SM6_7 = 67,
};
FALCOR_ENUM_INFO(
    ShaderModel, {
        {ShaderModel::Unknown, "Unknown"},
        {ShaderModel::SM6_0, "SM6_0"},
        {ShaderModel::SM6_1, "SM6_1"},
        {ShaderModel::SM6_2, "SM6_2"},
        {ShaderModel::SM6_3, "SM6_3"},
        {ShaderModel::SM6_4, "SM6_4"},
        {ShaderModel::SM6_5, "SM6_5"},
        {ShaderModel::SM6_6, "SM6_6"},
        {ShaderModel::SM6_7, "SM6_7"},
    }
);
FALCOR_ENUM_REGISTER(ShaderModel);

inline std::string to_string(ShaderModel sm) {
    return enumToString(sm);
}

inline uint32_t getShaderModelMajorVersion(ShaderModel sm)
{
    return uint32_t(sm) / 10;
}
inline uint32_t getShaderModelMinorVersion(ShaderModel sm)
{
    return uint32_t(sm) % 10;
}

/**
 * Falcor shader types
 */
enum class ShaderType {
    Vertex,        ///< Vertex shader
    Pixel,         ///< Pixel shader
    Geometry,      ///< Geometry shader
    Hull,          ///< Hull shader (AKA Tessellation control shader)
    Domain,        ///< Domain shader (AKA Tessellation evaluation shader)
    Compute,       ///< Compute shader
    RayGeneration, ///< Ray generation shader
    Intersection,  ///< Intersection shader
    AnyHit,        ///< Any hit shader
    ClosestHit,    ///< Closest hit shader
    Miss,          ///< Miss shader
    Callable,      ///< Callable shader
    Count          ///< Shader Type count
};
FALCOR_ENUM_INFO(
    ShaderType, {
        {ShaderType::Vertex, "Vertex"},
        {ShaderType::Pixel, "Pixel"},
        {ShaderType::Geometry, "Geometry"},
        {ShaderType::Hull, "Hull"},
        {ShaderType::Domain, "Domain"},
        {ShaderType::Compute, "Compute"},
        {ShaderType::RayGeneration, "RayGeneration"},
        {ShaderType::Intersection, "Intersection"},
        {ShaderType::AnyHit, "AnyHit"},
        {ShaderType::ClosestHit, "ClosestHit"},
        {ShaderType::Miss, "Miss"},
        {ShaderType::Callable, "Callable"},
    }
);
FALCOR_ENUM_REGISTER(ShaderType);

inline const std::string& to_string(const ShaderType& st) {
    return enumToString(st);
}

/** Shading languages. Used for shader cross-compilation.
*/
enum class ShadingLanguage {
    Unknown,        ///< Unknown language (e.g., for a plain .h file)
    GLSL,           ///< OpenGL Shading Language (GLSL)
    VulkanGLSL,     ///< GLSL for Vulkan
    HLSL,           ///< High-Level Shading Language
    Slang,          ///< Slang shading language
};

/** Framebuffer target flags. Used for clears and copy operations
*/
enum class FboAttachmentType {
    None    = 0,    ///< Nothing. Here just for completeness
    Color   = 1,    ///< Operate on the color buffer.
    Depth   = 2,    ///< Operate on the the depth buffer.
    Stencil = 4,    ///< Operate on the the stencil buffer.

    All = Color | Depth | Stencil  ///< Operate on all targets
};

enum_class_operators(FboAttachmentType);

enum class DataType {
    int8,
    int16,
    int32,
    int64,
    uint8,
    uint16,
    uint32,
    uint64,
    float16,
    float32,
    float64,
};
FALCOR_ENUM_INFO(
    DataType, {
        {DataType::int8, "int8"},
        {DataType::int16, "int16"},
        {DataType::int32, "int32"},
        {DataType::int64, "int64"},
        {DataType::uint8, "uint8"},
        {DataType::uint16, "uint16"},
        {DataType::uint32, "uint32"},
        {DataType::uint64, "uint64"},
        {DataType::float16, "float16"},
        {DataType::float32, "float32"},
        {DataType::float64, "float64"},
    }
);
FALCOR_ENUM_REGISTER(DataType);

enum class ComparisonFunc {
    Disabled,     ///< Comparison is disabled
    Never,        ///< Comparison always fails
    Always,       ///< Comparison always succeeds
    Less,         ///< Passes if source is less than the destination
    Equal,        ///< Passes if source is equal to the destination
    NotEqual,     ///< Passes if source is not equal to the destination
    LessEqual,    ///< Passes if source is less than or equal to the destination
    Greater,      ///< Passes if source is greater than to the destination
    GreaterEqual, ///< Passes if source is greater than or equal to the destination
};

FALCOR_ENUM_INFO(
    ComparisonFunc, {
        {ComparisonFunc::Disabled, "Disabled"},
        {ComparisonFunc::Never, "Never"},
        {ComparisonFunc::Always, "Always"},
        {ComparisonFunc::Less, "Less"},
        {ComparisonFunc::Equal, "Equal"},
        {ComparisonFunc::NotEqual, "NotEqual"},
        {ComparisonFunc::LessEqual, "LessEqual"},
        {ComparisonFunc::Greater, "Greater"},
        {ComparisonFunc::GreaterEqual, "GreaterEqual"},
    }
);
FALCOR_ENUM_REGISTER(ComparisonFunc);

inline std::string to_string(const ComparisonFunc& f) {
   return enumToString(f);
}

/** Flags indicating what hot-reloadable resources have changed
*/
enum class HotReloadFlags {
    None    = 0,    ///< Nothing. Here just for completeness
    Program = 1,    ///< Programs (shaders)
};

enum_class_operators(HotReloadFlags);

/** Clamps a value within a range.
    \param[in] val Value to clamp
    \param[in] minVal Low end to clamp to
    \param[in] maxVal High end to clamp to
    \return Result
*/
template<typename T>
inline T clamp(const T& val, const T& minVal, const T& maxVal) {
    return std::min(std::max(val, minVal), maxVal);
}

/** Returns whether an integer number is a power of two.
*/
template<typename T>
inline typename std::enable_if<std::is_integral<T>::value, bool>::type isPowerOf2(T a) {
    return (a & (a - (T)1)) == 0;
}

template <typename T>
inline T div_round_up(T a, T b) { return (a + b - (T)1) / b; }

#define align_to(_alignment, _val) ((((_val) + (_alignment) - 1) / (_alignment)) * (_alignment))

/** Helper class to check if a class has a vtable.
    Usage: has_vtable<MyClass>::value is true if vtable exists, false otherwise.
*/
template<class T>
struct has_vtable {
    class derived : public T {
        virtual void force_the_vtable() {}
    };
    enum { value = (sizeof(T) == sizeof(derived)) };
};

/*! @} */


// This is a helper class which should be used in case a class derives from a base class which derives from enable_shared_from_this
// If Derived will also inherit enable_shared_from_this, it will cause multiple inheritance from enable_shared_from_this, which results in a runtime errors because we have 2 copies of the WeakPtr inside shared_ptr
template<typename Base, typename Derived>
class inherit_shared_from_this {
public:
    typename std::shared_ptr<Derived> shared_from_this() {
        Base* pBase = static_cast<Derived*>(this);
        std::shared_ptr<Base> pShared = pBase->shared_from_this();
        return std::static_pointer_cast<Derived>(pShared);
    }

    typename std::shared_ptr<const Derived> shared_from_this() const {
        const Base* pBase = static_cast<const Derived*>(this);
        std::shared_ptr<const Base> pShared = pBase->shared_from_this();
        return std::static_pointer_cast<const Derived>(pShared);
    }
};

}  // namespace Falcor

namespace Falcor {

//
// Exceptions.
//

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4275) // allow dllexport on classes dervied from STL
#endif

/**
 * Base class for all Falcor exceptions.
 */
class FALCOR_API Exception : public std::exception
{
public:
    Exception() noexcept {}
    Exception(std::string_view what) : mpWhat(std::make_shared<std::string>(what)) {}
    Exception(const Exception& other) noexcept { mpWhat = other.mpWhat; }
    virtual ~Exception() override {}
    virtual const char* what() const noexcept override { return mpWhat ? mpWhat->c_str() : ""; }

protected:
    // Message is stored as a reference counted string in order to allow copy constructor to be noexcept.
    std::shared_ptr<std::string> mpWhat;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

/**
 * Exception to be thrown when an error happens at runtime.
 */
class FALCOR_API RuntimeError : public Exception
{
public:
    RuntimeError() noexcept {}
    RuntimeError(std::string_view what) : Exception(what) {}
    RuntimeError(const RuntimeError& other) noexcept: Exception() { mpWhat = other.mpWhat; }
    virtual ~RuntimeError() override {}
};

/**
 * Exception to be thrown on FALCOR_ASSERT.
 */
class FALCOR_API AssertionError : public Exception
{
public:
    AssertionError() noexcept {}
    AssertionError(std::string_view what) : Exception(what) {}
    AssertionError(const AssertionError& other) noexcept: Exception() { mpWhat = other.mpWhat; }
    virtual ~AssertionError() override {}
};


//
// Exception helpers.
//

/// Throw a RuntimeError exception.
/// If ErrorDiagnosticFlags::AppendStackTrace is set, a stack trace will be appended to the exception message.
/// If ErrorDiagnosticFlags::BreakOnThrow is set, the debugger will be broken into (if attached).
[[noreturn]] FALCOR_API void throwException(const fstd::source_location& loc, std::string_view msg);

namespace detail {
/// Overload to allow FALCOR_THROW to be called with a message only.
[[noreturn]] inline void throwException(const fstd::source_location& loc, std::string_view msg) {
    ::Falcor::throwException(loc, msg);
}

/// Overload to allow FALCOR_THROW to be called with a format string and arguments.
template<typename... Args>
[[noreturn]] inline void throwException(const fstd::source_location& loc, fmt::format_string<Args...> fmt, Args&&... args) {
    ::Falcor::throwException(loc, fmt::format(fmt, std::forward<Args>(args)...));
}

}  // namespace detail

/// Flags controlling the error diagnostic behavior.
enum class ErrorDiagnosticFlags{
    None = 0,
    /// Break into debugger (if attached) when calling FALCOR_THROW.
    BreakOnThrow,
    /// Break into debugger (if attached) when calling FALCOR_ASSERT.
    BreakOnAssert,
    /// Append a stack trace to the exception error message when using FALCOR_THROW and FALCOR_ASSERT.
    AppendStackTrace = 2,
    /// Show a message box when reporting errors using the reportError() functions.
    ShowMessageBoxOnError = 4,
};
enum_class_operators(ErrorDiagnosticFlags);

}  // namespace Falcor

/// Helper for throwing a RuntimeError exception.
/// Accepts either a string or a format string and arguments:
/// FALCOR_THROW("This is an error message.");
/// FALCOR_THROW("Expected {} items, got {}.", expectedCount, actualCount);
#define FALCOR_THROW(...) ::Falcor::detail::throwException(fstd::source_location::current(), __VA_ARGS__)

/// Helper for throwing a RuntimeError exception if condition isn't met.
/// Accepts either a string or a format string and arguments.
/// FALCOR_CHECK(device != nullptr, "Device is null.");
/// FALCOR_CHECK(count % 3 == 0, "Count must be a multiple of 3, got {}.", count);
#define FALCOR_CHECK(cond, ...)        \
    do                                 \
    {                                  \
        if (!(cond))                   \
            FALCOR_THROW(__VA_ARGS__); \
    } while (0)


// Remove defines from XLib.h (included by vulkan.h) that cause conflicts
#ifndef _WIN32
#undef None
#undef Status
#undef Bool
#undef Always
#endif

#ifdef WIN32
using WindowHandle = HWND;
#else
struct WindowHandle {
    void*       pDisplay;
    uint32_t    window;
};
#endif

namespace Falcor {

// Required to_string functions
using std::to_string;
inline std::string to_string(const std::string& s) { return '"' + s + '"'; }  // Here for completeness
// Use upper case True/False for compatibility with Python
inline std::string to_string(bool b) { return b ? "True" : "False"; }

template<typename A, typename B>
#ifdef _WIN32
std::string to_string(const std::pair<typename A, typename B>& p)
#else
std::string to_string(const std::pair<A, B>& p)
#endif
{
    return "[" + to_string(p.first) + ", " + to_string(p.second) + "]";
}

inline std::string to_string(const std::wstring& wstr) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wstr);
}

// Helper to check if a type has an iterator
template<typename T, typename = void>   struct has_iterator : std::false_type {};
template<typename T>                    struct has_iterator<T, std::void_t<typename T::const_iterator>> : std::true_type {};

template<typename T>
std::enable_if_t<has_iterator<T>::value, std::string> to_string(const T& t) {
    std::string s = "[";
    bool first = true;
    for (const auto i : t) {
        if (!first) s += ", ";
        first = false;
        s += to_string(i);
    }
    return s + "]";
}

}  // namespace Falcor

#if FALCOR_MSVC
// Enable Windows visual styles
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#define deprecate(_ver_, _msg_) __declspec(deprecated("This function has been deprecated in " ##  _ver_ ## ". " ## _msg_))
#define forceinline __forceinline
using DllHandle = HMODULE;
using SharedLibraryHandle = HMODULE;
#define suppress_deprecation __pragma(warning(suppress : 4996));
#elif FALCOR_GCC
#define deprecate(_ver_, _msg_) __attribute__ ((deprecated("This function has been deprecated in " _ver_ ". " _msg_)))
#define forceinline __attribute__((always_inline))
using DllHandle = void*;
using SharedLibraryHandle = void*;
#define suppress_deprecation _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#endif

#include "Falcor/Core/Platform/OS.h"
//#include "Falcor/Utils/Timing/Profiler.h"
//#include "Falcor/Utils/Scripting/Scripting.h"

#if (_ENABLE_NVAPI == true)
#include "nvapi.h"
#pragma comment(lib, "nvapi64.lib")
#endif

#if FALCOR_GCC
// restore compiler switches
#pragma GCC diagnostic pop
#endif

#endif  // SRC_FALCOR_CORE_FRAMEWORK_H_
