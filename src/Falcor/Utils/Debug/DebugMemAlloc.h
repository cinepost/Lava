#ifndef __FALCOR_UTILS_DEBUG_MEMALLOC_H__
#define __FALCOR_UTILS_DEBUG_MEMALLOC_H__

//#include <new>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <atomic>

#ifdef _WIN32
#define FALCOR_API_EXPORT __declspec(dllexport)
#define FALCOR_API_IMPORT __declspec(dllimport)
#else
#define FALCOR_API_EXPORT
#define FALCOR_API_IMPORT
#endif

#ifdef FALCOR_DLL
#define FALCOR_API FALCOR_API_EXPORT
#else
#define FALCOR_API FALCOR_API_IMPORT
#endif

#ifdef NO_DEBUG

#define BOOST_CONTAINER_DETAIL_PLACEMENT_NEW_HPP

#undef new

struct boost_container_new_t{};

inline void* operator new(std::size_t size, const char* file, int line, const char* function) {
	return malloc(size);
}

inline void* operator new[] (std::size_t size, const char* file, int line, const char* function) {
  return malloc(size);
}

inline void* operator new (std::size_t n, void* ptr, const char* file, int line, const char* function) {
	return ptr;
}

inline void* operator new[] (std::size_t n, void* ptr, const char* file, int line, const char* function) {
	return ptr;
}

inline void operator delete(void* ptr) _GLIBCXX_USE_NOEXCEPT {
	free(ptr);
}

inline void operator delete(void* ptr, const char*, int) _GLIBCXX_USE_NOEXCEPT {
  free(ptr);
}

inline void operator delete(void* ptr, std::size_t) _GLIBCXX_USE_NOEXCEPT {
  free(ptr);
}

inline void operator delete[](void* ptr) _GLIBCXX_USE_NOEXCEPT {
  free(ptr);
}

inline void operator delete[](void* ptr, const char*, int) _GLIBCXX_USE_NOEXCEPT {
  free(ptr);
}

inline void operator delete[](void* ptr, std::size_t) _GLIBCXX_USE_NOEXCEPT {
  free(ptr);
}

// Boost placement

inline void *operator new(std::size_t, void *p, boost_container_new_t) {  
  return p;  }

inline void operator delete(void *, void *, boost_container_new_t) {

}

//#define new new(__FILE__, __LINE__, __FUNCTION__)
//#define new(...) new (__FILE__, __LINE__, __FUNCTION__)

#define new(...) new(__VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)

//#define NEW new (__FILE__, __LINE__, __FUNCTION__)
//#define new NEW

#endif // _DEBUG

FALCOR_API void printMemAllocCount();

#endif // __FALCOR_UTILS_DEBUG_MEMALLOC_H__