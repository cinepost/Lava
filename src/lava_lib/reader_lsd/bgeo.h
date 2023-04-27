#ifndef SRC_LAVA_LIB_READER_LSD_BGEO_H_
#define SRC_LAVA_LIB_READER_LSD_BGEO_H_

#include <vector>
#include <array>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include <variant>
#include <map>

namespace lava {

    using uint = uint32_t;

namespace bgeo {

class Bgeo {

 public:


 private:
    std::string mFileversion;
    bool mHasIndex = false;
    uint mPointCount;
    uint mVertexCount;
    uint mPrimCount;

};

}  // namespace bgeo

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_BGEO_H_