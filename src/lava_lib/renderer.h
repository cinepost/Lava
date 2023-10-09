#ifndef SRC_LAVA_LIB_RENDERER_H_
#define SRC_LAVA_LIB_RENDERER_H_

#include "lava_dll.h"

#include <cstddef>
#include <string>
#include <vector>
#include <memory>

#include "types.h"

#include "Falcor/Falcor.h"
#include "Falcor/FalcorExperimental.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/DeviceManager.h"
#include "Falcor/Utils/Timing/FrameRate.h"
#include "Falcor/Core/Renderer.h"
#include "Falcor/Scene/Camera/Camera.h"

#include "Falcor/Utils/SampleGenerators/StratifiedSamplePattern.h"

#include "RenderPasses/AccumulatePass/AccumulatePass.h"
#include "RenderPasses/DepthPass/DepthPass.h"
#include "RenderPasses/EnvironmentPass/EnvPass.h"
#include "RenderPasses/TexturesResolvePass/TexturesResolvePass.h"
#include "RenderPasses/RTXDIPass/RTXDIPass.h"

#include "aov.h"
#include "scene_builder.h"

namespace lava {

class MaterialX;

class LAVA_API Renderer: public std::enable_shared_from_this<Renderer> {
  public:
    struct Config {
      // Rendering options
      bool useRaytracing = true;
      bool useVirtualTexturing = false;
      bool useAsyncGeometryProcessing = true;
      bool generateMeshlets = false;

      bool        forceVirtualTexturesReconversion = false;
      std::string virtualTexturesCompressionQuality = "high";
      std::string virtualTexturesCompressorType = "blosclz";
      uint8_t     virtualTexturesCompressionLevel = 9;

      std::string tangentGenerationMode = "mikkt";
      std::string cullMode = "back";
    };

    enum class SamplePattern : uint32_t {
      Center,
      DirectX,
      Halton,
      Stratified,
    };

  	struct GraphData {
      Falcor::RenderGraph::SharedPtr pGraph;
      std::string mainOutput;
      bool showAllOutputs = false;
      std::vector<std::string> originalOutputs;
      std::unordered_map<std::string, uint32_t> graphOutputRefs;
  	};

    // __HYDRA__ oriented structs begin ...

    struct FrameInfo {
      uint32_t imageWidth = 0;
      uint32_t imageHeight = 0;
      uint32_t imageSamples = 16;           // 0 for continuous rendering. 16 is default, roughly equal to 
      uint32_t frameNumber = 0;
      Falcor::uint4 renderRegion = {0, 0, 0, 0}; // default full frame {left, top, width, height}

      inline Falcor::uint2 renderRegionDims() const {
        if ((renderRegion[2] == 0) || (renderRegion[3] == 0)) return {imageWidth, imageHeight};
        return {std::min(imageWidth, renderRegion[2] - renderRegion[0] + 1), std::min(imageHeight, renderRegion[3] - renderRegion[1] + 1)};
      }

      inline uint32_t getImageHeight() const { return imageHeight; }
      inline uint32_t getImageWidth() const { return imageWidth; }

      inline uint32_t regionWidth() const { return (renderRegion[2] == 0 ? imageWidth : (renderRegion[2] - renderRegion[0] + 1)); }
      inline uint32_t regionHeight() const { return (renderRegion[3] == 0 ? imageWidth : (renderRegion[3] - renderRegion[1] + 1)); }
    };

  public:
 	  virtual ~Renderer();
    using SharedPtr = std::shared_ptr<Renderer>;
 	  using UniquePtr = std::unique_ptr<Renderer>;

  public:
    static SharedPtr create(Device::SharedPtr pDevice);
    inline Falcor::Device::SharedPtr device() const { return mpDevice; };

 	  bool loadScript(const std::string& file_name);
 	  bool addMaterialX(Falcor::MaterialX::UniquePtr pMaterialX);

  public:
    inline lava::SceneBuilder::SharedPtr sceneBuilder() const { return mpSceneBuilder; };
    bool init(const Config& config);
    inline bool isInited() const { return mInited; }

    inline const std::map<std::string, AOVPlane::SharedPtr>& aovPlanes() const { return mAOVPlanes; }

    AOVPlane::SharedPtr addAOVPlane(const AOVPlaneInfo& info);
    AOVPlane::SharedPtr getAOVPlane(const AOVName& name);
    bool deleteAOVPlane(const AOVName& name);
    void setAOVPlaneState(const AOVName& name, AOVPlane::State state);
    inline AOVPlane::SharedConstPtr getAOVPlane(const AOVName& name) const { return getAOVPlane(name); };
    bool hasAOVPlane(const AOVName& name) const;

    bool prepareFrame(const FrameInfo& frame_info); // prepares/resets frame rendering
    void renderSample();
    const uint8_t*  getAOVPlaneImageData(const AOVName& name);

