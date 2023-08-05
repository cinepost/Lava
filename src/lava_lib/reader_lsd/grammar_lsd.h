#ifndef SRC_LAVA_LIB_GRAMMAR_LSD_H_
#define SRC_LAVA_LIB_GRAMMAR_LSD_H_

#include <array>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include <variant>

#define BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS
#define BOOST_MPL_LIMIT_LIST_SIZE 30

#include "boost/array.hpp"
#include "boost/filesystem.hpp"
#include "boost/range.hpp"
//#include <boost/range/join.hpp>
//#include <boost/algorithm/string/join.hpp>

#ifdef DEBUG
   // #define BOOST_SPIRIT_X3_DEBUG
#endif

#include "boost/spirit/home/x3.hpp"
#include "boost/spirit/home/x3/support/ast/variant.hpp"
#include "boost/spirit/home/x3/support/traits/container_traits.hpp"
#include "boost/spirit/include/support_istream_iterator.hpp"
#include "boost/fusion/include/adapt_struct.hpp"
#include "boost/fusion/include/io.hpp"
#include "boost/fusion/sequence/io.hpp"
#include "boost/fusion/include/io.hpp"
#include "boost/fusion/adapted/array.hpp"
#include "boost/fusion/adapted/std_pair.hpp"

#include "boost/container/static_vector.hpp"

#include "Falcor/Utils/Math/Vector.h"
#include "Falcor/Scene/MaterialX/MxTypes.h"

#include "grammar_bgeo.h"
#include "grammar_lsd_expr.h"

#include "../display.h"

#include "boost/log/core.hpp"
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
    
    typedef double Float;

    typedef static_vector<int, 2> Int2;
    typedef static_vector<int, 3> Int3;
    typedef static_vector<int, 4> Int4; 

    typedef static_vector<uint, 2> Uint2;
    typedef static_vector<uint, 3> Uint3;
    typedef static_vector<uint, 4> Uint4; 

    typedef static_vector<double, 2> Vector2; 
    typedef static_vector<double, 3> Vector3;
    typedef static_vector<double, 4> Vector4; 
    typedef static_vector<double, 9> Matrix3;
    typedef static_vector<double, 16> Matrix4;
    typedef x3::variant<bool, int, Int2, Int3, Int4, double, Vector2, Vector3, Vector4, std::string> PropValue;

    static inline Falcor::int2 to_int2(const Int2& vec) {
        return {vec[0], vec[1]};
    }

    static inline Falcor::uint2 to_uint2(const Int2& vec) {
        return {vec[0], vec[1]};
    }

    static inline Falcor::int3 to_int3(const Int3& vec) {
        return {vec[0], vec[1], vec[2]};
    }

    static inline Falcor::uint3 to_uint3(const Int3& vec) {
        return {vec[0], vec[1], vec[2]};
    }

    static inline Falcor::int4 to_int4(const Int4& vec) {
        return {vec[0], vec[1], vec[2], vec[3]};
    }

    static inline Falcor::uint4 to_uint4(const Int4& vec) {
        return {vec[0], vec[1], vec[2], vec[3]};
    }

    static inline Falcor::float2 to_float2(const Vector2& vec) {
        return {vec[0], vec[1]};
    }

    static inline Falcor::float3 to_float3(const Vector3& vec) {
        return {vec[0], vec[1], vec[2]};
    }

    static inline Falcor::float4 to_float4(const Vector4& vec) {
        return {vec[0], vec[1], vec[2], vec[3]};
    }

    static inline glm::mat4 to_mat4(const Matrix4& m) {
        return {m[0], m[1], m[2], m[3],
                m[4], m[5], m[6], m[7],
                m[8], m[9], m[10],m[11],
                m[12],m[13],m[14],m[15]
        };
    }

namespace ast {

