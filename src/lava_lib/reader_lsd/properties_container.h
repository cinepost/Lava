#ifndef SRC_LAVA_LIB_READER_LSD_PROPERTIES_CONTAINER_H_
#define SRC_LAVA_LIB_READER_LSD_PROPERTIES_CONTAINER_H_

#include <map>
#include <unordered_map>
#include <memory>
#include <variant>
#include <string>
#include <regex>

#include "../scene_reader_base.h"
#include "grammar_lsd.h"

#include "lava_utils_lib/logging.h"

namespace lava {

namespace lsd {

static inline std::ostream& indentStream(std::ostream& os, uint indent = 0) {
    for(uint i = 0; i < indent; i++) {
        os << " ";
    }
    return os;
}

class PropertiesContainer;

template <typename R, typename A> R convert_variant(A const& arg) {
    return boost::apply_visitor([](auto const& v) -> R {
        if constexpr (std::is_convertible_v<decltype(v), R>)
            return v;
        else
            throw std::runtime_error("bad conversion");
    } , arg);
}

class Property {
    public:
        using Type = ast::Type;
        using Value = boost::variant<bool, int, Int2, Int3, Int4, double, Vector2, Vector3, Vector4, Matrix3, Matrix4, std::string>;

        enum class Owner{ SYS, USER };

    public:
        // Copy constructor 
        Property(const Property &prop): mType(prop.mType), mValue(prop.mValue), mOwner(prop.mOwner) { }

        // Move constructor
        Property(Property&& prop) noexcept : mType(std::move(prop.mType)), mValue(std::move(prop.mValue)), mOwner(std::move(prop.mOwner)) { }

        Property& operator=(Property& prop) {
            mType = prop.mType;
            mValue = prop.mValue;
            mOwner = prop.mOwner;
            return *this;
        }

        Property& operator=(Property&& prop) {
            mType = std::move(prop.mType);
            mValue = std::move(prop.mValue);
            mOwner = std::move(prop.mOwner);
            return *this;
        }

        static bool create(Type type, const Value& value, Property& property, Owner owner = Owner::SYS);

        inline const Type type() const { return mType; };
        inline const Value& value() const { return mValue; };

        inline bool isUserProperty() const { return (mOwner == Owner::USER ) ? true : false; };
        bool set(Type type, const Value& value);
        bool set(const Value& value);

        template<typename T>
        const T get() const;


        inline bool hasSubContainer() const { return mpSubContainer ? true : false; };
        std::shared_ptr<PropertiesContainer> createSubContainer();
        inline std::shared_ptr<PropertiesContainer> subContainer() const { return mpSubContainer; };

    private:
        Property(): mType(Type::UNKNOWN), mOwner(Owner::SYS) { }
        Property(Type type, const Value& value, Owner owner = Owner::SYS): mType(type), mValue(value), mOwner(owner) { }
        
        ast::Type   mType;
        Value       mValue;
        Owner       mOwner;

        std::shared_ptr<PropertiesContainer> mpSubContainer = nullptr;

