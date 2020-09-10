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

#include <boost/container/static_vector.hpp>

#include "renderer_iface_lsd.h"

namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;

namespace lava {

namespace lsd {
    using boost::container::static_vector;

    typedef static_vector<uint, 3> Version;
    
    typedef static_vector<int, 2> Int2;
    typedef static_vector<int, 3> Int3;
    typedef static_vector<int, 4> Int4; 

    typedef static_vector<double, 2> Vector2; 
    typedef static_vector<double, 3> Vector3;
    typedef static_vector<double, 4> Vector4; 
    typedef static_vector<double, 9> Matrix3;
    typedef static_vector<double, 16> Matrix4;
    typedef x3::variant<int, double, std::string> PropValue;

namespace ast {

    //using boost::fusion::operators::operator>>; // for input
    //using boost::fusion::operators::operator<<; // for output

    enum class Type { FLOAT, BOOL, INT, VECTOR2, VECTOR3, VECTOR4, MATRIX3, MATRIX4, STRING };
    enum class Object { GLOBAL, MATERIAL, GEO, GEOMERTY, SEGMENT, CAMERA, LIGHT, FOG, OBJECT, INSTANCE, PLANE, IMAGE, RENDERER };

    struct comment;
    struct setenv;
    struct cmd_time;
    struct cmd_version;
    struct cmd_defaults;
    struct cmd_transform;
    struct cmd_quit;
    struct cmd_start;
    struct cmd_end;
    struct cmd_detail;
    struct cmd_geometry;
    struct cmd_property;
    struct cmd_raytrace;
    struct cmd_image;
    struct cmd_declare;

    typedef x3::variant<
        comment,
        setenv,
        cmd_start,
        cmd_time,
        cmd_version,
        cmd_defaults,
        cmd_transform,
        cmd_end,
        cmd_quit,
        cmd_detail,
        cmd_geometry,
        cmd_property,
        cmd_raytrace,
        cmd_image,
        cmd_declare
    > Command;

    // nullary commands
    struct comment { };
    struct cmd_end { };
    struct cmd_quit { };
    struct cmd_raytrace { };

    // non-nullary commands
    struct setenv {
        std::string key;
        std::string value;
    };

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

    struct cmd_image {
        std::vector<std::string> values;
    };

    struct cmd_property {
        Object style;
        std::string token;
        std::vector<PropValue> values;
    };

