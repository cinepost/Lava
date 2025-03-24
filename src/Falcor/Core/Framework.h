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

#include "Falcor/Core/Macros.h"
#include "FalcorPlatform.h"

#if FALCOR_GCC
// save compiler switches
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING

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


#define safe_delete(_a) {delete _a; _a = nullptr;}
#define safe_delete_array(_a) {delete[] _a; _a = nullptr;}
#define stringize(a) #a
#define concat_strings_(a, b) a##b
#define concat_strings(a, b) concat_strings_(a, b)

namespace Falcor {

/*!
*  \addtogroup Falcor
*  @{
*/

/** Shading languages. Used for shader cross-compilation.
*/
enum class ShadingLanguage {
    Unknown,        ///< Unknown language (e.g., for a plain .h file)
    GLSL,           ///< OpenGL Shading Language (GLSL)
    VulkanGLSL,     ///< GLSL for Vulkan
    HLSL,           ///< High-Level Shading Language
    Slang,          ///< Slang shading language
};

/** Flags indicating what hot-reloadable resources have changed
*/
enum class HotReloadFlags {
    None    = 0,    ///< Nothing. Here just for completeness
    Program = 1,    ///< Programs (shaders)
};

ENUM_CLASS_OPERATORS(HotReloadFlags);

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
using DllHandle = HMODULE;
using SharedLibraryHandle = HMODULE;
#define suppress_deprecation __pragma(warning(suppress : 4996));
#elif FALCOR_GCC
#define deprecate(_ver_, _msg_) __attribute__ ((deprecated("This function has been deprecated in " _ver_ ". " _msg_)))
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
