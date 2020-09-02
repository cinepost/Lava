#ifndef SRC_LAVA_LIB_RENDERER_IFACE_LSD_H_
#define SRC_LAVA_LIB_RENDERER_IFACE_LSD_H_

#include <memory>


namespace lava {

class RendererIfaceLSD {
 public:
    using SharedPtr = std::shared_ptr<RendererIfaceLSD>;
    
    SharedPtr create();

 private:
    RendererIfaceLSD();
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_IFACE_LSD_H_