    inline Falcor::Camera::SharedPtr currentCamera() { return mpCamera; };

    /** Query AOV output (if exist) geometry
      \param[in] AOV name/path. Example: "AccumulatePass.output"
      \param[out] AOV geometry information.
      \return True if AOV exist otherwise False.
    */
    bool queryAOVPlaneGeometry(const AOVName& name, AOVPlaneGeometry& aov_plane_geometry) const;

    /** Well.. This is kind of ugly, but it does it's job. This is basically all purpose dictionary that might be used
        by any render pass in the rendering graph of the renderer. So for example we can store some named value here hoping that some render pass
        recognizes it and behaves in a way we wanted to. Ideally if we say set "bias" key to some value like 0.001 we expect
        that any render pass that uses varible bias ray tracing would adjust itsef acoording to this value.
        Because render graphs are dynamic in their nature we are yet to come up with a better approach.
      \return Dictionary.
    */ 

    Falcor::Dictionary& getRenderPassesDict() { mDirty = true; return mRenderPassesDict; };
    const Falcor::Dictionary& getRenderPassesDict() const { return mRenderPassesDict; };

    Falcor::Dictionary& getRendererConfDict() { mDirty = true; return mRendererConfDict; }
    const Falcor::Dictionary& getRendererConfDict() const { return mRendererConfDict; };
  
#ifdef SCRIPTING
 	static void registerBindings(pybind11::module& m);
#endif

  protected:
    Falcor::RenderGraph::SharedConstPtr  renderGraph() const { return mpRenderGraph; };

  private:
 	  void addGraph(const Falcor::RenderGraph::SharedPtr& pGraph);
 	  void initGraph(const Falcor::RenderGraph::SharedPtr& pGraph, GraphData* pData);
 	  std::vector<std::string> getGraphOutputs(const Falcor::RenderGraph::SharedPtr& pGraph);

    void resolvePerFrameSparseResourcesForActiveGraph(Falcor::RenderContext* pRenderContext);
 	  void executeActiveGraph(Falcor::RenderContext* pRenderContext);
 	
    void beginFrame(Falcor::RenderContext* pRenderContext, const Falcor::Fbo::SharedPtr& pTargetFbo);
 	  void endFrame(Falcor::RenderContext* pRenderContext, const Falcor::Fbo::SharedPtr& pTargetFbo);

  private:
    /** This should be called before any graph execution
    */
    void finalizeScene(const FrameInfo& frame_info);

    void createRenderGraph(const FrameInfo& frame_info);

    void bindAOVPlanesToResources();

  private:
    Renderer(Device::SharedPtr pDevice);
    
 	  std::vector<std::string> 	  mErrorMessages;
 	  Falcor::Device::SharedPtr 	mpDevice;

 	  int mGpuId;
    
 	  bool mIfaceAquired = false;
    bool mGlobalDataInited = false;

    Falcor::Camera::SharedPtr       mpCamera;

 	  Falcor::Fbo::SharedPtr 		      mpTargetFBO;		///< The FBO available to renderers
 	  Falcor::FrameRate*              mpFrameRate;
    Falcor::Clock* 	                mpClock;
    Falcor::ArgList 			          mArgList;

    lava::SceneBuilder::SharedPtr   mpSceneBuilder;
    Falcor::Sampler::SharedPtr      mpSampler;
    std::vector<GraphData>          mGraphs;
    uint32_t mActiveGraph = 0;

    Config                          mCurrentConfig;
    FrameInfo                       mCurrentFrameInfo;
    std::uint32_t                   mCurrentSampleNumber = 0;

    ///
    float2 mInvFrameDim;
    CPUSampleGenerator::SharedPtr   mpSampleGenerator = nullptr;
    
    Falcor::RenderGraph::SharedPtr  mpRenderGraph = nullptr;
    Falcor::RenderGraph::SharedPtr  mpTexturesResolvePassGraph = nullptr;

    AccumulatePass::SharedPtr       mpAccumulatePass = nullptr;
    DepthPass::SharedPtr            mpDepthPrePass = nullptr;
    DepthPass::SharedPtr            mpDepthPass = nullptr;
    EnvPass::SharedPtr              mpEnvPass = nullptr;
    TexturesResolvePass::SharedPtr  mpTexturesResolvePass = nullptr;
    ///

    Falcor::Dictionary              mRendererConfDict;
    Falcor::Dictionary              mRenderPassesDict;

    std::map<std::string, AOVPlane::SharedPtr> mAOVPlanes;

    std::map<std::string, Falcor::MaterialX::SharedPtr> mMaterialXs; ///< Materialx materials map

    bool mMainAOVPlaneExist = false;
    bool mInited = false;
    bool mDirty = true;

  private:
    // RenderFrame private
    Scene* _mpScene = nullptr;

    friend class RendererIface;
}; 

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_H_
