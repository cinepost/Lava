#ifndef SRC_LAVA_LIB_GRAMMAR_LSD_H_
#define SRC_LAVA_LIB_GRAMMAR_LSD_H_

#include <array>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include <variant>

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

#include "renderer_iface_lsd.h"

namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;

namespace lava {

namespace lsd {

    // TODO: switch to std::array<double, N>
    typedef std::vector<double> Vector2; 
    typedef std::vector<double> Vector3;
    typedef std::vector<double> Vector4; 
    typedef std::vector<double> Matrix3;
    typedef std::array<double, 16> Matrix4;

namespace ast {

    //using boost::fusion::operators::operator>>; // for input
    //using boost::fusion::operators::operator<<; // for output

    enum class Type { FLOAT, BOOL, INT, VECTOR2, VECTOR3, VECTOR4, MATRIX3, MATRIX4, STRING };
    enum class Object { GLOBAL, MATERIAL, GEO, GEOMERTY, SEGMENT, CAMERA, LIGHT, FOG, OBJECT, INSTANCE, PLANE, IMAGE, RENDERER };
    struct Version { uint major, minor, build; };

    struct cmd_time;
    struct cmd_version;
    struct cmd_defaults;
    struct cmd_transform;
    struct cmd_quit;
    struct cmd_start;
    struct cmd_end;
    struct cmd_detail;
    struct cmd_geometry;

    typedef x3::variant<
        cmd_start,
        cmd_time,
        cmd_version,
        cmd_defaults,
        cmd_transform,
        cmd_end,
        cmd_quit,
        cmd_detail,
        cmd_geometry
    > Command;

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
        Matrix4 m;
    };

    struct cmd_version {
        //int maj, min, bld;
        Version version;
    };

    struct cmd_defaults {
        std::string filename;
    };

    struct cmd_geometry {
        std::string geometry_object;
    };

    struct cmd_detail {
        std::string name;
        std::string filename;
    };

}  // namespace ast

static inline std::ostream& operator<<(std::ostream& os, const Matrix3& m) {
    os << "Matrix3[ ";
    std::copy(m.begin(), m.end(), std::ostream_iterator<int>(os, " "));
    return os << "]";
};

static inline std::ostream& operator<<(std::ostream& os, const Matrix4& m) {
    os << "Matrix4[ ";
    std::copy(m.begin(), m.end(), std::ostream_iterator<int>(os, " "));
    return os << "]";
};

using Version = ast::Version;
static inline std::ostream& operator<<(std::ostream& os, Version v) {
    return os << "Version: " << v.major << "." << v.minor << "." << v.build;
};

using Type = ast::Type;
static inline std::ostream& operator<<(std::ostream& os, Type t) {
    os << "Type: ";
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

using Object = ast::Object;
static inline std::ostream& operator<<(std::ostream& os, Object o) {
    os << "Object: ";
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

}  // namespace lsd

}  // namespace lava

BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_end)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_quit)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_time, time)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_start, type)
//BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_transform, m)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_defaults, filename)
//BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_version, version)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_detail, name, filename)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_geometry, geometry_object)

namespace lava { 

namespace lsd { 

/*
struct LSDVisitor(RendererInface::SharedPtr iface): public boost::static_visitor<> {
    LSDVisitor((RendererInface::SharedPtr iface) : iface(iface) {}

 private:
    RendererInface::SharedPtr iface;
};
*/

struct EchoVisitor: public boost::static_visitor<> {
    std::ostream& _os;

    using Command   = ast::Command;
    using Object    = ast::Object;

    EchoVisitor(): _os(std::cout){}

    void operator()(ast::cmd_end const& c) const { _os << "> cmd_end: " << "\n"; }
    void operator()(ast::cmd_quit const& c) const { _os << "> cmd_quit: " << "\n"; }
    void operator()(ast::cmd_start const& c) const { _os << "> cmd_start: " << c.type << "\n"; }
    void operator()(ast::cmd_time const& c) const { _os << "> cmd_time: " << c.time << "\n"; }
    void operator()(ast::cmd_detail const& c) const { _os << "> cmd_detail: name: " << c.name << " filename: " << c.filename << "\n"; }
    void operator()(ast::cmd_version const& c) const { _os << "> cmd_version: " << c.version << "\n"; }
    void operator()(ast::cmd_defaults const& c) const { _os << "> cmd_defaults: filename: " << c.filename << "\n"; }
    void operator()(ast::cmd_transform const& c) const { _os << "> cmd_transform: " << c.m << "\n"; }
    void operator()(ast::cmd_geometry const& c) const { _os << "> cmd_geometry: geometry_object: " << c.geometry_object << "\n"; }
};


namespace validators {
    auto is_valid_group = [](auto& ctx) {
        _pass(ctx) = 0 == (_val(ctx).size() % 3);
    };
}

namespace parser {
    namespace ascii = boost::spirit::x3::ascii;

    using namespace x3;

    auto const string 
        = x3::rule<struct string_, std::string> {"string"}
        = lexeme[+graph];

    auto const comment
        = x3::space | x3::lexeme[ '#' >> *(char_ - eol) >> eol];

    //auto const comment = blank | lexeme[ 
    //    "/*" >> *(char_ - "*/") >> "*/"
    //    | "//" >> *~char_("\r\n") >> eol
    //    | '#' >> *(char_ - eol) >> eol
    //];

    auto const skipper = blank | comment;

