#ifndef __FALCOR_UTILS_DEBUG_MEMALLOC_H__
#define __FALCOR_UTILS_DEBUG_MEMALLOC_H__

//#include <new>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <atomic>

#include "lava_utils_lib/logging.h"

#ifdef HUY_DEBUG

#undef new

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

//#define new new(__FILE__, __LINE__, __FUNCTION__)
//#define new(...) new (__FILE__, __LINE__, __FUNCTION__)

#define new(...) new(__VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)

//#define NEW new (__FILE__, __LINE__, __FUNCTION__)
//#define new NEW

#endif // _DEBUG

void printMemAllocCount();

#endif // __FALCOR_UTILS_DEBUG_MEMALLOC_H__