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
#include <fstream>
#include <regex>

#include "stdafx.h"
#include "Falcor/Utils/StringUtils.h"
#include "Falcor/Utils/Debug/debug.h"
#include "OS.h"

#ifndef PROJECT_DIR
#define PROJECT_DIR "/home/max/dev/Falcor/"
#endif

#ifndef LAVA_INSTALL_DIR
#define LAVA_INSTALL_DIR "/opt/lava/"
#endif

namespace Falcor {

std::string gMsgBoxTitle = "Falcor";

void msgBoxTitle(const std::string& title) {
    gMsgBoxTitle = title;
}

uint32_t getLowerPowerOf2(uint32_t a) {
    assert(a != 0);
    return 1 << bitScanReverse(a);
}

inline std::vector<fs::path> getInitialShaderDirectories() {
    std::vector<fs::path> developmentDirectories = {
        // First we search in source folders.
        //std::string(PROJECT_DIR),
        //std::string(PROJECT_DIR) + "../",
        //std::string(PROJECT_DIR) + "../Tools/FalcorTest/",
        
        // Then we search in deployment folders (necessary to pickup NVAPI and other third-party shaders).
        std::string(LAVA_INSTALL_DIR) + "shaders",
        //getExecutableDirectory() + "../shaders",
    };

    std::vector<fs::path> deploymentDirectories = {
        std::string(LAVA_INSTALL_DIR) + "shaders",
        //getExecutableDirectory() + "../shaders"
    };

    std::cout << "mode: " << (isDevelopmentMode() ? "development" : "production") << "\n";
    //std::cout << "exec dir: " << getExecutableDirectory() << "\n";

    return isDevelopmentMode() ? developmentDirectories : deploymentDirectories;
}

inline std::vector<std::string> getInitialRenderPassDirectories() {
    std::vector<std::string> developmentDirectories = {
        // Then we search in deployment folders (necessary to pickup NVAPI and other third-party shaders).
        std::string(LAVA_INSTALL_DIR) + "render_passes",
        getExecutableDirectory() + "../render_passes",
    };

    std::vector<std::string> deploymentDirectories = {
        std::string(LAVA_INSTALL_DIR) + "render_passes",
        getExecutableDirectory() + "../render_passes"
    };

    return isDevelopmentMode() ? developmentDirectories : deploymentDirectories;
}

static std::vector<fs::path> gShaderDirectories = getInitialShaderDirectories();
static std::vector<std::string> gRenderPassDirectories = getInitialRenderPassDirectories();

inline std::vector<std::string> getInitialDataDirectories() {
    std::vector<std::string> developmentDirectories = {
        //std::string(PROJECT_DIR) + "/Data",
        std::string(LAVA_INSTALL_DIR) + "data",
        getExecutableDirectory() + "../data",
    };

    std::vector<std::string> deploymentDirectories = {
        std::string(LAVA_INSTALL_DIR) + "data",
        getExecutableDirectory() + "../data"
    };

    std::vector<std::string> directories = isDevelopmentMode() ? developmentDirectories : deploymentDirectories;

    // Add development media folder.
#ifdef _MSC_VER
    directories.push_back(getExecutableDirectory() + "/../../../Media"); // Relative to Visual Studio output folder
#else
    directories.push_back(getExecutableDirectory() + "/../Media"); // Relative to Makefile output folder
#endif

    // Add additional media folders.
    std::string mediaFolders;
    if (getEnvironmentVariable("FALCOR_MEDIA_FOLDERS", mediaFolders)) {
        auto folders = splitString(mediaFolders, ";");
        directories.insert(directories.end(), folders.begin(), folders.end());
    }

    return directories;
}

static std::vector<std::string> gDataDirectories = getInitialDataDirectories();

const std::vector<std::string>& getDataDirectoriesList() {
    return gDataDirectories;
}

void addDataDirectory(const std::string& dir) {
    if (std::find(gDataDirectories.begin(), gDataDirectories.end(), dir) == gDataDirectories.end()) {
        gDataDirectories.push_back(dir);
    }
}

void removeDataDirectory(const std::string& dir) {
    auto it = std::find(gDataDirectories.begin(), gDataDirectories.end(), dir);
    if (it != gDataDirectories.end()) {
        gDataDirectories.erase(it);
    }
}

bool isDevelopmentMode() {
    static bool initialized = false;
    static bool devMode = false;

    if (!initialized) {
        std::string value;
        #ifdef DEBUG
        devMode = true;
        #else
        devMode = getEnvironmentVariable("FALCOR_DEVMODE", value) && value == "1";
        #endif
        initialized = true;
    }

    return devMode;
}

std::string canonicalizeFilename(const std::string& filename) {
    fs::path path(replaceSubstring(filename, "\\", "/"));
    return fs::exists(path) ? fs::canonical(path).string() : "";
}

bool findFileInDataDirectories(const std::string& filename, std::string& fullPath) {
    // Check if this is an absolute path
    if (fs::path(filename).is_absolute()) {
        fullPath = canonicalizeFilename(filename);
        return !fullPath.empty(); // Empty fullPath means path doesn't exist
    }

    for (const auto& dir : gDataDirectories) {
        fullPath = canonicalizeFilename(dir + '/' + filename);
        if (doesFileExist(fullPath)) {
            return true;
        }
    }

    return false;
}

bool findFileInDataDirectories(const fs::path& path, fs::path& fullPath) {
    std::string file_path;
    if (findFileInDataDirectories(path.string(), file_path)) {
        fullPath = fs::path(file_path);
        return true;
    }
    return false;
}

bool findFilesInDataDirectories(const std::string& searchPath, const std::regex& regex, std::vector<std::string>& filenames) {
    // Check if searchPath exists
    if (!fs::exists(fs::path(searchPath))) {
        logWarning("Search path \"" + searchPath + "\" does not exist !!!");
        return false;
    }

    bool result = false;
    const fs::directory_iterator end;
    
    for (fs::directory_iterator iter{searchPath}; iter != end; iter++) {
        const std::string fileName = iter->path().filename().string();
        if (fs::is_regular_file(*iter)) {
            if (std::regex_match(fileName, regex)) {
                filenames.push_back(iter->path().string());
                result = true;
            }
        }
    }
    
    return result;
}

const std::vector<fs::path>& getShaderDirectoriesList() {
    return gShaderDirectories;
}

bool findFileInShaderDirectories(const fs::path& path, fs::path& fullPath) {
    // Check if this is an absolute path.
    if (path.is_absolute()) {
        if (fs::exists(path)) {
            fullPath = fs::canonical(path);
            return true;
        }
    }

    // Search in other paths.
    for (const auto& dir : gShaderDirectories) {
        fullPath = dir / path;
        if (fs::exists(fullPath)) {
            fullPath = fs::canonical(fullPath);
            return true;
        }
    }

    return false;
}

uint32_t getNextPowerOf2(uint32_t a) {
    // TODO: With C++20 we could use std::bit_ceil instead.
    a--;
    a |= a >> 1;
    a |= a >> 2;
    a |= a >> 4;
    a |= a >> 8;
    a |= a >> 16;
    a++;
    return a;
}

bool findFileInShaderDirectories(const fs::path& path, std::string& fullPath) {
    fs::path p;
    bool result = findFileInShaderDirectories(path, p);
    fullPath = p.string();
    return result;
}

std::string getExtensionFromPath(const fs::path& path) {
    std::string ext;
    if (path.has_extension()) {
        ext = path.extension().string();
        // Remove the leading '.' from ext.
        if (ext.size() > 0 && ext[0] == '.') ext.erase(0, 1);
        // Convert to lower-case.
        std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) { return std::tolower(c); });
    }
    return ext;
}