    enum class Type { FLOAT, BOOL, INT, INT2, INT3, INT4, VECTOR2, VECTOR3, VECTOR4, MATRIX3, MATRIX4, STRING, UNKNOWN };
    enum class Style { GLOBAL, MATERIAL, NODE, GEO, GEOMETRY, SEGMENT, CAMERA, LIGHT, FOG, OBJECT, INSTANCE, PLANE, IMAGE, RENDERER, UNKNOWN };
    enum class EmbedDataType { TEXTURE, UNKNOWN };
    enum class EmbedDataEncoding { UUENCODED, UNKNOWN };
    enum class IPRMode { GENERATE, UPDATE };

    typedef lava::Display::DisplayType DisplayType;
    
    struct ifthen;
    struct endif;
    struct setenv;
    struct cmd_time;
    struct cmd_iprmode;
    struct cmd_version;
    struct cmd_config;
    struct cmd_defaults;
    struct cmd_transform;
    struct cmd_mtransform;
    struct cmd_quit;
    struct cmd_start;
    struct cmd_end;
    struct cmd_edge;
    struct cmd_detail;
    struct cmd_geometry;
    struct cmd_procedural;
    struct cmd_property;
    struct cmd_raytrace;
    struct cmd_socket;
    struct cmd_image;
    struct cmd_declare;
    struct cmd_deviceoption;
    struct cmd_reset;
    struct ray_embeddedfile;


    struct NoValue {
        bool operator==(NoValue const &) const { return true; }
    };

    typedef x3::variant<
        //NoValue,
        ifthen,
        endif,
        setenv,
        cmd_start,
        cmd_time,
        cmd_iprmode,
        cmd_version,
        cmd_config,
        cmd_defaults,
        cmd_transform,
        cmd_mtransform,
        cmd_end,
        cmd_edge,
        cmd_quit,
        cmd_detail,
        cmd_geometry,
        cmd_procedural,
        cmd_property,
        cmd_raytrace,
        cmd_socket,
        cmd_image,
        cmd_declare,
        cmd_deviceoption,
        cmd_reset,
        ray_embeddedfile
    > Command;

    // nullary commands
    struct cmd_end { };
    struct cmd_quit { };
    struct cmd_raytrace { };
    struct endif { };

    // non-nullary commands
    struct ifthen{
        expr::ast::Expr expr;
    };

    struct setenv {
        std::string key;
        std::string value;
    };

    struct cmd_time {
        double time;
    };

    struct cmd_iprmode {
        IPRMode mode;
        bool stash;
    };

    struct cmd_start {
        Style object_type;
    };

    struct cmd_transform {
        Matrix4 m;
    };

    struct cmd_mtransform {
        Matrix4 m;
    };

    struct cmd_version {
        Version version;
    };

    struct cmd_defaults {
        std::string filename;
    };

    struct cmd_config {
        Type prop_type;
        std::string prop_name;
        PropValue prop_value;
    };

    struct cmd_socket {
        Falcor::MxSocketDirection direction;
        Falcor::MxSocketDataType  data_type;
        std::string name;
    };

    struct cmd_geometry {
        std::string geometry_name;
    };

    struct cmd_procedural {
        Vector3 bbox_min;
        Vector3 bbox_max;
        std::string procedural;
        std::vector<std::pair<std::string, PropValue>> arguments;
    };

    struct cmd_detail {
        Vector3 preblur;
        Vector3 postblur;
        bool temporary;
        std::string name;
        std::string filename;
    };

    struct cmd_image {
        DisplayType display_type;
        std::string filename;
    };

    struct cmd_edge {
        std::string src_node_uuid;
        std::string src_node_output_socket;
        std::string dst_node_uuid;
        std::string dst_node_input_socket;
    };

    struct cmd_property {
        Style style;
        std::vector<std::pair<std::string, PropValue>> values;
    };

    struct cmd_deviceoption {
        Type type;
        std::string name;
        std::vector<PropValue> values;
    };

    struct cmd_declare {
        uint array_size;
        Style style;
        Type type;
        std::string token;
        std::vector<PropValue> values;
    };

    struct cmd_reset {
        bool lights = false;
        bool objects = false;
        bool fogs = false;
    };

