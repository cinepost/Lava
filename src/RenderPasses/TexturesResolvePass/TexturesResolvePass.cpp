#include <algorithm>
#include "TexturesResolvePass.h"

#include "Falcor/Utils/Debug/debug.h"

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerClass("TexturesResolve", "Resolves sparse textures tiles to be loaded", TexturesResolvePass::create);
}

const char* TexturesResolvePass::kDesc = "Creates a depth-buffer using the scene's active camera";

namespace {
    const std::string kProgramFile = "RenderPasses/TexturesResolvePass/TexturesResolvePass.ps.slang";

    const std::string kTileData = "tileData";
    const std::string kDebugColor = "debugColor";

    const std::string kTexResolveData = "gTexResolveData";

    const std::string kParameterBlockName = "gResolveData";

}  // namespace

void TexturesResolvePass::parseDictionary(const Dictionary& dict) {
    float3 a;
}

Dictionary TexturesResolvePass::getScriptingDictionary() {
    Dictionary d;
    return d;
}

TexturesResolvePass::SharedPtr TexturesResolvePass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    return SharedPtr(new TexturesResolvePass(pRenderContext->device(), dict));
}

TexturesResolvePass::TexturesResolvePass(Device::SharedPtr pDevice, const Dictionary& dict): RenderPass(pDevice) {
    Program::Desc desc;
    desc.addShaderLibrary(kProgramFile).vsEntry("vsMain").psEntry("psMain");

    GraphicsProgram::SharedPtr pProgram = GraphicsProgram::create(pDevice, desc);

    mpState = GraphicsState::create(pDevice);
    mpState->setProgram(pProgram);
    mpFbo = Fbo::create(pDevice);

    //mpTexResolveDataBuffer = Buffer::createStructured(pDevice, pProgram.get(), "gTexResolveData", 256);
    //mpTexResolveDataBuffer = Buffer::createStructured(pDevice, sizeof(TexturesResolveData), 256);
    
    //ParameterBlockReflection::SharedConstPtr pReflection = pProgram->getReflector()->getParameterBlock(kParameterBlockName);
    //assert(pReflection);

    //mpDataBlock = ParameterBlock::create(pDevice, pReflection);

    // Allocate GPU buffers.
    //LOG_WARN("Create structured buffer");    
    //mpTexResolveDataBuffer = Buffer::createStructured(pDevice, mpDataBlock["texData"], 256, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
    //mpTexResolveDataBuffer->setName("TexturesResolvePass::mpTexResolveDataBuffer");
    //if (mpTexResolveDataBuffer->getStructSize() != sizeof(TexturesResolveData)) throw std::runtime_error("Struct TexturesResolveData size mismatch between CPU/GPU");
    //LOG_WARN("Structured buffer created");


    parseDictionary(dict);
}

RenderPassReflection TexturesResolvePass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;

    //reflector.addOutput(kTileData, "TileData-buffer").format(mTileDataFormat).texture2D(0, 0, 0);
    reflector.addOutput(kDebugColor, "DebugColor-buffer").format(mTileDataDebugFormat).texture2D(0, 0, 0);
    return reflector;
}
/*
void TexturesResolvePass::initResources() {
    GraphicsProgram::SharedPtr pProgram = GraphicsProgram::createFromFile(mpDevice, "Scene/SceneBlock.slang", "", "main", getSceneDefines());
    ParameterBlockReflection::SharedConstPtr pReflection = pProgram->getReflector()->getParameterBlock(kParameterBlockName);
    assert(pReflection);

    mpSceneBlock = ParameterBlock::create(mpDevice, pReflection);
    mpMeshesBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kMeshBufferName], (uint32_t)mMeshDesc.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
    mpMeshesBuffer->setName("Scene::mpMeshesBuffer");
    mpMeshInstancesBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kMeshInstanceBufferName], (uint32_t)mMeshInstanceData.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
    mpMeshInstancesBuffer->setName("Scene::mpMeshInstancesBuffer");

    mpMaterialsBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kMaterialsBufferName], (uint32_t)mMaterials.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
    mpMaterialsBuffer->setName("Scene::mpMaterialsBuffer");

    if (mLights.size())
    {
        mpLightsBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kLightsBufferName], (uint32_t)mLights.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpLightsBuffer->setName("Scene::mpLightsBuffer");
    }
}
*/
void TexturesResolvePass::updateTexturesResolveData() {
 
}