    struct cmd_declare {
        Object style;
        Type type;
        std::string token;
        std::vector<PropValue> values;
    };

}  // namespace ast

static inline std::ostream& operator<<(std::ostream& os, const std::vector<std::string>& v) {
    std::copy(v.begin(), v.end(), std::ostream_iterator<std::string>(os, " "));
    return os;
};

static inline std::ostream& operator<<(std::ostream& os, const Int2& m) {
    os << "Int2[ ";
    std::copy(m.begin(), m.end(), std::ostream_iterator<int>(os, " "));
    return os << "]";
};

static inline std::ostream& operator<<(std::ostream& os, const Int3& m) {
    os << "Int3[ ";
    std::copy(m.begin(), m.end(), std::ostream_iterator<int>(os, " "));
    return os << "]";
};

static inline std::ostream& operator<<(std::ostream& os, const Int4& m) {
    os << "Int4[ ";
    std::copy(m.begin(), m.end(), std::ostream_iterator<int>(os, " "));
    return os << "]";
};

static inline std::ostream& operator<<(std::ostream& os, const Vector2& m) {
    os << "Vector2[ ";
    std::copy(m.begin(), m.end(), std::ostream_iterator<double>(os, " "));
    return os << "]";
};

static inline std::ostream& operator<<(std::ostream& os, const Vector3& m) {
    os << "Vector3[ ";
    std::copy(m.begin(), m.end(), std::ostream_iterator<double>(os, " "));
    return os << "]";
};

static inline std::ostream& operator<<(std::ostream& os, const Vector4& m) {
    os << "Vector4[ ";
    std::copy(m.begin(), m.end(), std::ostream_iterator<double>(os, " "));
    return os << "]";
};

static inline std::ostream& operator<<(std::ostream& os, const Matrix3& m) {
    os << "Matrix3[ ";
    std::copy(m.begin(), m.end(), std::ostream_iterator<double>(os, " "));
    return os << "]";
};

static inline std::ostream& operator<<(std::ostream& os, const Matrix4& m) {
    os << "Matrix4[ ";
    std::copy(m.begin(), m.end(), std::ostream_iterator<double>(os, " "));
    return os << "]";
};

static inline std::ostream& operator<<(std::ostream& os, Version v) {
    return os << "Version: " << v[0] << "." << v[1] << "." << v[2];
};

static inline std::ostream& operator<<(std::ostream& os, std::vector<PropValue> v) {
    std::copy(v.begin(), v.end(), std::ostream_iterator<PropValue>(os, " "));
    return os;
};

using Type = ast::Type;
static inline std::ostream& operator<<(std::ostream& os, Type t) {
    //os << "Type: ";
    switch(t) {
        case Type::INT: return os << "int";
        case Type::BOOL: return os << "bool";
        case Type::FLOAT: return os << "float";
        case Type::STRING: return os << "string";
        case Type::VECTOR2: return os << "vector2";
        case Type::VECTOR3: return os << "vector3";
        case Type::VECTOR4: return os << "vector4";
        case Type::MATRIX3: return os << "matrix3";
        case Type::MATRIX4: return os << "matrix4";
    }
    return os << "unknown type";
};

using Object = ast::Object;
static inline std::ostream& operator<<(std::ostream& os, Object o) {
    //os << "Object: ";
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

BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::setenv, key, value)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_end)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_quit)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_raytrace)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_time, time)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_start, type)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_transform, m)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_image, values)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_defaults, filename)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_version, version)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_detail, name, filename)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_geometry, geometry_object)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_property, style, token, values)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_declare, style, type, token, values)

namespace lava { 

namespace lsd { 

/*
struct LSDVisitor(RendererIfaceBase::SharedPtr iface): public boost::static_visitor<> {
    LSDVisitor((RendererIfaceBase::SharedPtr iface) : iface(iface) {}

 private:
    RendererInface::SharedPtr iface;
};
*/

struct EchoVisitor: public boost::static_visitor<> {
    std::ostream& _os;

    using Command   = ast::Command;
    using Object    = ast::Object;

    EchoVisitor(): _os(std::cout){}

    void operator()(ast::comment const& c) const { }
    void operator()(ast::setenv const& c) const { _os << "\x1b[32m" << "> setenv: " << c.key << " = " << c.value << "\x1b[0m\n"; }
    void operator()(ast::cmd_image const& c) const { _os << "\x1b[32m" << "> cmd_image: " << c.values << "\x1b[0m\n"; }
    void operator()(ast::cmd_end const& c) const { _os << "\x1b[32m" << "> cmd_end: " << "\x1b[0m\n"; }
    void operator()(ast::cmd_quit const& c) const { _os << "\x1b[32m" << "> cmd_quit: " << "\x1b[0m\n"; }
    void operator()(ast::cmd_start const& c) const { _os << "\x1b[32m" << "> cmd_start: " << c.type << "\x1b[0m\n"; }
    void operator()(ast::cmd_time const& c) const { _os << "\x1b[32m" << "> cmd_time: " << c.time << "\x1b[0m\n"; }
    void operator()(ast::cmd_detail const& c) const { _os << "\x1b[32m" << "> cmd_detail: name: " << c.name << " filename: " << c.filename << "\x1b[0m\n"; }
    void operator()(ast::cmd_version const& c) const { _os << "\x1b[32m" << "> cmd_version: " << c.version << "\x1b[0m\n"; }
    void operator()(ast::cmd_defaults const& c) const { _os << "\x1b[32m" << "> cmd_defaults: filename: " << c.filename << "\x1b[0m\n"; }
    void operator()(ast::cmd_transform const& c) const { _os << "\x1b[32m" << "> cmd_transform: " << c.m << "\x1b[0m\n"; }
    void operator()(ast::cmd_geometry const& c) const { _os << "\x1b[32m" << "> cmd_geometry: geometry_object: " << c.geometry_object << "\x1b[0m\n"; }
    void operator()(ast::cmd_property const& c) const { _os << "\x1b[32m" << "> cmd_property: style: " << c.style << " token: " << c.token << " value: "<< c.values << "\x1b[0m\n"; }
    void operator()(ast::cmd_declare const& c) const { _os << "\x1b[32m" << "> cmd_declare: style: " << c.style << " token: " << c.token << " type: " << c.type << " value: "<< c.values << "\x1b[0m\n"; }
    void operator()(ast::cmd_raytrace const& c) const { _os << "\x1b[32m" << "> cmd_raytrace: " << "\x1b[0m\n"; }
};


namespace validators {
    auto is_valid_vector2 = [](auto& ctx) {
        _pass(ctx) = 0 == (_val(ctx).size() % 2);
    };

