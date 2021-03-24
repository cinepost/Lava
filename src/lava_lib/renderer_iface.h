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
    enum class SamplePattern : uint32_t {
        Center,
        DirectX,
        Halton,
        Stratified,
    };

    struct DisplayData {
        Display::DisplayType                                            displayType;
        Display::TypeFormat                                             typeFormat;
        std::vector<std::pair<std::string, std::vector<std::string>>>   displayStringParameters;
        std::vector<std::pair<std::string, std::vector<int>>>           displayIntParameters;
        std::vector<std::pair<std::string, std::vector<float>>>         displayFloatParameters;
    };

    /* GLobalData struct contains data that doesn't change between frames
     */
    struct GlobalData {
        uint imageWidth = 1280;
        uint imageHeight = 720;
        uint imageSamples = 1;

        SamplePattern samplePattern = SamplePattern::Stratified;
        
        double  fps = 25.0;
        bool    use_ssao = false;
    };

    /* FrameData frame specific data
     */
    struct FrameData {
        double time = 0.0;

        // image section
        std::string imageFileName = "";

        // camera section
        std::string cameraProjectionName = "perspective";
        double      cameraNearPlane = 0.01;
        double      cameraFarPlane  = 1000.0;
        glm::mat4   cameraTransform;
        double      cameraFocalLength = 1.0;
        double      cameraFrameHeight = 1.0;
    
        // ssao section
        float       ssao_distance = 0.5;
        float       ssao_factor = 1.0;
        float       ssao_precision = 1.0;
        bool        ssao_bent_normals = false;
        bool        ssao_bounce_approx = false;
    };

    struct PlaneData {
        enum class Channel: uint32_t {
            COLOR,       // RGB  channel
            COLOR_ALPHA, // RGBA channel
            NOMRAL,
            DEPTH,       // Z channel
            ALBEDO,      // Surface albedo
        };

        Channel             channel;
        Display::TypeFormat format;
        std::string         name;
    };

    struct EnvLightData {
        bool        phantom = false;
        glm::mat4   transform;
        glm::vec3   intensity = {1, 1, 1};
        std::string imageFileName = "";
    };

 public:
    using UniquePtr = std::unique_ptr<RendererIface>;

    RendererIface(std::shared_ptr<Renderer> pRenderer);
    ~RendererIface();

    void setEnvVariable(const std::string& key, const std::string& value);

    /** get string expanded with local env variables
     */
    std::string getExpandedString(const std::string& s);

    /**
    */
    //bool openDisplay(const std::string& image_name, uint width, uint height);

    /**
    */
    bool setDisplay(const DisplayData& display_data);  

    /**
    */
    bool addPlane(const PlaneData& plane_data);

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
    void initRendererGlobalData(const GlobalData& global_data);
    bool isRendererInitialized() const;
    void renderFrame(const FrameData& frame_data);

 private:
    std::map<std::string, std::string>  mEnvmap;
    std::shared_ptr<Renderer>           mpRenderer;

    std::vector<std::string>            mDeferredScriptFileNames;

    friend class Renderer;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_IFACE_H_