#ifndef SRC_LAVA_LIB_RENDERER_IFACE_LSD_H_
#define SRC_LAVA_LIB_RENDERER_IFACE_LSD_H_

#include <memory>

#include "../renderer_iface_base.h"

namespace lava {

class RendererIfaceLSD: public RendererIfaceBase {

 public:
    RendererIfaceLSD(Renderer *renderer);
    ~RendererIfaceLSD();
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_IFACE_LSD_H_