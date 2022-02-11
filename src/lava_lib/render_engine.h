#ifndef SRC_LAVA_LIB_RENDERENGINE_H_
#define SRC_LAVA_LIB_RENDERENGINE_H_

/*

#include <memory>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Scene/Scene.h"
#include "Falcor/RenderGraph/RenderGraph.h"
#include "Falcor/Utils/ConfigStore.h"

namespace lava {

class RenderEngine : public std::enable_shared_from_this<RenderEngine> {
  public:
  	using SharedPtr = std::shared_ptr<RenderEngine>;

	  enum class EngineType {
        Rasterizer,
        Hybrid,
        Raytracer
    };

  public:
    static RenderEngine::SharedPtr createEngine(EngineType type, Falcor::Device::SharedPtr pDevice, const Falcor::ConfigStore &configStore); 

    virtual Falcor::RenderGraph::SharedPtr createRenderingGraph(const Falcor::Scene::SharedPtr& pScene);
    virtual Falcor::RenderGraph::SharedPtr createTexturesResolveGraph(const Falcor::Scene::SharedPtr& pScene);

  private:
    RenderEngine(Falcor::Device::SharedPtr pDevice, const Falcor::ConfigStore &configStore);

  protected:
    Falcor::Device::SharedPtr mpDevice = nullptr;
    Falcor::ConfigStore const *mpConfigStore;

};

class RasterizerEngine : public RenderEngine, public Falcor::inherit_shared_from_this<RenderEngine, RasterizerEngine> {
  public:
    using SharedPtr = std::shared_ptr<RasterizerEngine>;

  public:
    static RenderEngine::SharedPtr create(Falcor::Device::SharedPtr pDevice, const Falcor::ConfigStore &configStore);

};

class RaytracerEngine : public RenderEngine, public Falcor::inherit_shared_from_this<RenderEngine, RaytracerEngine> {
  public:
    using SharedPtr = std::shared_ptr<RaytracerEngine>;

  public:
    static RenderEngine::SharedPtr create(Falcor::Device::SharedPtr pDevice, const Falcor::ConfigStore &configStore);

};

class HybridEngine : public RenderEngine, public Falcor::inherit_shared_from_this<RenderEngine, HybridEngine> {
  public:
    using SharedPtr = std::shared_ptr<HybridEngine>;

  public:
    static RenderEngine::SharedPtr create(Falcor::Device::SharedPtr pDevice, const Falcor::ConfigStore &configStore);

};

class ToonEngine : public RenderEngine, public Falcor::inherit_shared_from_this<RenderEngine, ToonEngine> {
  public:
    using SharedPtr = std::shared_ptr<ToonEngine>;

  public:
    static RenderEngine::SharedPtr create(Falcor::Device::SharedPtr pDevice, const Falcor::ConfigStore &configStore);

};

}  // namespace lava
*/
#endif  // SRC_LAVA_LIB_RENDERENGINE_H_