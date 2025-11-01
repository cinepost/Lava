#include "Dictionary.h"
#include <sstream>


namespace Falcor {

namespace {

pybind11::object jsonToPython(const Dictionary::Value& v) {

    if (v.type() == typeid(int)) {
        return pybind11::int_(v.get<int>());
    } else if (v.type() == typeid(uint)) {
        return pybind11::int_(v.get<uint>());
    } else if (v.type() == typeid(float)) {
        return pybind11::float_(v.get<float>());
    } else if (v.type() == typeid(std::string)) {
        return pybind11::str(v.get<std::string>());
    } else if (v.type() == typeid(fs::path)) {
        return pybind11::str(v.get<fs::path>().string());
    } else if (v.type() == typeid(bool)) {
        return pybind11::bool_(v.get<bool>());
    } else if (v.type() == typeid(std::vector<int>)) {
        std::vector<int> vec = v.get<std::vector<int>>();
        pybind11::list obj(vec.size());
        for (std::size_t i = 0; i < vec.size(); i++) {
            obj[i] = pybind11::int_(vec[i]);
        }
        return std::move(obj);
    } else if (v.type() == typeid(std::vector<uint>)) {
        std::vector<uint> vec = v.get<std::vector<uint>>();
        pybind11::list obj(vec.size());
        for (std::size_t i = 0; i < vec.size(); i++) {
            obj[i] = pybind11::int_(vec[i]);
        }
        return std::move(obj);
    } else if (v.type() == typeid(std::vector<float>)) {
        std::vector<float> vec = v.get<std::vector<float>>();
        pybind11::list obj(vec.size());
        for (std::size_t i = 0; i < vec.size(); i++) {
            obj[i] = pybind11::float_(vec[i]);
        }
        return std::move(obj);
    } else if (v.type() == typeid(Dictionary)) {
        return v.get<Dictionary>().toPython();
    } else {
        LLOG_ERR << "Unsupported Properties::Value type " << v.type().name() << " !!!";
        return pybind11::none();
    }
}

} // namespace

pybind11::dict Dictionary::toPython() const {
    pybind11::dict obj;
    for(const auto&[key, value]: mContainer) {
        obj[pybind11::str(key)] = jsonToPython(value);
    }
    return obj;
}

bool Dictionary::Value::operator==(const Value& other) const { 
    if(type() != other.type()) return false;
    
    if(type() == typeid(std::string))
        return std::any_cast<std::string>(mValue) == std::any_cast<std::string>(other.mValue);

    if(type() == typeid(float))
        return std::any_cast<float>(mValue) == std::any_cast<float>(other.mValue);

    if(type() == typeid(int))
        return std::any_cast<int>(mValue) == std::any_cast<int>(other.mValue);

    if(type() == typeid(uint))
        return std::any_cast<uint>(mValue) == std::any_cast<uint>(other.mValue);

    if(type() == typeid(bool))
        return std::any_cast<bool>(mValue) == std::any_cast<bool>(other.mValue);

    if(type() == typeid(Falcor::float2))
        return std::any_cast<Falcor::float2>(mValue) == std::any_cast<Falcor::float2>(other.mValue);

    if(type() == typeid(Falcor::float3))
        return std::any_cast<Falcor::float3>(mValue) == std::any_cast<Falcor::float3>(other.mValue);

    if(type() == typeid(Falcor::float4))
        return std::any_cast<Falcor::float4>(mValue) == std::any_cast<Falcor::float4>(other.mValue);

    if(type() == typeid(Falcor::int2))
        return std::any_cast<Falcor::int2>(mValue) == std::any_cast<Falcor::int2>(other.mValue);

    if(type() == typeid(Falcor::int3))
        return std::any_cast<Falcor::int3>(mValue) == std::any_cast<Falcor::int3>(other.mValue);

    if(type() == typeid(Falcor::int4))
        return std::any_cast<Falcor::int4>(mValue) == std::any_cast<Falcor::int4>(other.mValue);

    throw std::runtime_error("Dictionary::Value comparison of unimplemented for type !!!");
}

bool Dictionary::operator==(const Dictionary& other) const {
    return mContainer == other.mContainer;
}

Dictionary::Value::operator std::string() const {
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

std::string Dictionary::Value::toJsonString() const {
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


Dictionary::json Dictionary::Value::toJson() const {
    using json = Dictionary::json;

    if (mValue.type() == typeid(int)) {
        return json(std::any_cast<int>(mValue));
    }
    if (mValue.type() == typeid(uint)) {
        return json(std::any_cast<uint>(mValue));
    }
    if (mValue.type() == typeid(float)) {
        return json(std::any_cast<float>(mValue));
    }
    if (mValue.type() == typeid(std::string)) {
        return json(std::any_cast<std::string>(mValue));
    }
    if (mValue.type() == typeid(fs::path)) {
        return json(std::any_cast<fs::path>(mValue).string());
    }
    if (mValue.type() == typeid(bool)) {
        return json(std::any_cast<bool>(mValue));
    }
    if (mValue.type() == typeid(std::vector<int>)) {
        return json(std::any_cast<std::vector<int>>(mValue));
    }
    if (mValue.type() == typeid(std::vector<uint>)) {
        return json(std::any_cast<std::vector<uint>>(mValue));
    }
    if (mValue.type() == typeid(std::vector<float>)) {
        return json(std::any_cast<std::vector<float>>(mValue));
    }
    FALCOR_UNREACHABLE();
    return {};
}

std::string Dictionary::toJsonString() const {
    std::stringstream ss; ss << "{";
    
    size_t i = 0;
    const size_t c_size = mContainer.size();
    for(const auto&[key, value]: mContainer) {
        ss << "\"" << key << "\"" << ":" << value.toJsonString() << ((++i != c_size) ? ",":"");
    }

    ss << "}";
    return ss.str();
}

Dictionary& Dictionary::update(const Dictionary& d) {
    for(auto const& e: d) mContainer[e.first] = e.second;
    return *this;
}

template<>
Dictionary::Value::operator bool() const {
    if (mValue.type() == typeid(bool)) return std::any_cast<bool>(mValue); 
    else if(mValue.type() == typeid(int)) return std::any_cast<int>(mValue) == 0 ? false : true;
    else if(mValue.type() == typeid(float)) return std::any_cast<float>(mValue) == 0.f ? false : true;

    return false;
}

Dictionary::Value::operator uint() const {
    if(mValue.type() == typeid(int)) return static_cast<uint>(std::any_cast<int>(mValue));
    else if(mValue.type() == typeid(float)) return static_cast<uint>(std::any_cast<float>(mValue));
    return std::any_cast<uint>(mValue);
}

}  // namespace Falcor
