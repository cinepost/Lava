#include "InternalDictionary.h"
#include <sstream>


namespace Falcor {

InternalDictionary::Value::operator std::string() const {
    if(mValue.type() == typeid(std::string))
        return std::any_cast<std::string>(mValue);

    if (mValue.type() == typeid(float))
        return std::to_string(std::any_cast<float>(mValue));

    if (mValue.type() == typeid(int))
        return std::to_string(std::any_cast<int>(mValue));

    if (mValue.type() == typeid(uint))
        return std::to_string(std::any_cast<uint>(mValue));

    if (mValue.type() == typeid(bool))
        return std::any_cast<bool>(mValue) ? "true" : "false";

    if (mValue.type() == typeid(Falcor::float2))
        return to_string(std::any_cast<Falcor::float2>(mValue));

    if (mValue.type() == typeid(Falcor::float3))
        return to_string(std::any_cast<Falcor::float3>(mValue));

    if (mValue.type() == typeid(Falcor::float4))
        return to_string(std::any_cast<Falcor::float4>(mValue));

    if (mValue.type() == typeid(Falcor::int2))
        return to_string(std::any_cast<Falcor::int2>(mValue));

    if (mValue.type() == typeid(Falcor::int3))
        return to_string(std::any_cast<Falcor::int3>(mValue));

    if (mValue.type() == typeid(Falcor::int4))
        return to_string(std::any_cast<Falcor::int4>(mValue));

    return "Unknown";
}

std::string InternalDictionary::Value::toJsonString() const {
    if(mValue.type() == typeid(std::string)) {
        #ifdef _WIN32
        // return "\"" + std::string{*this} + "\"";
        return "\"" + this->operator std::string() + "\"";
        // return "\"" + static_cast<const std::string&>(*this) + "\"";
        #else
        return "\"" + std::string(*this) + "\"";
        #endif
    }
    #ifdef _WIN32
    // return std::string{*this};
    return this->operator std::string();
    // return static_cast<const std::string&>(*this);
    #else
    return std::string(*this);
    #endif
}

std::string InternalDictionary::toJsonString() const {
    std::stringstream ss; ss << "{";
    
    size_t i = 0;
    const size_t c_size = mContainer.size();
    for(const auto[key, value]: mContainer) {
        ss << "\"" << key << "\"" << ":" << value.toJsonString() << ((++i != c_size) ? ",":"");
    }

    ss << "}";
    return ss.str();
}

template<>
InternalDictionary::Value::operator bool() const {
    if (mValue.type() == typeid(bool)) return std::any_cast<bool>(mValue); 
    else if(mValue.type() == typeid(int)) return std::any_cast<int>(mValue) == 0 ? false : true;
    else if(mValue.type() == typeid(float)) return std::any_cast<float>(mValue) == 0.f ? false : true;

    return false;
}

InternalDictionary::Value::operator uint() const {
    if(mValue.type() == typeid(int)) return static_cast<uint>(std::any_cast<int>(mValue));
    else if(mValue.type() == typeid(float)) return static_cast<uint>(std::any_cast<float>(mValue));
    return std::any_cast<uint>(mValue);
}

}  // namespace Falcor
