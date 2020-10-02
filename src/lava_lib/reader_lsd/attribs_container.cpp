#include <variant>
#include <map>

#include "grammar_lsd.h"
#include "attribs_container.h"

#include "lava_utils_lib/logging.h"

namespace lava {

namespace lsd {

// TODO: try casting value to appropriate type here in case it differs
bool Attrib::create(Attrib::Type type, Attrib::Value& value, Attrib& attrib) {
    attrib.mType = type;
    attrib.mValue = value;
    if (type == ast::Type::BOOL && value.type() == typeid(bool)) return true;
    if (type == ast::Type::INT && value.type() == typeid(int)) return true;
    if (type == ast::Type::FLOAT && value.type() == typeid(double)) return true;
    if (type == ast::Type::VECTOR2 && value.type() == typeid(Vector2)) return true;
    if (type == ast::Type::VECTOR3 && value.type() == typeid(Vector3)) return true;
    if (type == ast::Type::VECTOR4 && value.type() == typeid(Vector4)) return true;
    if (type == ast::Type::MATRIX3 && value.type() == typeid(Matrix3)) return true;
    if (type == ast::Type::MATRIX4 && value.type() == typeid(Matrix4)) return true;
    if (type == ast::Type::STRING && value.type() == typeid(std::string)) return true;

    LLOG_ERR << "Unable to create attribute!";
    return true;
}

template<>
int Attrib::get() {
    return boost::get<int>(mValue);
}

template<>
bool Attrib::get() {
    return boost::get<bool>(mValue);
}

bool AttribsContainer::declare(Attrib::Type type, const std::string& key, Attrib::Value& value) {
    const AttribsMap::iterator it = mAttribsMap.find(key);
    if (it != mAttribsMap.end()) {
        Attrib& attr = it->second;
        LLOG_WRN  << "Attribute \"" << key << "\" declared already!!!";//" with type: " << attr.type() << " value: " << attr.value();
        return false;
    }

    Attrib attr;
    if (!Attrib::create(type, value, attr))
        return false;

    mAttribsMap.insert(std::pair<std::string, Attrib>(key, std::move(attr)));
    return true;
}

bool AttribsContainer::attributeExist(const std::string& key) {
    if ( mAttribsMap.find(key) != mAttribsMap.end() ) {
        // found
        return true;
    }
    // not found
    return false;
}

}  // namespace lsd

}  // namespace lava
