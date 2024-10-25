#include "logging.h"
#include "ut_fsys.h"

namespace lava { namespace ut { namespace fsys {

/*
 * Get File extension from File path or File Name
 */
std::string getFileExtension(std::string file_path) {
    // Create a Path object from given string
    fs::path pathObj(file_path);
    // Check if file name in the path object has extension
    if (pathObj.has_extension()) {
        // Fetch the extension from path object and return
        return pathObj.extension().string();
    }
    // In case of no extension return empty string
    return "";
}

fs::path getDefaultTempDirPath() {
    return fs::temp_directory_path();
}

bool isDirectoryExists(const fs::path &directoryPath, bool createMissing) {
    if(directoryPath.empty()) return false;

    if(fs::is_directory(directoryPath)) return true;

    if(createMissing) {
        fs::create_directories(directoryPath);
        return isDirectoryExists(directoryPath, false);
    }

    return false;
}

fs::path getDefaultLavaTempDirPath(bool createMissing) {
    auto lava_temp_directory_path = getDefaultTempDirPath() / "lava";
    
    if(!isDirectoryExists(lava_temp_directory_path, createMissing)) {
        LLOG_WRN << "Directory " << lava_temp_directory_path << " doesn't exist !";
    }
    
    return lava_temp_directory_path;
}

fs::path getTempDirPath(const std::string& name, bool createMissing) {
    auto temp_directory_path = getDefaultLavaTempDirPath(createMissing);

    if(name.empty()) return temp_directory_path;
    temp_directory_path /= name;

    if(!isDirectoryExists(temp_directory_path, createMissing)) {
        LLOG_WRN << "Directory " << temp_directory_path << " doesn't exist !";
    }

    return temp_directory_path;
}

bool writeBinaryFile(const fs::path &filepath, const uint8_t* pData, uint32_t count) {
    if(filepath.empty()) return false;
    std::ofstream file;

    file.open(filepath.string(), std::ios::out | std::ios::binary | std::ios::trunc);

    if (!file.is_open()) {
        LLOG_ERR << "Failed to open file: " << filepath;
        return false;
    }

    if (count > 0) {
        file.write(reinterpret_cast<const char *>(pData), count);
    }

    file.close();
    return true;
}

}}}  // namespace lava::ut::fsys
