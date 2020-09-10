#ifndef SRC_LAVA_LIB_RENDERER_IFACE_BASE_H_
#define SRC_LAVA_LIB_RENDERER_IFACE_BASE_H_

#include <memory>
#include <map>

namespace lava {

class Renderer;

class RendererIfaceBase {
 public:
    using SharedPtr = std::shared_ptr<RendererIfaceBase>;
    
    void setEnvVariable(const std::string& key, const std::string& value);

    /*
     * get string expanded with local env variables
     */
    std::string getExpandedString(const std::string& s);

 public:
    virtual SharedPtr create() = 0;

 private:
    RendererIfaceBase();

    std::map<std::string, std::string> envmap;

    
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_IFACE_LSD_H_