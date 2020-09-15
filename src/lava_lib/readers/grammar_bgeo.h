#ifndef SRC_LAVA_LIB_GRAMMAR_LSD_H_
#define SRC_LAVA_LIB_GRAMMAR_LSD_H_

#include <array>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include <variant>
#include <map>

#include <boost/array.hpp>
#include <boost/filesystem.hpp>
#include <boost/range.hpp>
//#include <boost/range/join.hpp>
//#include <boost/algorithm/string/join.hpp>

#ifdef DEBUG
   // #define BOOST_SPIRIT_X3_DEBUG
#endif

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/support/traits/container_traits.hpp>
#include <boost/spirit/include/support_istream_iterator.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/sequence/io.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/adapted/array.hpp>
#include <boost/fusion/adapted/std_pair.hpp>

#include <boost/container/static_vector.hpp>

#include "renderer_iface_lsd.h"

namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;

namespace lava {

namespace bgeo {

namespace ast {
    struct Array;
    struct Element;

    struct Tuple: std::vector<std::pair<Element, Element>> { };
    struct Array: std::vector<Element> { };
    struct Element: x3::variant<std::string, int, double, bool, Tuple, Array> { };
}
 
namespace parser {
    namespace ascii = boost::spirit::x3::ascii;
    using namespace x3;

    x3::rule<struct rule_key_t, std::string> s;
    x3::rule<struct rule_element_t, ast::Element> r;
    x3::rule<struct rule_braced_t, ast::Element>  e;

    auto s_def = '"' >> ~char_('"') >> '"';
    auto r_def = (s >> ':' >> int_) % ',';
    auto tuple_def = '{' >> r >> '}';
    auto array_def = '[' >> r >> ']';

    BOOST_SPIRIT_DEFINE(s,r, array_def, tuple_def)
}  // namespace parser

}  // namespace bgeo

}  // namespace lava

#endif  // SRC_LAVA_LIB_GRAMMAR_LSD_H_