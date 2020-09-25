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

//#include "renderer_iface_lsd.h"
#include "grammar_bgeo.h"
#include "grammar_lsd_expr.h"

#include "lava_utils_lib/logging.h"

namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;

namespace lava {

namespace lsd {
    using boost::container::static_vector;

    struct NoValue {
        bool operator==(NoValue const &) const { return true; }
    };

    typedef static_vector<uint, 3> Version;
    
    typedef static_vector<int, 2> Int2;
    typedef static_vector<int, 3> Int3;
    typedef static_vector<int, 4> Int4; 

    typedef static_vector<double, 2> Vector2; 
    typedef static_vector<double, 3> Vector3;
    typedef static_vector<double, 4> Vector4; 
    typedef static_vector<double, 9> Matrix3;
    typedef static_vector<double, 16> Matrix4;
    typedef x3::variant<int, Int2, Int3, Int4, double, Vector2, Vector3, Vector4, std::string> PropValue;

namespace ast {

    //using boost::fusion::operators::operator>>; // for input
    //using boost::fusion::operators::operator<<; // for output

    enum class Type { FLOAT, BOOL, INT, VECTOR2, VECTOR3, VECTOR4, MATRIX3, MATRIX4, STRING };
    enum class Object { GLOBAL, MATERIAL, GEO, GEOMERTY, SEGMENT, CAMERA, LIGHT, FOG, OBJECT, INSTANCE, PLANE, IMAGE, RENDERER };
    enum class DisplayType { NONE, IP, MD, OPENEXR, JPEG, TIFF, PNG };

    struct ifthen;
    struct setenv;
    struct cmd_time;
    struct cmd_version;
    struct cmd_config;
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
    struct cmd_deviceoption;

    struct NoValue {
        bool operator==(NoValue const &) const { return true; }
    };

    typedef x3::variant<
        NoValue,
        ifthen,
        setenv,
        cmd_start,
        cmd_time,
        cmd_version,
        cmd_config,
        cmd_defaults,
        cmd_transform,
        cmd_end,
        cmd_quit,
        cmd_detail,
        cmd_geometry,
        cmd_property,
        cmd_raytrace,
        cmd_image,
        cmd_declare,
        cmd_deviceoption
    > Command;

    // nullary commands
    struct cmd_end { };
    struct cmd_quit { };
    struct cmd_raytrace { };

    // non-nullary commands
    struct ifthen{
        expr::ast::Expr expr;
        std::vector<Command> commands;
    };

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

    struct cmd_config {
        std::string filename;
    };

    struct cmd_geometry {
        std::string geometry_object;
    };

    struct cmd_detail {
        bool temporary;
        std::string name;
        std::string filename;
        bgeo::ast::Bgeo bgeo;
    };

    struct cmd_image {
        DisplayType display_type;
        std::string filename;
    };

    struct cmd_property {
        Object style;
        std::string token;
        std::vector<PropValue> values;
    };

    struct cmd_deviceoption {
        Type type;
        std::string name;
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

BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::ifthen, expr, commands)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::setenv, key, value)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_end)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_quit)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_raytrace)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_time, time)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_start, type)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_transform, m)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_image, display_type, filename)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_defaults, filename)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_config, filename)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_version, version)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_detail, temporary, name, filename, bgeo)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_geometry, geometry_object)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_property, style, token, values)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_deviceoption, type, name, values)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_declare, style, type, token, values)

namespace lava { 

class RendererIfaceLSD;

namespace lsd { 

struct Visitor: public boost::static_visitor<> {
 public:
    Visitor(std::unique_ptr<RendererIfaceLSD>& iface);

    virtual void operator()(ast::NoValue const& c) const {};
    virtual void operator()(ast::ifthen const& c) const;
    virtual void operator()(ast::setenv const& c) const;
    virtual void operator()(ast::cmd_image const& c) const;
    virtual void operator()(ast::cmd_end const& c) const;
    virtual void operator()(ast::cmd_quit const& c) const;
    virtual void operator()(ast::cmd_start const& c) const;
    virtual void operator()(ast::cmd_time const& c) const;
    virtual void operator()(ast::cmd_detail const& c) const;
    virtual void operator()(ast::cmd_version const& c) const;
    virtual void operator()(ast::cmd_config const& c) const;
    virtual void operator()(ast::cmd_defaults const& c) const;
    virtual void operator()(ast::cmd_transform const& c) const;
    virtual void operator()(ast::cmd_geometry const& c) const;
    virtual void operator()(ast::cmd_property const& c) const;
    virtual void operator()(ast::cmd_deviceoption const& c) const;
    virtual void operator()(ast::cmd_declare const& c) const;
    virtual void operator()(ast::cmd_raytrace const& c) const;

