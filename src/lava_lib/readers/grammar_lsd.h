#ifndef SRC_LAVA_LIB_GRAMMAR_LSD_H_
#define SRC_LAVA_LIB_GRAMMAR_LSD_H_

#include <array>
#include <memory>
#include <string>
#include <algorithm>

#include <boost/array.hpp>
#include <boost/filesystem.hpp>
#include <boost/range.hpp>
//#include <boost/range/join.hpp>
//#include <boost/algorithm/string/join.hpp>

#ifdef DEBUG
    #define BOOST_SPIRIT_X3_DEBUG
#endif

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/include/support_istream_iterator.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>

#include "renderer_iface_lsd.h"


namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;

namespace lava {

namespace lsd { 

namespace ast {

    typedef std::array<double, 16> matrix;

    template<class T, std::size_t N>
    static inline std::ostream& operator<<(std::ostream& os, const std::array<T, N>& m) {
        return os << "[ " << m.data() << " ]";
    };

    enum class Type { FLOAT, BOOL, INT, VECTOR2, VECTOR3, VECTOR4, MATRIX3, MATRIX4, STRING };
    enum class Object { GLOBAL, MATERIAL, GEO, GEOMERTY, SEGMENT, CAMERA, LIGHT, FOG, OBJECT, INSTANCE, PLANE, IMAGE, RENDERER };

    static inline std::ostream& operator<<(std::ostream& os, Type t) {
        switch(t) {
            case Type::FLOAT: return os << "float";
            case Type::BOOL: return os << "bool";
            case Type::INT: return os << "int";
            case Type::VECTOR2: return os << "vector2";
            case Type::VECTOR3: return os << "vector3";
            case Type::VECTOR4: return os << "vector4";
            case Type::MATRIX3: return os << "matrix3";
            case Type::MATRIX4: return os << "matrix4";
            case Type::STRING: return os << "string";
        }
        return os << "unknown type";
    };

    static inline std::ostream& operator<<(std::ostream& os, Object o) {
        switch(o) {
            case Object::GLOBAL: return os << "global";
            case Object::GEO: return os << "geo";
            case Object::GEOMERTY: return os << "geometry";
            case Object::MATERIAL: return os << "material";
            case Object::SEGMENT: return os << "segment";
            case Object::CAMERA: return os << "camera";
            case Object::LIGHT: return os << "light";
            case Object::FOG: return os << "fog";
            case Object::OBJECT: return os << "object";
            case Object::INSTANCE: return os << "instance";
            case Object::PLANE: return os << "plane";
            case Object::IMAGE: return os << "image";
            case Object::RENDERER: return os << "renderer";
        }
        return os << "unknown object";
    };

    using boost::fusion::operators::operator<<;
    static inline std::ostream& operator<<(std::ostream& os, const std::array<double, 16>& m) {
        return os << "[ " << m.data() << " ]";
    };

    struct cmd_time;
    struct cmd_version;
    struct cmd_defaults;
    struct cmd_transform;
    struct cmd_quit;
    struct cmd_start;
    struct cmd_end;
    

    typedef x3::variant<
        cmd_start,
        cmd_time,
        cmd_version,
        cmd_defaults,
        cmd_transform,
        cmd_end,
        cmd_quit
    > commands;

    /*
    struct employee {
        int age;
        std::string surname;
        std::string forename;
        double salary;
    };

    struct team {
        std::string name;
        int num_employees;
    };

    struct department {
        std::string name;
        int num_teams;
        double budget;
    };

    struct corporation {
        std::string name;
        int num_depts;
    };

    */

    // nullary commands
    struct cmd_end { };
    struct cmd_quit { };

    // non-nullary commands
    struct cmd_time {
        float time;
    };

    struct cmd_start {
        Object type;
    };

    struct cmd_transform {
        boost::array<double, 16> m;
    };

    struct cmd_version {
        int maj, min, bld;
    };

    struct cmd_defaults {
        std::string filename;
    };

}}  // namespace lsd::ast

}  // namespace lava

BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_transform, m)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_start, type)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_end)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_quit)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_time, time)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_defaults, filename)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_version, maj, min, bld)

namespace lava { 


namespace lsd { 

/*
struct printer : public boost::static_visitor<> {
    void operator()(ast::Object a) const {
        printf("Command: %d, Month: %d, Day: %d\n", a.year, a.month, a.day);
    }
    void operator()(ast::commands& cmd) const {
        std::cout << boost::fusion::as_deque(cmd) << "\n";
    }
};
*/

namespace parser {
    namespace ascii = boost::spirit::x3::ascii;

    using namespace x3;

    auto const string 
        = x3::rule<struct string_, std::string> {"string"}
        = lexeme[+graph];

