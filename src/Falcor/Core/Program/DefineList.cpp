#include "DefineList.h"


namespace Falcor {

DefineList& DefineList::add(const std::string& name, const std::string& val) {
    mMap[name] = val;
    return *this;
}

DefineList& DefineList::remove(const std::string& name) {
    mMap.erase(name);
    return *this;
}

DefineList& DefineList::add(const DefineList& dl) {
    for (const auto& p : dl.getMap()) {
        add(p.first, p.second);
    }
    return *this;
}

DefineList& DefineList::remove(const DefineList& dl) {
    for (const auto& p : dl.getMap()) remove(p.first);
    return *this;
}

DefineList::DefineList(std::initializer_list<std::pair<const std::string, std::string>> il): mMap(il) {}

DefineList::DefineList(const DefineList& dl) {
    add(dl);
}

}  // namespace Falcor