 protected:
    std::unique_ptr<RendererIfaceLSD> mIface;
};


struct EchoVisitor: public Visitor {
 public:
    EchoVisitor(std::unique_ptr<RendererIfaceLSD>& iface);
    EchoVisitor(std::unique_ptr<RendererIfaceLSD>& iface, std::ostream& os);

    void operator()(ast::NoValue const& c) const override {};
    void operator()(ast::ifthen const& c) const override;
    void operator()(ast::setenv const& c) const override;
    void operator()(ast::cmd_image const& c) const override;
    void operator()(ast::cmd_end const& c) const override;
    void operator()(ast::cmd_quit const& c) const override;
    void operator()(ast::cmd_start const& c) const override;
    void operator()(ast::cmd_time const& c) const override;
    void operator()(ast::cmd_detail const& c) const override;
    void operator()(ast::cmd_version const& c) const override;
    void operator()(ast::cmd_config const& c) const override;
    void operator()(ast::cmd_defaults const& c) const override;
    void operator()(ast::cmd_transform const& c) const override;
    void operator()(ast::cmd_geometry const& c) const override;
    void operator()(ast::cmd_property const& c) const override;
    void operator()(ast::cmd_deviceoption const& c) const override;
    void operator()(ast::cmd_declare const& c) const override;
    void operator()(ast::cmd_raytrace const& c) const override;

 //private:
    void operator()(std::vector<PropValue> const& v) const;
    void operator()(int v) const;
    void operator()(double v) const;
    void operator()(std::string const& v) const;
    void operator()(Int2 const& v) const;
    void operator()(Int3 const& v) const;
    void operator()(Int4 const& v) const;
    void operator()(Vector2 const& v) const;
    void operator()(Vector3 const& v) const;
    void operator()(Vector4 const& v) const;
    void operator()(PropValue const& v) const;

 private:
    std::ostream& _os;
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

    //
    // Since a double parser also parses an integer, we will always get a double, even if the input is "12"
    // In order to prevent this, we need a strict double parser
    //
    boost::spirit::x3::real_parser<double, boost::spirit::x3::strict_real_policies<double> > const double_ = {};

    template <typename T> auto as = [](auto p) { return x3::rule<struct _, T> {} = p; };
    //auto const uintPair = as<ast::uintPair_t> ( uint_ >> '-' >> uint_       );
    //auto const uintObj  = as<ast::uintObj>    ( uintPair | uint_            );
    //auto const varVec   = as<ast::varVec>     ( '[' >> uintObj % ',' >> ']' );

    auto const esc_char 
        = x3::rule<struct esc_char_, char> {"esc_char"}
        = '\\' >> char_("\"");

    auto const string 
        = x3::rule<struct string_, std::string> {"string"}
        = lexeme[+graph];

    auto const string_char
        = esc_char | alnum | char_("$/_.:-+@!~");

    x3::rule<class unquoted_string_, std::string> const unquoted_string = "unquoted_string";
    auto const unquoted_string_def = //lexeme[+(~char_(" \"\'"))];
        lexeme[+string_char];
    BOOST_SPIRIT_DEFINE(unquoted_string)

    x3::rule<class empty_string_> const empty_string = "empty_string";
    auto const empty_string_def = (char_('"') >> char_('"')) | (char_('\'') >> char_('\''));
    BOOST_SPIRIT_DEFINE(empty_string)

    x3::rule<class quoted_string_, std::string> const quoted_string = "quoted_string";
    auto const quoted_string_def = 
        x3::lexeme['"' > *(esc_char | ~x3::char_('"')) > '"'] | 
        x3::lexeme['\'' > *(esc_char | ~x3::char_('\'')) > '\''] | lexeme[empty_string];

    BOOST_SPIRIT_DEFINE(quoted_string)

    x3::rule<class any_string_, std::string> const any_string = "any_string";
    auto const any_string_def = quoted_string | unquoted_string;
    BOOST_SPIRIT_DEFINE(any_string)

    x3::rule<class identifier_, std::string> const identifier = "identifier";
    auto const identifier_def = lexeme[(alnum | char_('_')) >> *(alnum | char_('_'))];
    BOOST_SPIRIT_DEFINE(identifier)

    x3::rule<class prop_name_, std::string> const prop_name = "prop_name";
    auto const prop_name_def = lexeme[identifier >> *(char_(".:/") >> identifier)];
    BOOST_SPIRIT_DEFINE(prop_name)

