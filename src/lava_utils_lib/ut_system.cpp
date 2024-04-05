#include <memory>
#include <string>
#include <stdexcept>

#include "ut_string.h"
#include "cpuinfo.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace lava { namespace ut { namespace system {

static void init_cpuinfo() {
    static bool cpuinfo_initialized = false;
    if(!cpuinfo_initialized) {
        cpuinfo_initialize();
        cpuinfo_initialized = true;
    }
}

std::string getCPUName() {
    init_cpuinfo();
    return cpuinfo_get_package(0)->name;
}

unsigned long long getTotalSystemMemoryBytes() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullTotalPhys;
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
#endif
}

}}} // namespace lava::ut::system