void TexturesResolvePass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    mpScene = pScene;
    if (mpScene) {
        mpState->getProgram()->addDefines(mpScene->getSceneDefines());
        //updateTexturesResolveData();
    }
    mpVars = GraphicsVars::create(pRenderContext->device(), mpState->getProgram()->getReflector());
}

void TexturesResolvePass::initDepth(RenderContext* pContext, const RenderData& renderData) {
    mpState->setDepthStencilState(nullptr);
  
    if (mpFbo->getDepthStencilTexture() == nullptr) {
        auto pDepth = Texture::create2D(pContext->device(), 1920, 1080, ResourceFormat::D32Float, 1, 1, nullptr, Resource::BindFlags::DepthStencil);
        mpFbo->attachDepthStencilTarget(pDepth);

        pContext->clearDsv(pDepth->getDSV().get(), 1, 0);
    }
}


void TexturesResolvePass::execute(RenderContext* pContext, const RenderData& renderData) {
    initDepth(pContext, renderData);

    const auto& pTileData = renderData[kTileData]->asTexture();
    const auto& pDebugData = renderData[kDebugColor]->asTexture();
    mpFbo->attachColorTarget(pTileData, 0);
    mpFbo->attachColorTarget(pDebugData, 1);

    mpState->setFbo(mpFbo);
    //pContext->clearRtv(pTileData->getRTV().get(), {0, 0, 0, 0});
    pContext->clearRtv(pDebugData->getRTV().get(), {0, 0, 0, 0});

    //pContext->clearUAVCounter(mpTexResolveDataBuffer, 0);

    std::vector<MaterialResolveData> materialsResolveBuffer;
    if (mpScene) {
        uint32_t materialsCount = mpScene->getMaterialCount();

        for( uint32_t m_i = 0; m_i < materialsCount; m_i++ ) {
            auto pMaterial =  mpScene->getMaterial(m_i);
            auto materialResources = pMaterial->getResources();

            std::vector<Texture::SharedPtr> materialTextures;

            if(materialResources.baseColor) {
                if (materialResources.baseColor->isSparse()) {
                    materialTextures.push_back(materialResources.baseColor);
                }
            }

            MaterialResolveData materialResolveData = {};

            size_t virtualTexturesCount = std::min((size_t)MAX_VTEX_COUNT_PER_MATERIAL, materialTextures.size());
            materialResolveData.virtualTexturesCount = virtualTexturesCount;
            for( size_t t_i = 0; t_i < virtualTexturesCount; t_i++) {
                auto &textureData = materialResolveData.virtualTextures[t_i];
                auto &pTexture = materialTextures[t_i];

                textureData.textureID = pTexture->id();
                textureData.width = pTexture->getWidth();
                textureData.height = pTexture->getHeight();
                textureData.mipLevelsCount = pTexture->getMipCount();

                auto pageRes = pTexture->getSparsePageRes();
                textureData.pageSizeW = pageRes.x;
                textureData.pageSizeH = pageRes.y;
                textureData.pageSizeD = pageRes.z;
                
                textureData.testColor = {1, 0, 0, 1};
            }

            materialsResolveBuffer.push_back(materialResolveData);
        }

        auto buffer = Buffer::createStructured(mpDevice, sizeof(MaterialResolveData), materialsCount, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, materialsResolveBuffer.data(), true);
        mpVars->setBuffer("materialsResolveData", buffer);
    }

    if (mpScene) {
        mpVars["PerFrameCB"]["gRenderTargetDim"] = float2(mpFbo->getWidth(), mpFbo->getHeight());
        mpVars["PerFrameCB"]["materialsResolveDataSize"] = materialsResolveBuffer.size();
        mpScene->render(pContext, mpState.get(), mpVars.get());
    }
}

TexturesResolvePass& TexturesResolvePass::setDepthStencilState(const DepthStencilState::SharedPtr& pDsState) {
    mpState->setDepthStencilState(pDsState);
    return *this;
}

TexturesResolvePass& TexturesResolvePass::setRasterizerState(const RasterizerState::SharedPtr& pRsState) {
    mpRsState = pRsState;
    mpState->setRasterizerState(mpRsState);
    return *this;
}

void TexturesResolvePass::renderUI(Gui::Widgets& widget) {

}
