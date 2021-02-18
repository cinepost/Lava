#ifndef SRC_FALCOR_UTILS_CONFIG_STORE_H_
#define SRC_FALCOR_UTILS_CONFIG_STORE_H_

#include <iostream>
#include <map>
#include <fstream>
#include <string>
#include <variant>

#include "Falcor/Core/Framework.h"

namespace Falcor {

class dlldecl ConfigStore {
  using Value = std::variant<bool, int, float, std::string>;
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
    void set(const std::string& key, const T& val);

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
void ConfigStore::set(const std::string& key, const T& val) {
    if(mLocked) {
        //logError("Unable to set config value for key %s. ConfigStore is locked !!!", key);
        return;
    }

    if(mConfigMap.find(key) == mConfigMap.end()) {
        // not found
        mConfigMap.insert({key, val});
    } else {
        // found
        //logError("Unable to set already exisiting config value for key %s !!!", key);
    }
}

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_THREADING_H_
