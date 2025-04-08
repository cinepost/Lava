/*
 *  Copyright 2018 Laika, LLC. Authored by Peter Stuart
 *
 *  Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
 *  http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
 *  http://opensource.org/licenses/MIT>, at your option. This file may not be
 *  copied, modified, or distributed except according to those terms.
 */

#ifndef BGEO_PARSER_READ_ERROR_H
#define BGEO_PARSER_READ_ERROR_H

#include <stdexcept>
#include <vector>
#include <string>

//#include "../houdini_inc.h"

static std::string join(const std::vector<std::string>& v, const std::string& delimiter = " ") {
    std::string out;
    if (auto i = v.begin(), e = v.end(); i != e) {
        out += *i++;
        for (; i != e; ++i) out.append(delimiter).append(*i);
    }
    return out;
}

namespace ika {
namespace bgeo {
namespace parser {

class ReadError : public std::runtime_error {
public:
    explicit ReadError(const char* message) : std::runtime_error("") {
        errorString = std::string(message);
    }

    explicit ReadError(const std::vector<std::string>& errors) : std::runtime_error("") {
        errorString = join(errors);
        errorString.insert(0, "Bgeo read error: ");
    }

    ~ReadError() noexcept(true) { }

    /*virtual*/ const char* what() const noexcept(true) {
        return errorString.c_str();
    }

private:
    std::string errorString;
};

} // namespace parser
} // namespace bgeo
} // namespace ika

#endif // BGEO_PARSER_READ_ERROR_H
