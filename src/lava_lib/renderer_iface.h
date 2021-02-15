#ifndef SRC_LAVA_LIB_RENDERER_IFACE_H_
#define SRC_LAVA_LIB_RENDERER_IFACE_H_

#include <memory>
#include <map>

#include "Externals/GLM/glm/mat4x4.hpp"

#include "display.h"

namespace lava {

class Renderer;
class SceneBuilder;

class RendererIface {
 public:
    struct DisplayData {
        Display::DisplayType                                            displayType;
        std::vector<std::pair<std::string, std::vector<std::string>>>   displayStringParameters;
        std::vector<std::pair<std::string, std::vector<int>>>           displayIntParameters;
        std::vector<std::pair<std::string, std::vector<float>>>         displayFloatParameters;
    };

    struct GlobalData {
        double  fps = 25.0;
    };

    struct FrameData {
        std::string imageFileName = "";
        uint imageWidth = 0;
        uint imageHeight = 0;
        uint imageSamples = 0;

        double time = 0.0;

        std::string cameraProjectionName = "perspective";
        double      cameraNearPlane = 0.01;
        double      cameraFarPlane  = 1000.0;
        glm::mat4   cameraTransform;
        double      cameraFocalLength = 1.0;
        double      cameraFrameHeight = 1.0;
    };

    struct EnvLightData {
        bool        phantom = false;
        glm::mat4   transform;
        glm::vec3   intensity = {1, 1, 1};
        std::string imageFileName = "";
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
    bool openDisplay(const std::string& image_name, uint width, uint height);

    /**
    */
    bool setDisplay(const DisplayData& display_data);  

    /** load and execute python script file
     */
    bool loadScriptFile(const std::string& file_name);

    /** load and deffered execute (before frame being rendered) python script file.
     */
    void loadDeferredScriptFile(const std::string& file_name);

    /**
    */
    std::shared_ptr<SceneBuilder> getSceneBuilder();

    bool initRenderer();
    bool isRendererInitialized() const;
    void renderFrame(const FrameData& frame_data);

 private:
    std::map<std::string, std::string>  mEnvmap;
    Renderer                            *mpRenderer;

    std::vector<std::string>            mDeferredScriptFileNames;

    friend class Renderer;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_IFACE_H_