    struct ray_embeddedfile {
        EmbedDataType   type = EmbedDataType::UNKNOWN;
        std::string     name;
        EmbedDataEncoding encoding = EmbedDataEncoding::UNKNOWN;
        size_t          size;
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

static inline std::ostream& operator<<(std::ostream& os, const Version& v) {
    return os << "Version: " << v[0] << "." << v[1] << "." << v[2];
};

static inline std::ostream& operator<<(std::ostream& os, const PropValue& v) {
    os << v;
    return os;
};

static inline std::ostream& operator<<(std::ostream& os, std::vector<PropValue> v) {
    //std::copy(v.begin(), v.end(), std::ostream_iterator<PropValue>(os, " "));
    return os;
};

using EmbedDataType = lava::lsd::ast::EmbedDataType;
static inline std::string to_string(const lava::lsd::ast::EmbedDataType& t) {
    switch(t) {
        case EmbedDataType::TEXTURE: return "texture";
        default: return "unknown";
    }
}

using Type = lava::lsd::ast::Type;
static inline std::string to_string(const lava::lsd::ast::Type& t) {
    switch(t) {
        case Type::INT: return "int";
        case Type::INT2: return "int2";
        case Type::INT3: return "int3";
        case Type::INT4: return "int4";
        case Type::BOOL: return "bool";
        case Type::FLOAT: return "float";
        case Type::STRING: return "string";
        case Type::VECTOR2: return "vector2";
        case Type::VECTOR3: return "vector3";
        case Type::VECTOR4: return "vector4";
        case Type::MATRIX3: return "matrix3";
        case Type::MATRIX4: return "matrix4";
        default: return "unknown";
    }
}


using Style = lava::lsd::ast::Style;
static inline std::string to_string(const lava::lsd::ast::Style& s) {
    switch(s) {
        case Style::GLOBAL: return "global";
        case Style::GEO: return "geo";
        case Style::GEOMETRY: return "geometry";
        case Style::MATERIAL: return "material";
        case Style::NODE: return "node";
        case Style::SEGMENT: return "segment";
        case Style::CAMERA: return "camera";
        case Style::LIGHT: return "light";
        case Style::FOG: return "fog";
        case Style::OBJECT: return "object";
        case Style::INSTANCE: return "instance";
        case Style::PLANE: return "plane";
        case Style::IMAGE: return "image";
        case Style::RENDERER: return "renderer";
        default: return "unknown";
    }
}

using IPRMode = lava::lsd::ast::IPRMode;
static inline std::string to_string(const ast::IPRMode& mode) {
    switch(mode) {
        case IPRMode::GENERATE: return "generate";
        default: return "update";
    }
}

static inline std::ostream& operator<<(std::ostream& os, EmbedDataType t) {
    return os << to_string(t);
};

static inline std::ostream& operator<<(std::ostream& os, IPRMode mode) {
    return os << to_string(mode);
};

static inline std::ostream& operator<<(std::ostream& os, Type t) {
    return os << to_string(t);
};

static inline std::ostream& operator<<(std::ostream& os, Style s) {
    return os << to_string(s);
};

}  // namespace lsd

}  // namespace lava

BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::endif)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_end)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_quit)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_raytrace)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::ifthen, expr)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::setenv, key, value)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_time, time)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_start, object_type)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_transform, m)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_mtransform, m)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_image, display_type, filename)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_iprmode, mode, stash)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_defaults, filename)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_config, prop_type, prop_name, prop_value)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_version, version)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_detail, preblur, postblur, temporary, name, filename)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_geometry, geometry_name)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_procedural, bbox_min, bbox_max, procedural, arguments)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_property, style, values)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_deviceoption, type, name, values)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_socket, direction, data_type, name)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_declare, array_size, style, type, token, values)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_reset)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::cmd_edge, src_node_uuid, src_node_output_socket, dst_node_uuid, dst_node_input_socket)
BOOST_FUSION_ADAPT_STRUCT(lava::lsd::ast::ray_embeddedfile, type, name, encoding, size)