    x3::rule<class obj_name_, std::string> const obj_name = "obj_name";
    auto const obj_name_def = lexeme[(alnum | char_("/_")) >> *(alnum | char_("/_"))];
    BOOST_SPIRIT_DEFINE(obj_name)

    x3::rule<class unquoted_filename_, std::string> const unquoted_filename = "unquoted_filename";
    auto const unquoted_filename_def = lexeme[+string_char];
    BOOST_SPIRIT_DEFINE(unquoted_filename)

    x3::rule<class quoted_filename_filename_, std::string> const quoted_filename = "quoted_filename";
    auto const quoted_filename_def = 
        x3::lexeme['"' > *(esc_char | ~x3::char_('"')) > '"'] | 
        x3::lexeme['\'' > *(esc_char | ~x3::char_('\'')) > '\''] | lexeme[empty_string];
    BOOST_SPIRIT_DEFINE(quoted_filename)

    x3::rule<class any_filename_, std::string> const any_filename = "any_filename";
    auto const any_filename_def = quoted_filename | unquoted_filename;
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
    auto const vector2_def = repeat(2) [ double_ | int_ ];
    BOOST_SPIRIT_DEFINE(vector2)

    x3::rule<class vector3_, Vector3> const vector3 = "vector3";
    auto const vector3_def = repeat(3) [ double_ | int_ ];
    BOOST_SPIRIT_DEFINE(vector3)

    x3::rule<class vector4_, Vector4> const vector4 = "vector4";
    auto const vector4_def = repeat(4) [ double_ | int_ ];
    BOOST_SPIRIT_DEFINE(vector4)

    x3::rule<class matrix3_, Matrix3> const matrix3 = "matrix3";
    auto const matrix3_def = repeat(9) [ double_ | int_ ];
    BOOST_SPIRIT_DEFINE(matrix3)

    x3::rule<class matrix4_, Matrix4> const matrix4 = "matrix4";
    auto const matrix4_def = repeat(16) [ double_ | int_ ];
    BOOST_SPIRIT_DEFINE(matrix4)

    x3::rule<class version_, Version> const version = "version";
    auto const version_def = lexeme[-lexeme["VER"] >> int_ >> "." >> int_ >> "." >> int_];
    BOOST_SPIRIT_DEFINE(version)

    x3::rule<class bgeo_inline_, bgeo::ast::Bgeo> const bgeo_inline = "bgeo_inline";
    auto const bgeo_inline_def = bgeo::parser::input;
    BOOST_SPIRIT_DEFINE(bgeo_inline)

    x3::rule<class lsd_expr_, lsd::expr::ast::Expr> const lsd_expr = "lsd_expr";
    auto const lsd_expr_def = lsd::expr::parser::input;
    BOOST_SPIRIT_DEFINE(lsd_expr)

    using boost::fusion::at_c;
    auto assign_prop = [](auto& ctx) { 
        _val(ctx).push_back(PropValue(_attr(ctx)));
    };

    x3::rule<class prop_value_, PropValue> const prop_value = "prop_value";
    auto const prop_value_def = 
        vector4 | vector3 | vector2 | double_
      | int4 | int3 | int2 | int_
      | any_string ;
    BOOST_SPIRIT_DEFINE(prop_value)

    auto const keyword
        = x3::rule<class keyword>{"keyword"}
        = x3::lit("setenv") | lit("cmd_time") | lit("cmd_property") | lit("cmd_image") | lit("cmd_transform") | lit("cmd_end") | lit("cmd_detail") | lit("cmd_deviceoption") | lit("cmd_start")
        | lit("cmd_version") | lit("cmd_defaults") | lit("cmd_declare") | lit("cmd_config");

    x3::rule<class prop_values_, std::vector<PropValue>> const prop_values = "prop_values";
    auto const prop_values_def = *(prop_value - keyword);
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

        struct DisplayTypesTable : x3::symbols<ast::DisplayType> {
        DisplayTypesTable() {
            add ("\"ip\""       , ast::DisplayType::IP)
                ("\"md\""       , ast::DisplayType::MD)
                ("\"JPEG\""     , ast::DisplayType::JPEG)
                ("\"PNG\""      , ast::DisplayType::PNG)
                ("\"OpenEXR\""  , ast::DisplayType::OPENEXR)
                ("\"TIFF\""     , ast::DisplayType::TIFF)
                ;
        }
    } const display_type;

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
    //auto assign_comment = [](auto& ctx) {};
    //auto assign_prop_values = [](auto& ctx) { std::cout << "PROP: " << _attr(ctx); };
    //auto assign_bgeo = [](auto& ctx) { 
    //    std::cout << "BGEO!!!";
    //    _val(ctx).bgeo = _attr(ctx); 
    //};

