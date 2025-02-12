#include <unordered_map>

#include "Falcor/Utils/Debug/DebugMemAlloc.h"

#ifdef _DEBUG

std::atomic<int> gNumberOfAllocCalls = 0;
std::atomic<int> gNumberOfDeallocCalls = 0;
std::atomic<int> gNumberOfAllocs = 0;

// Custom new operator. Do your memory logging here.
static void* operator new (size_t size, char* file, unsigned int line) {
    void* x = malloc(size);
    //cout << "Allocated " << size << " byte(s) at address " << x << " in " << file << ":" << line << endl;
    return x;  
}

// You must override the default delete operator to detect all deallocations
void operator delete (void* p) {
   free(p);
   //cout << "Freed memory at address " << p << endl;
}

// You also should provide an overload with the same arguments as your
// placement new. This would be called in case the constructor of the 
// created object would throw.
static void operator delete (void* p, char* file, unsigned int line) {
   free(p);
   //cout << "Freed memory at address " << p << endl;
}

#define new new(__FILE__, __LINE__)

#endif

void printMemAllocCount() {
#ifdef _DEBUG
  printf("Number of active CPU memory allocations: %zd\n", gNumberOfAllocs.load());
#endif
}