    auto is_valid_vector3 = [](auto& ctx) {
        _pass(ctx) = 0 == (_val(ctx).size() % 3);
    };

    auto is_valid_vector4 = [](auto& ctx) {
        _pass(ctx) = 0 == (_val(ctx).size() % 4);
    };

    auto is_valid_matrix3 = [](auto& ctx) {
        _pass(ctx) = 0 == (_val(ctx).size() % 9);
    };

    auto is_valid_matrix4 = [](auto& ctx) {
        _pass(ctx) = 0 == (_val(ctx).size() % 16);
    };
}

namespace parser {
    namespace ascii = boost::spirit::x3::ascii;

    using namespace x3;

    template <typename T> auto as = [](auto p) { return x3::rule<struct _, T> {} = p; };
    //auto const uintPair = as<ast::uintPair_t> ( uint_ >> '-' >> uint_       );
    //auto const uintObj  = as<ast::uintObj>    ( uintPair | uint_            );
    //auto const varVec   = as<ast::varVec>     ( '[' >> uintObj % ',' >> ']' );

    auto const string 
        = x3::rule<struct string_, std::string> {"string"}
        = lexeme[+graph];

    x3::rule<class unquoted_string_, std::string> const unquoted_string = "unquoted_string";
    auto const unquoted_string_def = lexeme[+(~char_(" \"\'"))];
    BOOST_SPIRIT_DEFINE(unquoted_string)

    x3::rule<class quoted_string_, std::string> const quoted_string = "quoted_string";
    auto const quoted_string_def = 
            lexeme[char_("\"") > *(~char_('\"')) > char_("\"")]
        |   lexeme[char_("\'") > *(~char_('\'')) > char_("\'")];
    BOOST_SPIRIT_DEFINE(quoted_string)

    x3::rule<class any_string_, std::string> const any_string = "any_string";
    auto const any_string_def = unquoted_string | quoted_string;
    BOOST_SPIRIT_DEFINE(any_string)

    x3::rule<class identifier_, std::string> const identifier = "identifier";
    auto const identifier_def = lexeme[(alnum | char_('_')) >> *(alnum | char_('_'))];
    BOOST_SPIRIT_DEFINE(identifier)

    x3::rule<class prop_name_, std::string> const prop_name = "prop_name";
    auto const prop_name_def = lexeme[identifier >> *(char_('.') >> identifier)];
    BOOST_SPIRIT_DEFINE(prop_name)

    x3::rule<class obj_name_, std::string> const obj_name = "obj_name";
    auto const obj_name_def = lexeme[(alnum | char_("/_")) >> *(alnum | char_("/_"))];
    BOOST_SPIRIT_DEFINE(obj_name)

