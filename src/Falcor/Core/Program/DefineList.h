/***************************************************************************
 # Copyright (c) 2015-23, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#ifndef SRC_FALCOR_CORE_PROGRAM_DEFINESLIST_H_
#define SRC_FALCOR_CORE_PROGRAM_DEFINESLIST_H_

#include <initializer_list>
#include <map>
#include <string>

namespace Falcor {

class DefineList {
    using Map = std::map<std::string, std::string>;
public:
    /**
     * Adds a macro definition. If the macro already exists, it will be replaced.
     * @param[in] name The name of macro.
     * @param[in] value Optional. The value of the macro.
     * @return The updated list of macro definitions.
     */
    DefineList& add(const std::string& name, const std::string& val = "");

    /**
     * Removes a macro definition. If the macro doesn't exist, the call will be silently ignored.
     * @param[in] name The name of macro.
     * @return The updated list of macro definitions.
     */
    DefineList& remove(const std::string& name);

    /**
     * Add a define list to the current list
     */
    DefineList& add(const DefineList& dl);

    /**
     * Remove a define list from the current list
     */
    DefineList& remove(const DefineList& dl);

    const Map& getMap() const { return mMap; }

    bool operator<(const DefineList& rhs) const {
          return mMap < rhs.getMap();
    }

    bool operator==(const DefineList& other) const { return mMap == other.getMap(); }
    bool operator!=(const DefineList& other) const { return !(mMap == other.getMap()); }

    void clear() noexcept { mMap.clear(); }


    // iterators
    Map::iterator begin() { return mMap.begin(); }
    Map::const_iterator begin() const { return mMap.begin(); }
    
    Map::iterator end() { return mMap.end(); }
    Map::const_iterator end() const { return mMap.end(); }

    Map::const_iterator cbegin() const { return mMap.cbegin(); }
    Map::const_iterator cend() const { return mMap.cend(); }

    // map operations
    Map::iterator find(const std::string& x) { return mMap.find(x); }
    Map::const_iterator find(const std::string& x) const { return mMap.find(x); }

    // element access
    std::string& operator[](const std::string& x) { return mMap[x]; }

    // modifiers
    Map::size_type erase(const std::string& x) { return mMap.erase(x); }
    Map::iterator erase(Map::iterator position) { return mMap.erase(position); }
    Map::iterator erase(Map::const_iterator position) { return mMap.erase(position); }

public:
    DefineList() = default;
    DefineList(std::initializer_list<std::pair<const std::string, std::string>> il);
    DefineList(const DefineList& dl);
private:
    Map mMap;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_PROGRAM_DEFINESLIST_H_