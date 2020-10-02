#ifndef SRC_LAVA_LIB_READER_LSD_GLOBAL_H_
#define SRC_LAVA_LIB_READER_LSD_GLOBAL_H_

#include <map>
#include <memory>
#include <variant>
#include <string>

#include "../scene_reader_base.h"
#include "attribs_container.h"


namespace lava {

namespace lsd {


class Global: public AttribsContainer {
    public:
        Global();
        ~Global() {};
};

}  // namespace lsd

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_GLOBAL_H_