namespace lava { 

namespace lsd { 

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
    
    auto const esc_char 
        = x3::rule<struct esc_char_, char> {"esc_char"}
        = '\\' >> char_("\"");

    auto const string 
        = x3::rule<struct string_, std::string> {"string"}
        = lexeme[+graph];

    auto const string_char
        = esc_char | alnum | char_('-') | char_('"') | char_('$') | char_('/') | char_('_') | char_('.') | 
         char_(':') | char_('+') | char_('@') | char_('!') | char_('~') | char_('?');

    x3::rule<class unquoted_string_, std::string> const unquoted_string = "unquoted_string";
    auto const unquoted_string_def = lexeme[+string_char];
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
    auto const prop_name_def = lexeme[identifier >> *(char_(".:/_") >> identifier)];
    BOOST_SPIRIT_DEFINE(prop_name)

    x3::rule<class obj_name_, std::string> const obj_name = "obj_name";
    auto const obj_name_def = lexeme[(alnum | char_(".:/_")) >> *(alnum | char_("./_"))];
    BOOST_SPIRIT_DEFINE(obj_name)

    x3::rule<class unquoted_edge_path_, std::string> const unquoted_edge_path = "unquoted_edge_path";
    auto const unquoted_edge_path_def = lexeme[(alnum | char_(".:/_")) >> *(alnum | char_(".:/_"))];
    BOOST_SPIRIT_DEFINE(unquoted_edge_path)

    x3::rule<class quoted_edge_path_, std::string> const quoted_edge_path = "quoted_edge_path";
    auto const quoted_edge_path_def = lexeme['"' > unquoted_edge_path > '"'];
    BOOST_SPIRIT_DEFINE(quoted_edge_path)

    x3::rule<class any_edge_path_, std::string> const any_edge_path = "any_edge_path";
    auto const any_edge_path_def = quoted_edge_path | unquoted_edge_path;
    BOOST_SPIRIT_DEFINE(any_edge_path)


    x3::rule<class unquoted_filename_, std::string> const unquoted_filename = "unquoted_filename";
    auto const unquoted_filename_def = lexeme[string_char >> *(string_char)];
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
      | any_string;
    BOOST_SPIRIT_DEFINE(prop_value)

    auto const keyword
        = x3::rule<class keyword>{"keyword"}
        = x3::lit("setenv") | lit("cmd_time") | lit("cmd_property") | lit("cmd_image") | lit("cmd_transform") | lit("cmd_end") | lit("cmd_detail") | lit("cmd_deviceoption") | lit("cmd_start")
        | lit("cmd_version") | lit("cmd_defaults") | lit("cmd_declare") | lit("cmd_config") | lit("cmd_mtransform") | lit("cmd_reset") | lit("cmd_iprmode") | lit("ray_embeddedfile")
        | lit("cmd_edge") | lit("cmd_procedural");

    x3::rule<class prop_values_, std::vector<PropValue>> const prop_values = "prop_values";
    auto const prop_values_def = *(prop_value - keyword);
    BOOST_SPIRIT_DEFINE(prop_values)

    x3::rule<class prop_values_array_, std::vector<std::pair<std::string, PropValue>>> const prop_values_array = "prop_values_array";
    auto const prop_values_array_def = *(prop_name >> prop_value) - keyword;
    BOOST_SPIRIT_DEFINE(prop_values_array)

    x3::rule<class image_values_, std::vector<std::string>> const image_values = "image_values";
    auto const image_values_def = *(quoted_string);
    BOOST_SPIRIT_DEFINE(image_values)


    struct ObjectTypesTable : x3::symbols<ast::Style> {
        ObjectTypesTable() {
            add ("geo"      , ast::Style::GEO)
                ("material" , ast::Style::MATERIAL)
                ("node"     , ast::Style::NODE)
                ("light"    , ast::Style::LIGHT)
                ("fog"      , ast::Style::FOG)
                ("object"   , ast::Style::OBJECT)
                ("instance" , ast::Style::INSTANCE)
                ("segment"  , ast::Style::SEGMENT)
                ("plane"    , ast::Style::PLANE);
        }
    } const object_type;