    static auto const skipper = lexeme[ 
        "/*" >> *(char_ - "*/") >> "*/"
        | "//" >> *~char_("\r\n")
        | '#' >> *~char_("\r\n")
        | blank
    ] | blank;

    auto const setenv
        = x3::rule<class setenv, ast::setenv>{"setenv"}
        = "setenv" >> identifier >> "=" >> any_string >> eps;

    auto const cmd_image
        = x3::rule<class cmd_image, ast::cmd_image>{"cmd_image"}
        = "cmd_image" >> lit("\"-f\"") >> display_type >> any_filename >> eps
        | "cmd_image" >> display_type >> attr("") >> eps
        | "cmd_image" >> attr(ast::DisplayType::NONE) >> any_filename >> eps;

    auto const cmd_property
        = x3::rule<class cmd_property, ast::cmd_property>{"cmd_property"}
        = "cmd_property" >> object >> identifier >> prop_values;
    
    auto const cmd_deviceoption
        = x3::rule<class cmd_deviceoption, ast::cmd_deviceoption>{"cmd_deviceoption"}
        = "cmd_deviceoption" >> prop_type >> prop_name >> prop_values;

    auto const cmd_declare
        = x3::rule<class cmd_declare, ast::cmd_declare>{"cmd_declare"}
        = "cmd_declare" >> object >> prop_type >> prop_name >> prop_values;

    auto const cmd_transform
        = x3::rule<class cmd_transform, ast::cmd_transform>{"cmd_transform"}
        = "cmd_transform" >> matrix4 >> eps;

    auto const cmd_start
        = x3::rule<class cmd_start, ast::cmd_start>{"cmd_start"}
        = "cmd_start" >> object >> eps;

    auto const cmd_time
        = x3::rule<class cmd_time, ast::cmd_time>{"cmd_time"}
        = "cmd_time" >> float_ >> eps;

    auto const cmd_version
        = x3::rule<class cmd_version, ast::cmd_version>{"cmd_version"}
        = "cmd_version" >> version >> eps;

    auto const cmd_config
        = x3::rule<class cmd_config, ast::cmd_config>{"cmd_config"}
        = "cmd_config" >> any_filename >> eps;

    auto const cmd_defaults
        = x3::rule<class cmd_defaults, ast::cmd_defaults>{"cmd_defaults"}
        = "cmd_defaults" >> any_filename >> eps;

    auto const cmd_detail
        = x3::rule<class cmd_detail, ast::cmd_detail>{"cmd_detail"}
        = "cmd_detail" >> lit("-T") >> attr(true) >> obj_name >> any_filename >> attr(bgeo::ast::Bgeo())
        | "cmd_detail" >> attr(false) >> obj_name >> "stdin" >> attr("stdin") >> bgeo_inline
        | "cmd_detail" >> attr(false) >> obj_name >> any_filename >> attr(bgeo::ast::Bgeo());

    auto const cmd_geometry
        = x3::rule<class cmd_geometry, ast::cmd_geometry>{"cmd_geometry"}
        = "cmd_geometry" >> obj_name >> eps;

    auto const cmd_raytrace
        = x3::rule<class cmd_raytrace, ast::cmd_raytrace>{"cmd_raytrace"}
        = "cmd_raytrace" >> eps;

    auto const cmd_quit
        = x3::rule<class cmd_quit, ast::cmd_quit>{"cmd_quit"}
        = "cmd_quit" >> eps;

    auto const cmd_end
        = x3::rule<class cmd_end, ast::cmd_end>{"cmd_end"}
        = "cmd_end" >> eps;

    auto const cmd = setenv | cmd_image | cmd_time | cmd_version | cmd_config | cmd_defaults | cmd_end | cmd_quit | cmd_start | 
        cmd_transform | cmd_detail | cmd_geometry | cmd_property | cmd_raytrace | cmd_declare | cmd_deviceoption;
    
    auto const ifthen
        = x3::rule<class ifthen, ast::ifthen>{"ifthen"}
        = "if" >> lsd_expr >> "then" >> (+cmd)  >> "endif";

    auto const input  = skip(skipper | char_("\n\t")) [*(cmd | ifthen) % eol];

}}  // namespace lsd::parser

}  // namespace lava

#endif  // SRC_LAVA_LIB_GRAMMAR_LSD_H_