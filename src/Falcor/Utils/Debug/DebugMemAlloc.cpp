#include <mutex>
#include <unordered_map>

#include "Falcor/Utils/Debug/DebugMemAlloc.h"

#ifdef _DEBUG

std::atomic<int64_t> gNumberOfAllocs = 0;
std::atomic<int64_t> gNumberOfAllocCalls = 0;
std::atomic<int64_t> gNumberOfDeallocCalls = 0;

#endif

void printMemAllocCount() {
#ifdef _DEBUG
  printf("Number of active CPU memory allocations: %zd\n", gNumberOfAllocs.load());
#endif
}
