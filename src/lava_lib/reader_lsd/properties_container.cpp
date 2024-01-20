#include <variant>
#include <map>
#include <string>

#include "Falcor/Utils/Math/Vector.h"

#include "grammar_lsd.h"
#include "properties_container.h"

#include "lava_utils_lib/logging.h"

namespace lava {

namespace lsd {

std::shared_ptr<PropertiesContainer> Property::createSubContainer() {
    if(mpSubContainer) {
        LLOG_WRN << "Sub-container already exist for property !"; 
    } else {
        mpSubContainer = std::shared_ptr<PropertiesContainer>( new PropertiesContainer());
    }
    return mpSubContainer;
}

template<>
const bool Property::get() const {
    try {
        return boost::get<bool>(mValue);
    } catch (...) {
        return (bool) (boost::get<int>(mValue) == 0 ? false : true);
    }
}

template<>
const int Property::get() const {
    return boost::get<int>(mValue);
}

template<>
const unsigned int Property::get() const {
    return (unsigned int)boost::get<int>(mValue);
}

template<>
const Int2 Property::get() const {
    try {
        return boost::get<Int2>(mValue);
    } catch (...) {
        auto v = boost::get<Vector2>(mValue);
        return Int2{static_cast<int>(v[0]), static_cast<int>(v[1])};
    }
}

template<>
const Int3 Property::get() const {
    return boost::get<Int3>(mValue);
}

template<>
const Int4 Property::get() const {
    return boost::get<Int4>(mValue);
}

template<>
const double Property::get() const {
    try {
        return boost::get<double>(mValue);
    } catch (...) {
        return (double) boost::get<int>(mValue);
    }
}

template<>
const float Property::get() const {
    return (float) this->get<double>();
}

template<>
const Vector2 Property::get() const {
    return boost::get<Vector2>(mValue);
}

template<>
const Vector3 Property::get() const {
    return boost::get<Vector3>(mValue);
}

template<>
const Vector4 Property::get() const {
    return boost::get<Vector4>(mValue);
}

template<>
const std::string Property::get() const {
    return boost::get<std::string>(mValue);
}

// TODO: try casting value to appropriate type here in case it differs
bool Property::create(Property::Type type, const Property::Value& value, Property& property, Property::Owner owner) {
    if (!checkValueTypeLoose(type, value)) return false;

    property.mType = type;
    property.mValue = value;
    property.mOwner = owner;

    if(type == Property::Type::BOOL) property.mValue = property.get<bool>();

    return true;
}

bool Property::set(Type type, const Value& value) {
    if (mType == Type::UNKNOWN) {
        mType = type;
        mValue = value;
        return true;
    }

    if (mType == type && type == valueType(value)) {
        mValue = value;
        return true;
    }
    
    LLOG_ERR << "Can not set property of type " << to_string(mType) << " to value of type " << to_string(type) << " !!!";
    return false;
}

bool Property::set(const Value& value) {
    auto type = valueType(value);
    if (mType == type) {
        mValue = value;
        return true;
    }

    if (mType == Type::BOOL && type == Type::INT) {
        LLOG_TRC << "Conversion from " << to_string(type) << " to " << to_string(mType);
        mValue = (bool) (boost::get<int>(value) == 0 ? false : true);
        return true;
    }

    if (mType == Type::BOOL && type == Type::FLOAT) {
        LLOG_TRC << "Conversion from " << to_string(type) << " to " << to_string(mType);
        mValue = (bool) (boost::get<double>(value) == 0 ? false : true);
        return true;
    }

    if (mType == Type::INT && type == Type::FLOAT) {
        LLOG_TRC << "Conversion from " << to_string(type) << " to " << to_string(mType);
        mValue = (int) boost::get<double>(value);
        return true;
    }

    if (mType == Type::INT2 && type == Type::VECTOR2) {
        LLOG_TRC << "Conversion from " << to_string(type) << " to " << to_string(mType);
        auto const& v = boost::get<Vector2>(value);
        mValue = Int2{int(v[0]), int(v[1])};
        return true;
    }

    if (mType == Type::INT3 && type == Type::VECTOR3) {
        LLOG_TRC << "Conversion from " << to_string(type) << " to " << to_string(mType);
        auto const& v = boost::get<Vector3>(value);
        mValue = Int3{int(v[0]), int(v[1]), int(v[2])};
        return true;
    }

    if (mType == Type::INT4 && type == Type::VECTOR4) {
        LLOG_TRC << "Conversion from " << to_string(type) << " to " << to_string(mType);
        auto const& v = boost::get<Vector4>(value);
        mValue = Int4{int(v[0]), int(v[1]), int(v[2]), int(v[3])};
        return true;
    }

    if (mType == Type::FLOAT && type == Type::INT) {
        LLOG_TRC << "Conversion from " << to_string(type) << " to " << to_string(mType);
        mValue = (double) boost::get<int>(value);
        return true;
    }

    if (mType == Type::VECTOR2 && type == Type::INT2) {
        LLOG_TRC << "Conversion from " << to_string(type) << " to " << to_string(mType);
        auto const& v = boost::get<Int2>(value);
        mValue = Vector2{double(v[0]), double(v[1])};
        return true;
    }

    if (mType == Type::VECTOR3 && type == Type::INT3) {
        LLOG_TRC << "Conversion from " << to_string(type) << " to " << to_string(mType);
        auto const& v = boost::get<Int3>(value);
        mValue = Vector3{double(v[0]), double(v[1]), double(v[2])};
        return true;
    }

    if (mType == Type::VECTOR4 && type == Type::INT4) {
        LLOG_TRC << "Conversion from " << to_string(type) << " to " << to_string(mType);
        auto const& v = boost::get<Int4>(value);
        mValue = Vector4{double(v[0]), double(v[1]), double(v[2]), double(v[3])};
        return true;
    }

    LLOG_ERR << "Can not set property of type " << to_string(mType) << " to value of type " << to_string(type) << " !!!";
    return false;
}

bool PropertiesContainer::declareProperty(ast::Style style, Property::Type type, const std::string& name, const Property::Value& value, Property::Owner owner) {
    auto propKey = PropertyKey(style, name);
    const PropertiesMap::iterator it = mPropertiesMap.find(propKey);
    if (it != mPropertiesMap.end()) {
        LLOG_DBG << "Property \"" << to_string(propKey) << "\" declared already !";
        return false;
    }

    Property prop;
    if (!Property::create(type, value, prop, owner)) {
        LLOG_ERR << "Error creating property type: " << to_string(type) << " with value type: " << to_string(value) << " !!!";
        return false;
    }

    mPropertiesMap.insert(std::pair<PropertyKey, Property>(std::move(propKey), std::move(prop)));
    return true;
}

bool PropertiesContainer::setProperty(ast::Style style, const std::string& name, const Property::Value& value) {
    auto propKey = PropertyKey(style, name);
    PropertiesMap::iterator it = mPropertiesMap.find(propKey);
    if (it == mPropertiesMap.end()) {
        // property does not exist. declare it here as a user property
        if(!declareProperty(style, valueType(value), name, value, Property::Owner::USER)) {
            LLOG_WRN << "Error declaring user property \"" << to_string(propKey) << "\" !!!";
            return false;
        }

        it = mPropertiesMap.find(propKey);
    }

    if (!it->second.set(value)) {
        LLOG_ERR << "Error setting property \"" << to_string(propKey) << "\" !!!";
        return false;
    }
    
    return true;
}

Property* PropertiesContainer::getProperty(ast::Style style, const std::string& name) {
    auto it = mPropertiesMap.find(PropertyKey(style, name));
    if (it == mPropertiesMap.end()) {
        if( mpParent ) {
            return mpParent->getProperty(style, name);
        }
        return nullptr;
    }
    return &it->second;
}

const Property* PropertiesContainer::getProperty(ast::Style style, const std::string& name) const {
    auto it = mPropertiesMap.find(PropertyKey(style, name));
    if (it == mPropertiesMap.end()) {
        if( mpParent ) {
            return mpParent->getProperty(style, name);
        }
        return nullptr;
    }
    return &it->second;
}

const Property::Value& PropertiesContainer::_getPropertyValue(ast::Style style, const std::string& name, const  Property::Value& default_value) const {
    auto pProperty = getProperty(style, name);
    
    if(!pProperty)
        LLOG_DBG << "Can't find property " << to_string(PropertyKey(style, name)) << " ! Returning default value...";
        return default_value;

    if(!checkValueTypeStrict(pProperty->type(), default_value)) {
        LLOG_WRN << "Property " << to_string(PropertyKey(style, name)) << " type and default_value type does not match !!!";
    }

    return pProperty->value();
}

template<>
const Int2 PropertiesContainer::getPropertyValue(ast::Style style, const std::string& name, const Int2& default_value) const {
    auto pProperty = getProperty(style, name);
    if(pProperty) return pProperty->get<Int2>();
    LLOG_TRC << "Property '" << name << "' of style '" << to_string(style) << "'' doesn't exist. Returning default value "<< to_string(default_value);
    return default_value;
}

template<>
const Int3 PropertiesContainer::getPropertyValue(ast::Style style, const std::string& name, const Int3& default_value) const {
    auto pProperty = getProperty(style, name);
    if(pProperty) return pProperty->get<Int3>();
    LLOG_TRC << "Property '" << name << "' of style '" << to_string(style) << "'' doesn't exist. Returning default value "<< to_string(default_value);
    return default_value;
}

template<>
const Int4 PropertiesContainer::getPropertyValue(ast::Style style, const std::string& name, const Int4& default_value) const {
    auto pProperty = getProperty(style, name);
    if(pProperty) return pProperty->get<Int4>();
    LLOG_TRC << "Property '" << name << "' of style '" << to_string(style) << "'' doesn't exist. Returning default value "<< to_string(default_value);
    return default_value;
}

template<>
const Vector2 PropertiesContainer::getPropertyValue(ast::Style style, const std::string& name, const Vector2& default_value) const {
    auto pProperty = getProperty(style, name);
    if(pProperty) return pProperty->get<Vector2>();
    LLOG_TRC << "Property '" << name << "' of style '" << to_string(style) << "'' doesn't exist. Returning default value "<< to_string(default_value);
    return default_value;
}

template<>
const Vector3 PropertiesContainer::getPropertyValue(ast::Style style, const std::string& name, const Vector3& default_value) const {
    auto pProperty = getProperty(style, name);
    if(pProperty) return pProperty->get<Vector3>();
    LLOG_TRC << "Property '" << name << "' of style '" << to_string(style) << "'' doesn't exist. Returning default value "<< to_string(default_value);
    return default_value;
}

template<>
const Vector4 PropertiesContainer::getPropertyValue(ast::Style style, const std::string& name, const Vector4& default_value) const {
    auto pProperty = getProperty(style, name);
    if(pProperty) return pProperty->get<Vector4>();
    LLOG_TRC << "Property '" << name << "' of style '" << to_string(style) << "'' doesn't exist. Returning default value "<< to_string(default_value);
    return default_value;
}

template<>
const std::string PropertiesContainer::getPropertyValue(ast::Style style, const std::string& name, const std::string& default_value) const {
    auto pProperty = getProperty(style, name);
    if(pProperty) return pProperty->get<std::string>();
    LLOG_TRC << "Property '" << name << "' of style '" << to_string(style) << "'' doesn't exist. Returning default value "<< to_string(default_value);
    return default_value;
}

template<>
const double PropertiesContainer::getPropertyValue(ast::Style style, const std::string& name, const double& default_value) const {
    auto pProperty = getProperty(style, name);
    if(pProperty) return pProperty->get<double>();
    LLOG_TRC << "Property '" << name << "' of style '" << to_string(style) << "'' doesn't exist. Returning default value "<< to_string(default_value);
    return default_value;
}

template<>
const float PropertiesContainer::getPropertyValue(ast::Style style, const std::string& name, const float& default_value) const {
    auto pProperty = getProperty(style, name);
    if(pProperty) return pProperty->get<float>();
    LLOG_TRC << "Property '" << name << "' of style '" << to_string(style) << "'' doesn't exist. Returning default value "<< to_string(default_value);
    return default_value;
}

template<>
const int PropertiesContainer::getPropertyValue(ast::Style style, const std::string& name, const int& default_value) const {
    auto pProperty = getProperty(style, name);
    if(pProperty) return pProperty->get<int>();
    LLOG_TRC << "Property '" << name << "' of style '" << to_string(style) << "'' doesn't exist. Returning default value "<< to_string(default_value);
    return default_value;
}

template<>
const bool PropertiesContainer::getPropertyValue(ast::Style style, const std::string& name, const bool& default_value) const {
    auto pProperty = getProperty(style, name);
    if(pProperty) return pProperty->get<bool>();
    LLOG_TRC << "Property '" << name << "' of style '" << to_string(style) << "'' doesn't exist. Returning default value "<< (default_value ? "True" : "False");
    return default_value;
}

bool PropertiesContainer::propertyExist(ast::Style style, const std::string& name) const {
    auto propKey = PropertiesContainer::PropertyKey(style, name);
    if ( mPropertiesMap.find(propKey) != mPropertiesMap.end() ) {
        // found
        return true;
    }
    // not found
    return false;
}

const PropertiesContainer PropertiesContainer::filterProperties(ast::Style style, const std::regex& re) const {
    PropertiesContainer container;

    for(auto const& item: mPropertiesMap) {
        auto const& key = item.first;
        if ((key.first == style) && std::regex_match(key.second, re)) {
            container.mPropertiesMap.insert(item);
        }
    }

    return container;
}

const PropertiesContainer PropertiesContainer::filterProperties(ast::Style style) const {
    PropertiesContainer container;

    for(auto const& item: mPropertiesMap) {
        auto const& key = item.first;
        if (key.first == style) {
            container.mPropertiesMap.insert(item);
        }
    }

    return container;
}

Falcor::Dictionary PropertiesContainer::to_dict(ast::Style style, bool recursive) const {
    Falcor::Dictionary dict;

    for(auto const& item: mPropertiesMap) {
        auto const& key = item.first;
        auto const& prop = item.second;
        if (key.first == style) {
            switch (valueType(prop.value())) {
                case Property::Type::BOOL:
                    dict[key.second] = static_cast<bool>(prop.get<bool>());
                    break;
                
                case Property::Type::INT:
                    dict[key.second] = static_cast<int32_t>(prop.get<int>());
                    break;
                
                case Property::Type::FLOAT:
                    dict[key.second] = static_cast<float>(prop.get<double>());
                    break;

                case Property::Type::INT2:
                    {
                        auto tmp = prop.get<Int2>();
                        dict[key.second] = Falcor::int2({tmp[0], tmp[1]});
                    }
                    break;
                
                case Property::Type::INT3:
                    {
                        auto tmp = prop.get<Int3>();
                        dict[key.second] = Falcor::int3({tmp[0], tmp[1], tmp[2]});
                    }
                    break;
                
                case Property::Type::INT4:
                    {
                        auto tmp = prop.get<Int4>();
                        dict[key.second] = Falcor::int4({tmp[0], tmp[1], tmp[2], tmp[3]});
                    }
                    break;
                
                case Property::Type::VECTOR2:
                    {
                        auto tmp = prop.get<Vector2>();
                        dict[key.second] = Falcor::float2({tmp[0], tmp[1]});
                    }
                    break;
                
                case Property::Type::VECTOR3:
                    {
                        auto tmp = prop.get<Vector3>();
                        dict[key.second] = Falcor::float3({tmp[0], tmp[1], tmp[2]});
                    }
                    break;
                
                case Property::Type::VECTOR4:
                    {
                        auto tmp = prop.get<Vector4>();
                        dict[key.second] = Falcor::float4({tmp[0], tmp[1], tmp[2], tmp[3]});
                    }
                    break;

                case Property::Type::STRING:
                    dict[key.second] = static_cast<std::string>(prop.get<std::string>());
                    break;
                
                case Property::Type::MATRIX3:
                    assert(false && "Unimplemented value type Matrix3 !");
                    break;
                
                case Property::Type::MATRIX4:
                    assert(false && "Unimplemented value type Matrix4 !");
                    break;
                
                default:
                    assert(false && "Unsupported value type !!!");
                    break;
            }
        }
    }

    return dict;
}

const void PropertiesContainer::printSummary(std::ostream& os, uint indent) const { 
    for( auto const& [key, prop]: mPropertiesMap) {
        indentStream(os, indent) << (prop.isUserProperty() ? "user" : "sys ") << " property " << to_string(key) << " type: " << to_string(prop.type()) << " value: " << to_string(prop.value()) << "\n";
        if (prop.hasSubContainer()) {
            indentStream(os, indent) << "Sub-container {\n";
            prop.subContainer()->printSummary(os, indent + 4);
            indentStream(os, indent) << "}\n";
        } 
    }
}


std::string to_string(const PropertiesContainer::PropertyKey& key) {
    return "style: " + to_string(key.first) + " name: \"" + key.second + "\"";
};

using Value = lava::lsd::Property::Value;
std::string to_string(const Value& val) {
    if (val.type() == typeid(std::string)) {
        return std::string("String: ") + boost::get<std::string>(val);
    }
    if (val.type() == typeid(bool)) {
        return boost::get<bool>(val) ? "Bool: true" : "Bool: false";
    }
    
    std::ostringstream ss;
    if (val.type() == typeid(int)) {
        ss << "Int: " << boost::get<int>(val);
        return ss.str();    
    }
    if (val.type() == typeid(double)) {
        ss << "Double: " << boost::get<double>(val);
        return ss.str();
    }
    if (val.type() == typeid(lava::lsd::Int2)) {
        auto const& v = boost::get<lava::lsd::Int2>(val);
        ss << "Int2[ ";
        std::copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
        ss << " ]";
        return ss.str();
    };
    if (val.type() == typeid(lava::lsd::Int3)) {
        auto const& v = boost::get<lava::lsd::Int3>(val);
        ss << "Int3[ ";
        std::copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
        ss << " ]";
        return ss.str();
    };
    if (val.type() == typeid(lava::lsd::Int4)) {
        auto const& v = boost::get<lava::lsd::Int4>(val);
        ss << "Int4[ ";
        std::copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
        ss << " ]";
        return ss.str();
    }
    if (val.type() == typeid(lava::lsd::Vector2)) {
        auto const& v = boost::get<lava::lsd::Vector2>(val);
        ss << "Vector2[ ";
        std::copy(v.begin(), v.end(), std::ostream_iterator<double>(ss, " "));
        ss << " ]";
        return ss.str();
    };
    if (val.type() == typeid(lava::lsd::Vector3)) {
        auto const& v = boost::get<lava::lsd::Vector3>(val);
        ss << "Vector3[ ";
        std::copy(v.begin(), v.end(), std::ostream_iterator<double>(ss, " "));
        ss << " ]";
        return ss.str();
    };
    if (val.type() == typeid(lava::lsd::Vector4)) {
        auto const& v = boost::get<lava::lsd::Vector4>(val);
        ss << "Vector4[ ";
        std::copy(v.begin(), v.end(), std::ostream_iterator<double>(ss, " "));
        ss << " ]";
        return ss.str();
    };
    if (val.type() == typeid(lava::lsd::Matrix3)) {
        auto const& v = boost::get<lava::lsd::Matrix3>(val);
        ss << "Matrix3[ ";
        std::copy(v.begin(), v.end(), std::ostream_iterator<double>(ss, " "));
        ss << " ]";
        return ss.str();
    };
    if (val.type() == typeid(lava::lsd::Matrix4)) {
        auto const& v = boost::get<lava::lsd::Matrix4>(val);
        ss << "Matrix4[ ";
        std::copy(v.begin(), v.end(), std::ostream_iterator<double>(ss, " "));
        ss << " ]";
        return ss.str();
    };
    return "unknown";
}

}  // namespace lsd

}  // namespace lava
