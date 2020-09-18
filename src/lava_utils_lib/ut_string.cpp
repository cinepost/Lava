#include "ut_string.h"

namespace lava { namespace ut { namespace string {

std::string replace(const std::string& str, const std::string& from, const std::string& to) {
	if(from.empty())
        return str;

    std::string result = str;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        result.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
    return result;
}

}}} // namespace lava::ut::string
