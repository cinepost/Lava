#ifndef LAVA_UTILS_UT_FSYS_H_
#define LAVA_UTILS_UT_FSYS_H_

#include <string>

#include "lava_utils_lib/logging.h"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;


namespace lava { namespace ut { namespace fsys {

/*
 * Get File extension from File path or File Name
 */
std::string getFileExtension(std::string file_path);

bool writeBinaryFile(const fs::path &filepath, const uint8_t* pData, uint32_t count);

bool isDirectoryExists(const fs::path &directoryPath, bool createMissing);

fs::path getDefaultTempDirPath();

fs::path getDefaultLavaTempDirPath(bool createMissing);

fs::path getTempDirPath(const std::string& name, bool createMissing);

}}} // namespace lava::ut::fsys

#endif // LAVA_UTILS_UT_FSYS_H_