    struct PrpertyStylesTable : x3::symbols<ast::Style> {
        PrpertyStylesTable() {
            add ("global"   , ast::Style::GLOBAL)
                ("geo"      , ast::Style::GEO)
                ("geometry" , ast::Style::GEOMETRY)
                ("material" , ast::Style::MATERIAL)
                ("node"     , ast::Style::NODE)
                ("segment"  , ast::Style::SEGMENT)
                ("camera"   , ast::Style::CAMERA)
                ("light"    , ast::Style::LIGHT)
                ("fog"      , ast::Style::FOG)
                ("object"   , ast::Style::OBJECT)
                ("instance" , ast::Style::INSTANCE)
                ("plane"    , ast::Style::PLANE)
                ("image"    , ast::Style::IMAGE)
                ("renderer" , ast::Style::RENDERER);
        }
    } const property_style;

    struct DeclareStylesTable : x3::symbols<ast::Style> {
        DeclareStylesTable() {
            add ("global"   , ast::Style::GLOBAL)
                ("geometry" , ast::Style::GEOMETRY)
                ("light"    , ast::Style::LIGHT)
                ("object"   , ast::Style::OBJECT)
                ("material" , ast::Style::MATERIAL)
                ("node"     , ast::Style::NODE)
                ("image"    , ast::Style::IMAGE)
                ("plane"    , ast::Style::PLANE);
        }
    } const declare_style;

    struct MxSocketDirectionTable : x3::symbols<Falcor::MxSocketDirection> {
        MxSocketDirectionTable() {
            add ("input"   , Falcor::MxSocketDirection::INPUT)
                ("output"  , Falcor::MxSocketDirection::OUTPUT);
        }
    } const socket_direction;

    struct PropTypesTable : x3::symbols<ast::Type> {
        PropTypesTable() {
            add ("float"    , ast::Type::FLOAT)
                ("bool"     , ast::Type::BOOL)
                ("int"      , ast::Type::INT)
                ("int2"     , ast::Type::INT2)
                ("int3"     , ast::Type::INT3)
                ("int4"     , ast::Type::INT4)
                ("vector"   , ast::Type::VECTOR3)
                ("vector2"  , ast::Type::VECTOR2)
                ("vector3"  , ast::Type::VECTOR3)
                ("vector4"  , ast::Type::VECTOR4)
                ("matrix3"  , ast::Type::MATRIX3)
                ("matrix4"  , ast::Type::MATRIX4)
                ("string"   , ast::Type::STRING);
        }
    } const prop_type;

    struct IPRModesTable : x3::symbols<ast::IPRMode> {
        IPRModesTable() {
            add ("generate"    , ast::IPRMode::GENERATE)
                ("update"     ,  ast::IPRMode::UPDATE);
        }
    } const ipr_mode;

    struct MxSocketDataTypeTable : x3::symbols<Falcor::MxSocketDataType> {
        MxSocketDataTypeTable() {
            add ("float"        , Falcor::MxSocketDataType::FLOAT)
                ("bool"         , Falcor::MxSocketDataType::BOOL)
                ("int"          , Falcor::MxSocketDataType::INT)
                ("int2"         , Falcor::MxSocketDataType::INT2)
                ("int3"         , Falcor::MxSocketDataType::INT3)
                ("int4"         , Falcor::MxSocketDataType::INT4)
                ("vector"       , Falcor::MxSocketDataType::VECTOR3)
                ("vector2"      , Falcor::MxSocketDataType::VECTOR2)
                ("vector3"      , Falcor::MxSocketDataType::VECTOR3)
                ("vector4"      , Falcor::MxSocketDataType::VECTOR4)
                ("matrix3"      , Falcor::MxSocketDataType::MATRIX3)
                ("matrix4"      , Falcor::MxSocketDataType::MATRIX4)
                ("string"       , Falcor::MxSocketDataType::STRING)
                ("surface"      , Falcor::MxSocketDataType::DISPLACEMENT)
                ("displacement" , Falcor::MxSocketDataType::SURFACE)
                ("bsdf"         , Falcor::MxSocketDataType::BSDF);
                ;
        }
    } const socket_data_type;