    auto const comment
        = x3::space | x3::lexeme[ '#' >> *(char_ - eol) >> eol];

    x3::rule<class no_quote_string_, std::string> const no_quote_string = "no_quote_string";
    auto const no_quote_string_def = lexeme[+(~char_('"'))];
    BOOST_SPIRIT_DEFINE(no_quote_string)

    x3::rule<class double_quote_string_, std::string> const double_quote_string = "double_quote_string";
    auto const double_quote_string_def = lexeme['"' > *(~char_('"')) > '"'];
    BOOST_SPIRIT_DEFINE(double_quote_string)

    x3::rule<class single_quote_string_, std::string> const single_quote_string = "single_quote_string";
    auto const single_quote_string_def = lexeme['\'' > *(~char_('\'')) > '\''];
    BOOST_SPIRIT_DEFINE(single_quote_string)

    x3::rule<class identifier_, std::string> const identifier = "identifier";
    auto const identifier_def = lexeme[(alpha | char_('_')) >> *(alnum | char_('_'))];
    BOOST_SPIRIT_DEFINE(identifier)

    x3::rule<class objname_, std::string> const objname = "objname";
    auto const objname_def = lexeme['/' >> no_quote_string >> *('/' >> no_quote_string)];
    BOOST_SPIRIT_DEFINE(objname)

    x3::rule<class filepath_, fs::path> const filepath = "filepath";
    auto const filepath_def = lexeme[(alpha | char_('_') | char_('/')) >> *(alnum | char_('_') | char_('/'))];
    BOOST_SPIRIT_DEFINE(filepath)

    x3::rule<class any_string_, std::string> const any_string = "any_string";
    auto const any_string_def = single_quote_string | double_quote_string | no_quote_string;
    BOOST_SPIRIT_DEFINE(any_string)

    x3::rule<class matrix_, std::string> const matrix = "matrix";
    auto const matrix_def = repeat(16) [ double_ ];
    BOOST_SPIRIT_DEFINE(matrix)

    /*
    auto const employee
        = x3::rule<class employee, ast::employee>{"employee"}
        = int_ >> string >> string >> double_;

    auto const team
        = x3::rule<class team, ast::team>{"team"}
        = string >> int_;

    auto const department
        = x3::rule<class department, ast::department>{"department"}
        = string >> int_ >> double_;

    auto const corporation
        = x3::rule<class corporation, ast::corporation>{"corporation"}
        = string >> int_;
    */

    struct objects_table : x3::symbols<ast::Object> {
        objects_table() {
            add ("gloabal"  , ast::Object::GLOBAL)
                ("geo"      , ast::Object::GEO)
                ("geometry" , ast::Object::GEO)
                ("material" , ast::Object::GEOMERTY)
                ("segment"  , ast::Object::SEGMENT)
                ("camera"   , ast::Object::CAMERA)
                ("light"    , ast::Object::LIGHT)
                ("fog"      , ast::Object::FOG)
                ("object"   , ast::Object::OBJECT)
                ("instance" , ast::Object::INSTANCE)
                ("plane"    , ast::Object::PLANE)
                ("image"    , ast::Object::IMAGE)
                ("renderer" , ast::Object::RENDERER);
        }
    } const object;

    auto const cmd_transform
        = x3::rule<class cmd_transform, ast::cmd_transform>{"cmd_transform"}
        = "cmd_transform" >> matrix;

    auto const cmd_start
        = x3::rule<class cmd_start, ast::cmd_start>{"cmd_start"}
        = "cmd_start" >> object;

    auto const cmd_time
        = x3::rule<class cmd_time, ast::cmd_time>{"cmd_time"}
        = "cmd_time" >> float_;

    auto const cmd_version
        = x3::rule<class cmd_version, ast::cmd_version>{"cmd_version"}
        = "cmd_version" >> lexeme["VEX" >> int_ >> "." >> int_ >> "." >> int_];

    auto const cmd_defaults
        = x3::rule<class cmd_defaults, ast::cmd_defaults>{"cmd_defaults"}
        = "cmd_defaults" >> any_string;

    auto const cmd_quit
        = x3::rule<class cmd_quit, ast::cmd_quit>{"cmd_quit"}
        = "cmd_quit" >> eps;

    auto const cmd_end
        = x3::rule<class cmd_end, ast::cmd_end>{"cmd_end"}
        = "cmd_end" >> eps;

    auto cmds = cmd_time | cmd_version | cmd_defaults | cmd_end | cmd_quit | cmd_start | cmd_transform;
    //auto const input  = skip(blank) [ *(cmds >> eol) ];
    auto const input  = skip(blank | comment) [ cmds % eol ];

}}  // namespace lsd::parser

}  // namespace lava

#endif  // SRC_LAVA_LIB_GRAMMAR_LSD_H_