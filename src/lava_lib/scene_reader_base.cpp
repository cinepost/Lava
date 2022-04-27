#include <chrono>

#include "renderer_iface.h"
#include "scene_reader_base.h"
#include "lava_utils_lib/logging.h"

namespace lava {

ReaderBase::ReaderBase() { }

ReaderBase::~ReaderBase() { }

bool ReaderBase::readStream(std::istream &input) {
    bool result;
    if(!isInitialized())
        return false;

    auto t1 = std::chrono::high_resolution_clock::now();
    result = parseStream(input);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>( t2 - t1 ).count();

    LLOG_DBG << "Scene parsed in: " << duration << " sec.";

    return result;
}

}  // namespace lava