    struct DisplayTypesTable : x3::symbols<ast::DisplayType> {
        DisplayTypesTable() {
            add ("\"ip\""       , ast::DisplayType::IP)
                ("\"md\""       , ast::DisplayType::MD)
                ("\"houdini\""  , ast::DisplayType::HOUDINI)
                ("\"idisplay\"" , ast::DisplayType::IDISPLAY)
                ("\"sdl\""      , ast::DisplayType::SDL)

                ("\"jpeg\""     , ast::DisplayType::JPEG)
                ("\"JPEG\""     , ast::DisplayType::JPEG)

                ("\"png\""      , ast::DisplayType::PNG)
                ("\"PNG\""      , ast::DisplayType::PNG)

                ("\"exr\""      , ast::DisplayType::OPENEXR)
                ("\"OpenEXR\""  , ast::DisplayType::OPENEXR)

                ("\"tiff\""     , ast::DisplayType::TIFF)
                ("\"TIFF\""     , ast::DisplayType::TIFF)

                ("\"null\""     , ast::DisplayType::NUL)
                ;
        }
    } const display_type;

    struct EmbeddedDataTypeTable : x3::symbols<ast::EmbedDataType> {
        EmbeddedDataTypeTable() {
            add ("texture"   , ast::EmbedDataType::TEXTURE);
        }
    } const embedded_data_type;

    struct EmbeddedDataEncodingTable : x3::symbols<ast::EmbedDataEncoding> {
        EmbeddedDataEncodingTable() {
            add ("binary-uu"   , ast::EmbedDataEncoding::UUENCODED);
        }
    } const embedded_data_encoding;

    using boost::fusion::at_c;

    auto reset_lights = [](auto& ctx) { 
        _val(ctx).lights = true; 
    };

    auto reset_objects = [](auto& ctx) { 
        _val(ctx).objects = true; 
    };

    auto reset_fogs = [](auto& ctx) { 
        _val(ctx).fogs = true; 
    };

    static auto const skipper = lexeme[ 
        "/*" >> *(char_ - "*/") >> "*/"
        | "//" >> *~char_("\r\n")
        | '#' >> *~char_("\r\n")
        | blank
    ] | blank;

    auto const null_vector = x3::attr("0.0 0.0 0.0");
    
    x3::rule<class null_vector3_, Vector3> const null_vector3 = "null_vector3";
    auto const null_vector3_def = attr(Vector3{0.0, 0.0, 0.0});
    BOOST_SPIRIT_DEFINE(null_vector3)

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
        = "cmd_property" >> property_style >> prop_values_array;

    auto const cmd_procedural
        = x3::rule<class cmd_procedural, ast::cmd_procedural>{"cmd_procedural"}
        = "cmd_procedural" >> lit("-m") >> vector3 >> lit("-M") >> vector3 >> any_string >> prop_values_array |
          "cmd_procedural" >> null_vector3 >> null_vector3 >> any_string >> prop_values_array;

    auto const cmd_deviceoption
        = x3::rule<class cmd_deviceoption, ast::cmd_deviceoption>{"cmd_deviceoption"}
        = "cmd_deviceoption" >> prop_type >> prop_name >> prop_values;

    auto const cmd_declare
        = x3::rule<class cmd_declare, ast::cmd_declare>{"cmd_declare"}
        = "cmd_declare" >> lit("-v") >> int_ >> declare_style >> prop_type >> prop_name >> prop_values |
          "cmd_declare" >> attr(0) >> declare_style >> prop_type >> prop_name >> prop_values;