    x3::rule<class unquoted_filename_, std::string> const unquoted_filename = "unquoted_filename";
    auto const unquoted_filename_def = lexeme[(alnum | char_("$/-_.")) >> *(alnum | char_("$/-_."))];
    BOOST_SPIRIT_DEFINE(unquoted_filename)

    x3::rule<class quoted_filename_filename_, std::string> const quoted_filename = "quoted_filename";
    auto const quoted_filename_def = 
            lexeme[char_("\"") > unquoted_filename > char_("\"")]
        |   lexeme[char_("\'") > unquoted_filename > char_("\'")];
    BOOST_SPIRIT_DEFINE(quoted_filename)

    x3::rule<class any_filename_, std::string> const any_filename = "any_filename";
    auto const any_filename_def = unquoted_filename | quoted_filename;
    BOOST_SPIRIT_DEFINE(any_filename)

    x3::rule<class int2_, Int2> const int2 = "int2";
    auto const int2_def = repeat(2) [ int_ ];
    BOOST_SPIRIT_DEFINE(int2)

    x3::rule<class int3_, Int3> const int3 = "int3";
    auto const int3_def = repeat(3) [ int_ ];
    BOOST_SPIRIT_DEFINE(int3)

    x3::rule<class int4_, Int4> const int4 = "int4";
    auto const int4_def = repeat(4) [ int_ ];
    BOOST_SPIRIT_DEFINE(int4)

    x3::rule<class vector2_, Vector2> const vector2 = "vector2";
    auto const vector2_def = repeat(2) [ double_ ];
    BOOST_SPIRIT_DEFINE(vector2)

    x3::rule<class vector3_, Vector3> const vector3 = "vector3";
    auto const vector3_def = repeat(3) [ double_ ];
    BOOST_SPIRIT_DEFINE(vector3)

    x3::rule<class vector4_, Vector4> const vector4 = "vector4";
    auto const vector4_def = repeat(4) [ double_ ];
    BOOST_SPIRIT_DEFINE(vector4)

    x3::rule<class matrix3_, Matrix3> const matrix3 = "matrix3";
    auto const matrix3_def = repeat(9) [ double_ ];
    BOOST_SPIRIT_DEFINE(matrix3)

    x3::rule<class matrix4_, Matrix4> const matrix4 = "matrix4";
    auto const matrix4_def = repeat(16) [ double_ ];
    BOOST_SPIRIT_DEFINE(matrix4)

    x3::rule<class version_, Version> const version = "version";
    auto const version_def = lexeme[-lexeme["VEX"] >> int_ >> "." >> int_ >> "." >> int_];
    BOOST_SPIRIT_DEFINE(version)

    x3::rule<class prop_values_, std::vector<PropValue>> const prop_values = "prop_values";
    auto const prop_values_def = *(int_ | double_ | any_string);
    BOOST_SPIRIT_DEFINE(prop_values)

    x3::rule<class image_values_, std::vector<std::string>> const image_values = "image_values";
    auto const image_values_def = *(quoted_string);
    BOOST_SPIRIT_DEFINE(image_values)


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

    struct PropTypesTable : x3::symbols<ast::Type> {
        PropTypesTable() {
            add ("float"    , ast::Type::FLOAT)
                ("bool"     , ast::Type::BOOL)
                ("int"      , ast::Type::INT)
                ("vector2"  , ast::Type::VECTOR2)
                ("vector3"  , ast::Type::VECTOR3)
                ("vector4"  , ast::Type::VECTOR4)
                ("matrix3"  , ast::Type::MATRIX3)
                ("matrix4"  , ast::Type::MATRIX4)
                ("string"   , ast::Type::STRING);
        }
    } const prop_type;

    //x3::rule<class object_, ast::Object> const object = "object";
    //auto const object_def = lexeme["global" | "geo" | "geometry"];
    //BOOST_SPIRIT_DEFINE(object)

