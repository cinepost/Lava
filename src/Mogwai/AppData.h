/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#ifndef SRC_MOGWAI_APPDATA_H_
#define SRC_MOGWAI_APPDATA_H_

#include <vector>
#include <string>

// #ifdef _WIN32
//   #include <filesystem>
//   namespace fs = std::filesystem;
// #else
  #include "boost/filesystem.hpp"
  namespace fs = boost::filesystem;
// #endif

#include "Falcor/Falcor.h"

namespace Mogwai {

/** Holds a set of persistent application data stored in the user directory.
*/
class AppData {
 public:
    explicit AppData(const fs::path& path);

    const std::vector<std::string>& getRecentScripts() const { return mRecentScripts; }
    const std::vector<std::string>& getRecentScenes() const { return mRecentScenes; }

    void addRecentScript(const std::string& filename);
    void addRecentScene(const std::string& filename);

 private:
    void addRecentFile(std::vector<std::string>& recentFiles, const std::string& filename);
    void removeNonExistingFiles(std::vector<std::string>& files);

    void save();

    void loadFromFile(const fs::path& path);
    void saveToFile(const fs::path& path);

    fs::path mPath;

    std::vector<std::string> mRecentScripts;
    std::vector<std::string> mRecentScenes;
};

}  // namespace Mogwai

#endif  // SRC_MOGWAI_APPDATA_H_