    auto const cmd_socket
        = x3::rule<class cmd_socket, ast::cmd_socket>{"cmd_socket"}
        = "cmd_socket" >> socket_direction >> socket_data_type >> any_string >> eps;

    auto const cmd_reset
        = x3::rule<class cmd_reset, ast::cmd_reset>{"cmd_reset"}
        = "cmd_reset" >> *(lit("-l")[reset_lights] | lit("-o")[reset_objects] | lit("-f")[reset_fogs]);

    auto const cmd_transform
        = x3::rule<class cmd_transform, ast::cmd_transform>{"cmd_transform"}
        = "cmd_transform" >> matrix4 >> eps;

    auto const cmd_mtransform
        = x3::rule<class cmd_mtransform, ast::cmd_mtransform>{"cmd_mtransform"}
        = "cmd_mtransform" >> matrix4 >> eps;

    auto const cmd_start
        = x3::rule<class cmd_start, ast::cmd_start>{"cmd_start"}
        = "cmd_start" >> object_type >> eps;

    auto const cmd_time
        = x3::rule<class cmd_time, ast::cmd_time>{"cmd_time"}
        = "cmd_time" >> float_ >> eps;

    auto const cmd_edge
        = x3::rule<class cmd_edge, ast::cmd_edge>{"cmd_edge"}
        = "cmd_edge" >> any_string >> any_string >> any_string >> any_string >> eps;

    auto const cmd_version
        = x3::rule<class cmd_version, ast::cmd_version>{"cmd_version"}
        = "cmd_version" >> version >> eps;

    auto const cmd_config
        = x3::rule<class cmd_config, ast::cmd_config>{"cmd_config"}
        = "cmd_config" >> prop_type >> prop_name >> prop_value >> eps;

    auto const cmd_iprmode
        = x3::rule<class cmd_iprmode, ast::cmd_iprmode>{"cmd_iprmode"}
        = "cmd_iprmode" >> ipr_mode >> lit("-s") >> attr(true)
        | "cmd_iprmode" >> ipr_mode >> attr(false);

    auto const cmd_defaults
        = x3::rule<class cmd_defaults, ast::cmd_defaults>{"cmd_defaults"}
        = "cmd_defaults" >> any_filename >> eps;

    auto const cmd_detail
        = x3::rule<class cmd_detail, ast::cmd_detail>{"cmd_detail"}
        = "cmd_detail" >> null_vector3 >> null_vector3 >> lit("-T") >> attr(true) >> obj_name >> any_filename
        | "cmd_detail" >> null_vector3 >> null_vector3 >> attr(false) >> obj_name >> any_filename
        | "cmd_detail" >> lit("-v") >> null_vector3 >> vector3 >> attr(false) >> obj_name >> obj_name
        | "cmd_detail" >> lit("-V") >> vector3 >> vector3 >>  attr(false) >> obj_name >> obj_name;

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

    auto const ifthen
        = x3::rule<class ifthen, ast::ifthen>{"ifthen"}
        = "if" >> lsd_expr >> "then";

    auto const endif
        = x3::rule<class endif, ast::endif>{"endif"}
        = "endif" >> eps;

    auto const ray_embeddedfile
        = x3::rule<class ray_embeddedfile, ast::ray_embeddedfile>{"ray_embeddedfile"}
        = "ray_embeddedfile" >> embedded_data_type >> any_string >> embedded_data_encoding >> int_;

    auto const cmd = setenv | cmd_image | cmd_time | cmd_iprmode | cmd_version | cmd_config | cmd_defaults | cmd_end | cmd_quit | cmd_start | cmd_reset | cmd_edge |
        cmd_socket |
        cmd_transform | cmd_mtransform | cmd_detail | cmd_geometry | cmd_property | cmd_raytrace | cmd_declare | cmd_deviceoption | ray_embeddedfile |
        ifthen | endif;

    auto const input  = skip(skipper) [*cmd % eol];

}}  // namespace lsd::parser

}  // namespace lava

#endif  // SRC_LAVA_LIB_GRAMMAR_LSD_H_