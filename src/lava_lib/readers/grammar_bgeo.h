#ifndef SRC_LAVA_LIB_GRAMMAR_BGEO_H_
#define SRC_LAVA_LIB_GRAMMAR_BGEO_H_

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
#include <boost/fusion/adapted.hpp>

#include <boost/container/static_vector.hpp>

#include "renderer_iface_lsd.h"

namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;

namespace lava {

namespace bgeo {

namespace ast {
    /*
    typedef x3::variant<int, double> number_t;
    typedef x3::variant<std::string, number_t, bool> value_t;
    typedef std::vector<std::pair<std::string, value_t>> object_t;
    typedef std::vector<value_t> array_t;
    typedef x3::variant<object_t, array_t> bgeo_t;
    */

    struct Array;
    struct Object;
    struct Value;
    struct Pair;

    struct NoValue {
        bool operator==(NoValue const &) const { return true; }
    };

    struct Number: x3::variant<int, double> {
        using base_type::base_type;
        using base_type::operator=;
    };

    struct Object: std::vector<Pair> { };
    
    struct Array: std::vector<Value> { };
    
    struct Value: x3::variant<std::string, Number, bool, Object, Array, bool, NoValue> {
        using base_type::base_type;
        using base_type::operator=;
    };

    struct Bgeo: x3::variant<Array, Object> { 
        using base_type::base_type;
        using base_type::operator=;
    };

    struct Pair{
        std::string key;
        Value value;
    };

}  // namespace ast

} }
 
BOOST_FUSION_ADAPT_STRUCT(lava::bgeo::ast::Pair, key, value)

namespace lava {

namespace bgeo {

struct EchoVisitor: public boost::static_visitor<> {
    std::ostream& _os;

    EchoVisitor(): _os(std::cout){}

    void operator()(ast::NoValue const& n) const { _os << "null"; }
    void operator()(bool b) const { _os << (b ? "true":"false"); }
    void operator()(int i) const { _os << i; }
    void operator()(double d) const { _os << d; }
    void operator()(char c) const { _os << c; }
    void operator()(std::string s) const { _os << '"' << s << '"'; }

    void operator()(ast::Bgeo const& g) const { 
        _os << "\x1b[32m";
        boost::apply_visitor(EchoVisitor{}, g);
        _os << "\x1b[0m\n";
    }
    
    void operator()(ast::Object const& o) const { 
        _os << "{";
        if (!o.empty()) {
            for(ast::Object::const_iterator it = o.begin(); it != (o.end()-1); it++) {
                _os << it->key << ": ";
                operator()(it->value);
                _os << ", ";
            }
        
            _os << o.back().key << ": ";
            operator()(o.back().value);
        }
        _os << "}"; 
    }
    void operator()(ast::Array const& a) const {
        _os << "[";
        if (!a.empty()) {
            for(ast::Array::const_iterator it = a.begin(); it != (a.end()-1); it++) {
                operator()(*it);
                _os << ", ";
            }
            operator()(a.back());
        }
        _os << "]";
    }

    void operator()(ast::Value const& v) const { 
        boost::apply_visitor(EchoVisitor{}, v); 
    }

    void operator()(ast::Number const& n) const { 
        boost::apply_visitor(EchoVisitor{}, n);
    }

};

namespace parser {
    namespace ascii = boost::spirit::x3::ascii;
    using namespace x3;

    struct BoolValuesTable : x3::symbols<bool> {
        BoolValuesTable() {
            add ("false"    , false)
                ("true"     , true)
                ("False"    , false)
                ("True"     , true)
                ("FALSE"    , false)
                ("TRUE"     , true)
                ;
        }
    } const bool_value;

    auto assign_null = [](auto& ctx) {
        _val(ctx) = ast::NoValue();
    };

    auto const esc_char 
        = x3::rule<struct esc_char_, char> {"esc_char"}
        = '\\' >> char_("\"");

    auto const null_value 
        = x3::rule<struct null_value_, ast::NoValue> {"null_value"}
        = lit("null") | lit("Null") | lit("NULL");

    x3::rule<class quoted_string_, std::string> const quoted_string = "quoted_string";
    auto const quoted_string_def = 
        lexeme["\"" > *(esc_char | ~char_('"')) > "\""];
    BOOST_SPIRIT_DEFINE(quoted_string)

    x3::rule<class empry_array_> const empty_array = "empty_array";
    x3::rule<class array_, ast::Array> const array = "array";

    x3::rule<class empry_object_> const empty_object = "empty_object";
    x3::rule<class object_, ast::Object> const object = "object";

    //
    // Since a double parser also parses an integer, we will always get a double, even if the input is "12"
    // In order to prevent this, we need a strict double parser
    //
    boost::spirit::x3::real_parser<double, boost::spirit::x3::strict_real_policies<double> > const double_ = {};

    x3::rule<class number_, ast::Number> const number = "number";
    auto const number_def =  double_ | int32;
    BOOST_SPIRIT_DEFINE(number)

    x3::rule<class value_, ast::Value> const value = "value";
    auto const value_def = array | object | number | quoted_string | bool_value | null_value;
    BOOST_SPIRIT_DEFINE(value)

    x3::rule<class pair_, ast::Pair> const pair = "pair";
    auto const pair_def = quoted_string >> ':' >> value;
    BOOST_SPIRIT_DEFINE(pair)

    auto const empty_object_def = char_('{') >> char_('}');
    BOOST_SPIRIT_DEFINE(empty_object)

    auto const object_def = ('{' >> pair >> *(',' >> pair) >> '}') | empty_object;
    BOOST_SPIRIT_DEFINE(object)

    auto const empty_array_def = char_('[') >> char_(']');
    BOOST_SPIRIT_DEFINE(empty_array)

    auto const array_def = ('[' >> value >> *(',' >> value) >> ']') | empty_array;
    BOOST_SPIRIT_DEFINE(array)

    x3::rule<class bgeo_, ast::Bgeo> const bgeo = "bgeo";
    auto const bgeo_def = array | object;
    BOOST_SPIRIT_DEFINE(bgeo)

    auto const input  = skip(blank | char_("\n\t")) [bgeo];
}  // namespace parser

}  // namespace bgeo

}  // namespace lava

#endif  // SRC_LAVA_LIB_GRAMMAR_BGEO_H_