#ifndef SRC_LAVA_LIB_RENDERER_IFACE_H_
#define SRC_LAVA_LIB_RENDERER_IFACE_H_

#include <memory>
#include <map>

#include "display.h"

namespace lava {

class Renderer;
class SceneBuilder;

class RendererIface {
 public:
    struct FrameData {
        std::string imageFileName = "";
        uint imageWidth = 0;
        uint imageHeight = 0;
        uint imageSamples = 0;

        std::vector<std::pair<std::string, std::vector<std::string>>>   displayStringParameters;
        std::vector<std::pair<std::string, std::vector<int>>>           displayIntParameters;
        std::vector<std::pair<std::string, std::vector<float>>>         displayFloatParameters;
    };

 public:
    using UniquePtr = std::unique_ptr<RendererIface>;

    RendererIface(Renderer *renderer);
    ~RendererIface();

    void setEnvVariable(const std::string& key, const std::string& value);

    /** get string expanded with local env variables
     */
    std::string getExpandedString(const std::string& s);

    /**
     */
    bool loadDisplay(const std::string& display_name);

    /**
    */
    bool openDisplay(const std::string& image_name, uint width, uint height);

    /**
    */
    bool closeDisplay();

    /**
     */
    bool loadScriptFile(const std::string& file_name);

    /**
    */
    std::shared_ptr<SceneBuilder> getSceneBuilder();

    bool initRenderer();
    bool isRendererInitialized() const;
    void renderFrame(const FrameData& frame_data);

 private:
    std::map<std::string, std::string>  mEnvmap;
    Renderer                            *mpRenderer;

    friend class Renderer;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_IFACE_H_