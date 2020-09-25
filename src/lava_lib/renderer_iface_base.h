#ifndef SRC_LAVA_LIB_RENDERER_IFACE_BASE_H_
#define SRC_LAVA_LIB_RENDERER_IFACE_BASE_H_

#include <memory>
#include <map>

namespace lava {

class Renderer;

class RendererIfaceBase {
 public:
    //using UniquePtr = std::unique_ptr<RendererIfaceBase>;

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

    RendererIfaceBase(Renderer *renderer);
    virtual ~RendererIfaceBase();

 //protected:
    bool initRenderer();
    void renderFrame();

 private:
    std::map<std::string, std::string>  mEnvmap;
    Renderer                            *mpRenderer;

    friend class Renderer;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_IFACE_LSD_H_