    using boost::fusion::at_c;
    //auto assign_objtype = [](auto& ctx) { _val(ctx).type = _attr(ctx); };
    //auto assign_time = [](auto& ctx) { _val(ctx).time = _attr(ctx); };
    //auto assign_version = [](auto& ctx) { 
    //    _val(ctx).version[0] = at_c<0>(_attr(ctx)); 
    //    _val(ctx).version[1] = at_c<1>(_attr(ctx));
    //    _val(ctx).version[2] = at_c<2>(_attr(ctx));
    //};
    auto assign_comment = [](auto& ctx) {};
    auto assign_prop_value = [](auto& ctx) { std::cout << "PROP: " << _attr(ctx); };

    auto const comment
        = x3::rule<class comment, ast::comment>{"comment"}
        = lexeme[ 
        "/*" >> *(char_ - "*/") >> "*/"
        | "//" >> *~char_("\r\n") >> eol
        | '#' >> *(char_)
    ][assign_comment];

    auto const setenv
        = x3::rule<class setenv, ast::setenv>{"setenv"}
        = "setenv" >> identifier >> "=" >> any_string;

    auto const cmd_image
        = x3::rule<class cmd_image, ast::cmd_image>{"cmd_image"}
        = "cmd_image" >> image_values;

    auto const cmd_property
        = x3::rule<class cmd_property, ast::cmd_property>{"cmd_property"}
        = "cmd_property" >> object >> identifier >> prop_values;

    auto const cmd_declare
        = x3::rule<class cmd_declare, ast::cmd_declare>{"cmd_declare"}
        = "cmd_declare" >> object >> prop_type >> prop_name >> prop_values;

    auto const cmd_transform
        = x3::rule<class cmd_transform, ast::cmd_transform>{"cmd_transform"}
        = "cmd_transform" >> matrix4;

    auto const cmd_start
        = x3::rule<class cmd_start, ast::cmd_start>{"cmd_start"}
        = "cmd_start" >> object;// [assign_objtype];

    auto const cmd_time
        = x3::rule<class cmd_time, ast::cmd_time>{"cmd_time"}
        = "cmd_time" >> float_;// [assign_time];

    auto const cmd_version
        = x3::rule<class cmd_version, ast::cmd_version>{"cmd_version"}
        = "cmd_version" >> version;// [assign_version];

    auto const cmd_defaults
        = x3::rule<class cmd_defaults, ast::cmd_defaults>{"cmd_defaults"}
        = "cmd_defaults" >> any_filename;

    auto const cmd_detail
        = x3::rule<class cmd_detail, ast::cmd_detail>{"cmd_detail"}
        = "cmd_detail" >> obj_name >> any_filename;

    auto const cmd_geometry
        = x3::rule<class cmd_geometry, ast::cmd_geometry>{"cmd_geometry"}
        = "cmd_geometry" >> obj_name;

    auto const cmd_raytrace
        = x3::rule<class cmd_raytrace, ast::cmd_raytrace>{"cmd_raytrace"}
        = "cmd_raytrace" >> eps;

    auto const cmd_quit
        = x3::rule<class cmd_quit, ast::cmd_quit>{"cmd_quit"}
        = "cmd_quit" >> eps;

    auto const cmd_end
        = x3::rule<class cmd_end, ast::cmd_end>{"cmd_end"}
        = "cmd_end" >> eps;

    auto cmd = comment | setenv | cmd_image | cmd_time | cmd_version | cmd_defaults | cmd_end | cmd_quit | cmd_start | 
        cmd_transform | cmd_detail | cmd_geometry | cmd_property | cmd_raytrace | cmd_declare;
    
    auto const input  = skip(blank) [*(cmd) % eol];
    
}}  // namespace lsd::parser

}  // namespace lava

#endif  // SRC_LAVA_LIB_GRAMMAR_LSD_H_