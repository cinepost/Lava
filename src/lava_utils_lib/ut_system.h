#ifndef LAVA_UTILS_UT_SYSTEM_H_
#define LAVA_UTILS_UT_SYSTEM_H_

#include <string>
#include <vector>


namespace lava { namespace ut { namespace system {

std::string getCPUName();
unsigned long long getTotalSystemMemoryBytes();

}}} // namespace lava::ut::system

#endif // LAVA_UTILS_UT_SYSTEM_H_