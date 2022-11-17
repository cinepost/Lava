#include <memory>
#include <string>
#include <stdexcept>

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

std::vector<std::string> split(const std::string& s, char seperator) {
   std::vector<std::string> output;

    std::string::size_type prev_pos = 0, pos = 0;

    while((pos = s.find(seperator, pos)) != std::string::npos) {
        std::string substring( s.substr(prev_pos, pos-prev_pos) );
        output.push_back(substring);
        prev_pos = ++pos;
    }
    output.push_back(s.substr(prev_pos, pos-prev_pos)); // Last word
    return output;
}

}}} // namespace lava::ut::string
