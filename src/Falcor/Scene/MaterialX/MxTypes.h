#ifndef SRC_FALCOR_SCENE_MATERIALX_MXTYPES_H_
#define SRC_FALCOR_SCENE_MATERIALX_MXTYPES_H_

#include <string>

namespace Falcor {

enum class MxSocketDirection {
  INPUT,
  OUTPUT,
};

enum class MxSocketDataType { 
  FLOAT, BOOL, INT, INT2, INT3, INT4, VECTOR2, VECTOR, 
  VECTOR3, VECTOR4, MATRIX3, MATRIX4, STRING, UNKNOWN, 
  SURFACE, DISPLACEMENT, BSDF, Count 
};


static inline std::string to_string(const MxSocketDataType& t) {
    switch(t) {
        case Falcor::MxSocketDataType::INT: return "int";
        case Falcor::MxSocketDataType::INT2: return "int2";
        case Falcor::MxSocketDataType::INT3: return "int3";
        case Falcor::MxSocketDataType::INT4: return "int4";
        case Falcor::MxSocketDataType::BOOL: return "bool";
        case Falcor::MxSocketDataType::FLOAT: return "float";
        case Falcor::MxSocketDataType::STRING: return "string";
        case Falcor::MxSocketDataType::VECTOR2: return "vector2";
        case Falcor::MxSocketDataType::VECTOR: return "vector";
        case Falcor::MxSocketDataType::VECTOR3: return "vector3";
        case Falcor::MxSocketDataType::VECTOR4: return "vector4";
        case Falcor::MxSocketDataType::MATRIX3: return "matrix3";
        case Falcor::MxSocketDataType::MATRIX4: return "matrix4";
        case Falcor::MxSocketDataType::SURFACE: return "surface";
        case Falcor::MxSocketDataType::DISPLACEMENT: return "displacement";
        case Falcor::MxSocketDataType::BSDF: return "bsdf";
        default: return "unknown";
    }
};

static inline std::string to_string(const MxSocketDirection& dir) {
    if( dir == MxSocketDirection::INPUT) return "input";
    return "output";
};
  
static inline std::ostream& operator<<(std::ostream& os, MxSocketDataType t) {
    return os << to_string(t);
};

static inline std::ostream& operator<<(std::ostream& os, MxSocketDirection d) {
    return os << to_string(d);
};

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_MATERIALX_MXTYPES_H_