        friend class PropertiesContainer;
};

static inline Property::Type valueType(const Property::Value& value) {
    if (value.type() == typeid(bool)) return Property::Type::BOOL;
    if (value.type() == typeid(int)) return Property::Type::INT;
    if (value.type() == typeid(double)) return Property::Type::FLOAT;
    if (value.type() == typeid(Int2)) return Property::Type::INT2;
    if (value.type() == typeid(Int3)) return Property::Type::INT3;
    if (value.type() == typeid(Int4)) return Property::Type::INT4;
    if (value.type() == typeid(Vector2)) return Property::Type::VECTOR2;
    if (value.type() == typeid(Vector3)) return Property::Type::VECTOR3;
    if (value.type() == typeid(Vector4)) return Property::Type::VECTOR4;
    if (value.type() == typeid(Matrix3)) return Property::Type::MATRIX3;
    if (value.type() == typeid(Matrix4)) return Property::Type::MATRIX4;
    if (value.type() == typeid(std::string)) return Property::Type::STRING;

    return Property::Type::UNKNOWN;
}

static inline bool checkValueTypeStrict(Property::Type type, const Property::Value& value) {
    if (type == ast::Type::BOOL && value.type() != typeid(bool)) return false;
    if (type == ast::Type::INT && value.type() != typeid(int)) return false;
    if (type == ast::Type::FLOAT && value.type() != typeid(double)) return false;
    if (type == ast::Type::INT2 && value.type() != typeid(Int2)) return false;
    if (type == ast::Type::INT3 && value.type() != typeid(Int3)) return false;
    if (type == ast::Type::INT4 && value.type() != typeid(Int4)) return false;
    if (type == ast::Type::VECTOR2 && value.type() != typeid(Vector2)) return false;
    if (type == ast::Type::VECTOR3 && value.type() != typeid(Vector3)) return false;
    if (type == ast::Type::VECTOR4 && value.type() != typeid(Vector4)) return false;
    if (type == ast::Type::MATRIX3 && value.type() != typeid(Matrix3)) return false;
    if (type == ast::Type::MATRIX4 && value.type() != typeid(Matrix4)) return false;
    if (type == ast::Type::STRING && value.type() != typeid(std::string)) return false;

    return true;
}

/**
 * loose type checking. true returned when converion is possible
 */
static inline bool checkValueTypeLoose(Property::Type type, const Property::Value& value) {
    switch (type) {
        case ast::Type::BOOL:
            if(value.type() == typeid(bool)) { return true; }
            else if (value.type() == typeid(int)) { return true; }
            else if (value.type() == typeid(double)) { return true; }
            return false;
        default:
            break;
    };

    if ((type == ast::Type::INT || type == ast::Type::FLOAT) && (value.type() == typeid(int) || value.type() == typeid(double))) return true;
    if ((type == ast::Type::INT2 || type == ast::Type::VECTOR2) && (value.type() == typeid(Int2) || value.type() == typeid(Vector2))) return true;
    if ((type == ast::Type::INT3 || type == ast::Type::VECTOR3) && (value.type() == typeid(Int3) || value.type() == typeid(Vector3))) return true;
    if ((type == ast::Type::INT4 || type == ast::Type::VECTOR4) && (value.type() == typeid(Int4) || value.type() == typeid(Vector4))) return true;
    if (type == ast::Type::MATRIX3 && value.type() != typeid(Matrix3)) return false;
    if (type == ast::Type::MATRIX4 && value.type() != typeid(Matrix4)) return false;
    if (type == ast::Type::STRING && value.type() != typeid(std::string)) return false;

    return true;
}

class PropertiesContainer: public std::enable_shared_from_this<PropertiesContainer> {
 public:
    using SharedPtr = std::shared_ptr<PropertiesContainer>;
    using PropertyKey = std::pair<ast::Style, std::string>;
    using PropertiesMap = std::map<PropertyKey, Property>;

 public:
    PropertiesContainer(): mpParent(nullptr) {};
    PropertiesContainer(PropertiesContainer::SharedPtr pParent): mpParent(pParent) {};
    //virtual SharedPtr parent() { return mpParent; };

    bool declareProperty(ast::Style style, Property::Type type, const std::string& name, const Property::Value& value, Property::Owner owner = Property::Owner::SYS);
    bool setProperty(ast::Style style, const std::string& name, const Property::Value& value);

    Property* getProperty(ast::Style style, const std::string& name);
    const Property* getProperty(ast::Style style, const std::string& name) const;

    template<typename T>
    const T getPropertyValue(ast::Style style, const std::string& name, const T& default_value) const;

    bool propertyExist(ast::Style style, const std::string& name) const;

    const PropertiesContainer filterProperties(ast::Style style, const std::regex& re) const;
    const PropertiesContainer filterProperties(ast::Style style) const;
    inline const PropertiesMap& properties() const { return mPropertiesMap; };

    inline size_t size() const { return mPropertiesMap.size(); };

    virtual const void printSummary(std::ostream& os, uint indent = 0) const;

 private:
    const Property::Value& _getPropertyValue(ast::Style style, const std::string& name, const Property::Value& default_value) const;

 protected:
    PropertiesMap mPropertiesMap;
    SharedPtr mpParent = nullptr;
};

std::string to_string(const PropertiesContainer::PropertyKey& key);

using Value = lava::lsd::Property::Value;
std::string to_string(const Value& val);

}  // namespace lsd

}  // namespace lava

using Value = lava::lsd::Property::Value;
static inline std::string to_string(const Value& val) {
    if (val.type() == typeid(std::string)) return "string";
    if (val.type() == typeid(bool)) return "bool";
    if (val.type() == typeid(int)) return "int";
    if (val.type() == typeid(double)) return "float";
    if (val.type() == typeid(lava::lsd::Int2)) return "int2";
    if (val.type() == typeid(lava::lsd::Int3)) return "int3";
    if (val.type() == typeid(lava::lsd::Int4)) return "int4";
    if (val.type() == typeid(lava::lsd::Vector2)) return "vector2";
    if (val.type() == typeid(lava::lsd::Vector3)) return "vector3";
    if (val.type() == typeid(lava::lsd::Vector4)) return "vector4";
    if (val.type() == typeid(lava::lsd::Matrix3)) return "matrix3";
    if (val.type() == typeid(lava::lsd::Matrix4)) return "matrix4";
    return "unknown";
}

using Type = lava::lsd::Property::Type;
static inline std::string to_string(const Type& t) {
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

#endif  // SRC_LAVA_LIB_READER_LSD_PROPERTIES_CONTAINER_H_
