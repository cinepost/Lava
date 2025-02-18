#include <mutex>
#include <unordered_map>

#include "Falcor/Utils/Debug/DebugMemAlloc.h"

#ifdef _DEBUG

std::atomic<int> gNumberOfAllocCalls = 0;
std::atomic<int> gNumberOfDeallocCalls = 0;

#endif

void printMemAllocCount() {
#ifdef _DEBUG
  printf("Number of active CPU memory allocations: %zd\n", gNumberOfAllocs.load());
#endif
}
