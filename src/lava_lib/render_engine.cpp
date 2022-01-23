#include "render_engine.h"

/*

#include "RenderPasses/DepthPass/DepthPass.h"
#include "RenderPasses/TexturesResolvePass/TexturesResolvePass.h"

namespace lava {

RenderEngine::SharedPtr RenderEngine::createEngine(EngineType type, Falcor::Device::SharedPtr pDevice, const Falcor::ConfigStore &configStore) {
	RenderEngine::SharedPtr pEngine = nullptr;

	switch (type) {
        case EngineType::Rasterizer:
            pEngine = RasterizerEngine::create(pDevice, configStore);
            break;
        case EngineType::Raytracer:
            pEngine = RaytracerEngine::create(pDevice, configStore);
            break;
        case EngineType::Hybrid:
        default:
            pEngine = HybridEngine::create(pDevice, configStore);
            break;
    }

    return pEngine;
}

RenderEngine::RenderEngine(Falcor::Device::SharedPtr pDevice, const Falcor::ConfigStore &configStore) {
	mpDevice = pDevice;
	mpConfigStore = &configStore; // configStore is a sigleton so it's safe
}

Falcor::RenderGraph::SharedPtr RenderEngine::createTexturesResolveGraph(const Falcor::Scene::SharedPtr& pScene) {
	auto pRenderContext = mpDevice->getRenderContext();

	auto vtexResolveChannelOutputFormat = ResourceFormat::RGBA8Unorm;
    auto pTexturesResolvePassGraph = RenderGraph::create(mpDevice, imageSize, vtexResolveChannelOutputFormat, "VirtualTexturesGraph");

    // Depth pre-pass
    Falcor::Dictionary depthPrePassDictionary;
    depthPrePassDictionary["disableAlphaTest"] = true; // no virtual textures loaded at this point

    auto pDepthPrePass = DepthPass::create(pRenderContext, depthPrePassDictionary);
    pDepthPrePass->setDepthBufferFormat(ResourceFormat::D32Float);
    pDepthPrePass->setScene(pRenderContext, pScene);
    pDepthPrePass->setCullMode(cullMode);
    pTexturesResolvePassGraph->addPass(pDepthPrePass, "DepthPrePass");

    // Vitrual textures resolve pass
    auto pTexturesResolvePass = TexturesResolvePass::create(pRenderContext);
    pTexturesResolvePass->setRasterizerState(Falcor::RasterizerState::create(rsDesc));
    pTexturesResolvePass->setScene(pRenderContext, pScene);

    pTexturesResolvePassGraph->addPass(pTexturesResolvePass, "SparseTexturesResolvePrePass");
    pTexturesResolvePassGraph->markOutput("SparseTexturesResolvePrePass.output");

    pTexturesResolvePassGraph->addEdge("DepthPrePass.depth", "SparseTexturesResolvePrePass.depth");

    return pTexturesResolvePassGraph;
}


RenderEngine::SharedPtr RasterizerEngine::create(Falcor::Device::SharedPtr pDevice, const Falcor::ConfigStore &configStore) {

}

RenderEngine::SharedPtr RaytracerEngine::create(Falcor::Device::SharedPtr pDevice, const Falcor::ConfigStore &configStore) {
	
}

RenderEngine::SharedPtr HybridEngine::create(Falcor::Device::SharedPtr pDevice, const Falcor::ConfigStore &configStore) {
	
}


}  // namespace lava
*/