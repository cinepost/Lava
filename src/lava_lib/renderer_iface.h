#ifndef SRC_LAVA_LIB_RENDERER_IFACE_H_
#define SRC_LAVA_LIB_RENDERER_IFACE_H_

#include <memory>
#include <map>

namespace lava {

class Renderer;

class RendererIface {
 public:
    using UniquePtr = std::unique_ptr<RendererIface>;

    RendererIface(Renderer *renderer);
    ~RendererIface();

    void setEnvVariable(const std::string& key, const std::string& value);

    /* get string expanded with local env variables
     */
    std::string getExpandedString(const std::string& s);

    /*
     */
    bool loadDisplay(const std::string& display_name);

    /*
     */
    bool loadScript(const std::string& file_name);

 //protected:
    bool initRenderer();
    void renderFrame();

 private:
    std::map<std::string, std::string>  mEnvmap;
    Renderer                            *mpRenderer;

    friend class Renderer;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_IFACE_H_