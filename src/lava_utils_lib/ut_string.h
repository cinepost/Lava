#ifndef LAVA_UTILS_UT_STRING_H_
#define LAVA_UTILS_UT_STRING_H_

#include <string>
#include <vector>


namespace lava { namespace ut { namespace string {

std::string replace(const std::string& str, const std::string& from, const std::string& to);
std::vector<std::string> split(const std::string& s, char seperator);

}}} // namespace lava::ut::string

#endif // LAVA_UTILS_UT_STRING_H_