    x3::rule<class unquoted_string_, std::string> const unquoted_string = "unquoted_string";
    auto const unquoted_string_def = lexeme[+(~char_("\"\'"))];
    BOOST_SPIRIT_DEFINE(unquoted_string)

    x3::rule<class quoted_string_, std::string> const quoted_string = "quoted_string";
    auto const quoted_string_def = lexeme[char_("\"\'") > *(~char_('\'')) > char_("\"\'")];
    BOOST_SPIRIT_DEFINE(quoted_string)

    x3::rule<class any_string_, std::string> const any_string = "any_string";
    auto const any_string_def = unquoted_string | quoted_string;
    BOOST_SPIRIT_DEFINE(any_string)

    x3::rule<class identifier_, std::string> const identifier = "identifier";
    auto const identifier_def = lexeme[(alpha | char_('_')) >> *(alnum | char_('_'))];
    BOOST_SPIRIT_DEFINE(identifier)

    x3::rule<class objname_, std::string> const objname = "objname";
    auto const objname_def = lexeme[char('/') >> identifier >> *(char('/') >> identifier)];
    BOOST_SPIRIT_DEFINE(objname)

    x3::rule<class unquoted_filename_, std::string> const unquoted_filename = "unquoted_filename";
    auto const unquoted_filename_def = lexeme[(alnum | char_("/-_.")) >> *(alnum | char_("/-_."))];
    BOOST_SPIRIT_DEFINE(unquoted_filename)

    x3::rule<class quoted_filename_filename_, std::string> const quoted_filename = "quoted_filename";
    auto const quoted_filename_def = lexeme[char_("\"\'") > unquoted_filename > char_("\"\'")];
    BOOST_SPIRIT_DEFINE(quoted_filename)

    x3::rule<class any_filename_, std::string> const any_filename = "any_filename";
    auto const any_filename_def = unquoted_filename | quoted_filename;
    BOOST_SPIRIT_DEFINE(any_filename)

    x3::rule<class matrix3_, Matrix3> const matrix3 = "matrix3";
    auto const matrix3_def = repeat(9) [ double_ ];
    BOOST_SPIRIT_DEFINE(matrix3)

    x3::rule<class matrix4_, std::vector<double>> const matrix4 = "matrix4";
    auto const matrix4_def = repeat(16) [ double_ ];
    BOOST_SPIRIT_DEFINE(matrix4)

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

    struct ObjectsTable : x3::symbols<ast::Object> {
        ObjectsTable() {
            add ("global"   , ast::Object::GLOBAL)
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

    //x3::rule<class object_, ast::Object> const object = "object";
    //auto const object_def = lexeme["global" | "geo" | "geometry"];
    //BOOST_SPIRIT_DEFINE(object)

    using boost::fusion::at_c;
    //auto assign_objtype = [](auto& ctx) { _val(ctx).type = _attr(ctx); };
    //auto assign_time = [](auto& ctx) { _val(ctx).time = _attr(ctx); };
    auto assign_version = [](auto& ctx) { 
        _val(ctx).version.major = at_c<0>(_attr(ctx)); 
        _val(ctx).version.minor = at_c<1>(_attr(ctx));
        _val(ctx).version.build = at_c<2>(_attr(ctx));
    };

    auto assign_transform = [](auto& ctx) {
        std::copy_n(std::make_move_iterator(_attr(ctx).begin()), 16, _val(ctx).m.begin());
    };

    auto const cmd_transform
        = x3::rule<class cmd_transform, ast::cmd_transform>{"cmd_transform"}
        = "cmd_transform" >> matrix4 [assign_transform];

    auto const cmd_start
        = x3::rule<class cmd_start, ast::cmd_start>{"cmd_start"}
        = "cmd_start" >> object;// [assign_objtype];

    auto const cmd_time
        = x3::rule<class cmd_time, ast::cmd_time>{"cmd_time"}
        = "cmd_time" >> float_;//[assign_time];

    auto const cmd_version
        = x3::rule<class cmd_version, ast::cmd_version>{"cmd_version"}
        = "cmd_version" >> lexeme["VEX" >> int_ >> "." >> int_ >> "." >> int_][assign_version];

    auto const cmd_defaults
        = x3::rule<class cmd_defaults, ast::cmd_defaults>{"cmd_defaults"}
        = "cmd_defaults" >> any_filename;

    auto const cmd_detail
        = x3::rule<class cmd_detail, ast::cmd_detail>{"cmd_detail"}
        = "cmd_detail" >> objname >> any_filename;

    auto const cmd_geometry
        = x3::rule<class cmd_geometry, ast::cmd_geometry>{"cmd_geometry"}
        = "cmd_geometry" >> objname;

    auto const cmd_quit
        = x3::rule<class cmd_quit, ast::cmd_quit>{"cmd_quit"}
        = "cmd_quit" >> eps;

    auto const cmd_end
        = x3::rule<class cmd_end, ast::cmd_end>{"cmd_end"}
        = "cmd_end" >> eps;

    auto cmds = cmd_time | cmd_version | cmd_defaults | cmd_end | cmd_quit | cmd_start | cmd_transform | cmd_detail | cmd_geometry;
    //auto const input  = skip(blank) [ *(cmds >> eol) ];
    auto const input  = skip(skipper) [ cmds % eol ];
    
}}  // namespace lsd::parser

}  // namespace lava

#endif  // SRC_LAVA_LIB_GRAMMAR_LSD_H_