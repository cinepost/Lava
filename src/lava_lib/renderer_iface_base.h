#ifndef SRC_LAVA_LIB_RENDERER_IFACE_BASE_H_
#define SRC_LAVA_LIB_RENDERER_IFACE_BASE_H_

#include <memory>


namespace lava {

class Renderer;

class RendererIfaceBase {
 public:
    using SharedPtr = std::shared_ptr<RendererIfaceBase>;
    
    virtual SharedPtr create() = 0;

 private:
    RendererIfaceBase();

    
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_IFACE_LSD_H_