#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

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

}}}  // namespace lava::ut::fsys
