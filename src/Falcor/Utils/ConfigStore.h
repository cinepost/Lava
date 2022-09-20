#ifndef SRC_FALCOR_UTILS_CONFIG_STORE_H_
#define SRC_FALCOR_UTILS_CONFIG_STORE_H_

#include <iostream>
#include <map>
#include <fstream>
#include <string>
#include <variant>

#include "Falcor/Core/Framework.h"

//typedef x3::variant<bool, int, Int2, Int3, Int4, double, Vector2, Vector3, Vector4, std::string> PropValue;

namespace Falcor {

class dlldecl ConfigStore {
  using Value = std::variant<
        bool, 
        int, Falcor::int2, Falcor::int3, Falcor::int4, 
        float, Falcor::float2, Falcor::float3, Falcor::float4, 
        std::string
    >;
  public:
    ConfigStore(): mLocked(false){};

    static ConfigStore& instance() {
        static ConfigStore instance;
        return instance;
    }
    void parseFile(std::ifstream& inStream) {}; // TODO: implement
    
    void lock() { mLocked = true; };

    template<typename T>
    T get(const std::string& key, const T& defaultValue) const;
  
    template<typename T>
    void set(const std::string& key, const T& value);

  private:
    ConfigStore(const ConfigStore&) = delete;
    ConfigStore& operator=(const ConfigStore&) = delete;

    std::map<std::string, Value> mConfigMap;

    bool mLocked = false;
};

template<typename T>
T ConfigStore::get(const std::string& key, const T& defaultValue) const {
    auto const& it = mConfigMap.find(key);
    if(it == mConfigMap.end()) {
        // not found
        return defaultValue;
    } else {
        // found
        return std::get<T>(it->second);
    }
}

//template bool ConfigStore::get<bool>(const std::string&, const bool&) const;

template<typename T>
void ConfigStore::set(const std::string& key, const T& value) {
    if(mLocked) {
        LLOG_ERR << "Unable to set config value for key " << key << ". ConfigStore is locked !";
        return;
    }
/*
    if(!mContainer.propertyExist(global_style, key)) {
        bool declared = mContainer.declareProperty(
            global_style, 
            Property::Type type, 
            key, value, lava::lsd::Property::Owner::SYS);
    
        if(!declared) {
            LLOG_ERR << "Error declaring ConfigStore variable " << key;
            return;
        }
        
    } else {
        bool set = mContainer.setProperty(global_style, key, value);
        if(!set) {
            LLOG_ERR << "Error setting ConfigStore variable " << key;
            return;
        }
    }
*/
}

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_THREADING_H_