bool hasExtension(const fs::path& path, std::string_view ext) {
    // Remove leading '.' from ext.
    if (ext.size() > 0 && ext[0] == '.') ext.remove_prefix(1);

    std::string pathExt = getExtensionFromPath(path);

    if (ext.size() != pathExt.size()) return false;

    return std::equal(ext.rbegin(), ext.rend(), pathExt.rbegin(),
        [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

bool findFileInRenderPassDirectories(const std::string& filename, std::string& fullPath) {
    for (const auto& dir : gRenderPassDirectories) {
        fullPath = canonicalizeFilename(dir + '/' + filename);
        if (doesFileExist(fullPath)) {
            LOG_DBG("RenderPass library: %s found as: %s", filename.c_str(), fullPath.c_str());
            return true;
        }
    }
    return false;
}

bool findAvailableFilename(const std::string& prefix, const std::string& directory, const std::string& extension, std::string& filename) {
    for (uint32_t i = 0; i < (uint32_t)-1; i++) {
        std::string newPrefix = prefix + '.' + std::to_string(i);
        filename = directory + '/' + newPrefix + "." + extension;

        if (doesFileExist(filename) == false) {
            return true;
        }
    }
    should_not_get_here();
    filename = "";
    return false;
}

std::string stripDataDirectories(const std::string& filename) {
    std::string stripped = filename;
    std::string canonFile = canonicalizeFilename(filename);

    for (const auto& dir : gDataDirectories) {
        std::string canonDir = canonicalizeFilename(dir);

        if (canonDir.size() && hasPrefix(canonFile, canonDir, false)) {
            // canonicalizeFilename adds trailing \\ to drive letters and removes them from paths containing folders
            // The entire prefix directory including the slash should be removed
            bool trailingSlash = canonDir.back() == '\\' || canonDir.back() == '/';
            size_t len = trailingSlash ? canonDir.length() : canonDir.length() + 1;
            std::string tmp = canonFile.erase(0, len);
            
            if (tmp.length() < stripped.length()) {
                stripped = tmp;
            }
        }
    }

    return stripped;
}

std::string swapFileExtension(const std::string& str, const std::string& currentExtension, const std::string& newExtension) {
    if (hasSuffix(str, currentExtension)) {
        std::string ret = str;
        return (ret.erase(ret.rfind(currentExtension)) + newExtension);
    } else {
        return str;
    }
}

std::string getDirectoryFromFile(const std::string& filename) {
    fs::path path = filename;
    return path.has_filename() ? path.parent_path().string() : filename;
}

std::string getExtensionFromFile(const std::string& filename) {
    fs::path path = filename;
    std::string ext;
    if (path.has_extension()) {
        // remove the leading '.' that filesystem gives us
        ext = path.extension().string();
        if (hasPrefix(ext, "."))   ext = ext.substr(1, ext.size());
    }
    return ext;
}

std::string getFilenameFromPath(const std::string& filename) {
    return fs::path(filename).filename().string();
}

std::string readFile(const std::string& filename) {
    std::ifstream filestream(filename);
    std::string str;
    filestream.seekg(0, std::ios::end);
    str.reserve(filestream.tellg());
    filestream.seekg(0, std::ios::beg);
    str.assign(std::istreambuf_iterator<char>(filestream), std::istreambuf_iterator<char>());
    return str;
}

}  // namespace Falcor
