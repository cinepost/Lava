#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Utils/Color/ColorGenerationUtils.h"
#include "Falcor/Utils/SampleGenerators/StratifiedSamplePattern.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"

#include "glm/gtc/random.hpp"

#include "glm/gtx/string_cast.hpp"

#include "DebugShadingPass.h"

static const uint32_t meshletColorCycleSize = 1024;

const RenderPass::Info DebugShadingPass::kInfo
{
    "DebugShadingPass",
    "Debug pass."
};

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(DebugShadingPass::kInfo, DebugShadingPass::create);
}

namespace {
    const char kShaderFile[] = "RenderPasses/DebugShadingPass/DebugShadingPass.cs.slang";

    const std::string kInputColor           = "color";
    const std::string kInputVBuffer         = "vbuffer";
    const std::string kInputDepth           = "depth";
    const std::string kInputTexGrads        = "texGrads";
    const std::string kInputMVectors        = "mvec";

    const std::string kInputOuputMeshlet    = "meshlet_id";
    const std::string kInputOuputMicroPoly  = "micropoly_id";

    const std::string kOutputMeshletColor   = "meshlet_color";
    const std::string kOutputMicroPolyColor = "micropoly_color";

    const std::string kShaderModel = "6_5";

    const ChannelList kExtraInputChannels = {
        { kInputDepth,              "gDepth",             "Depth buffer",                   true /* optional */, ResourceFormat::Unknown           },
        { kInputTexGrads,           "gTextureGrads",      "Texture gradients",              true /* optional */, ResourceFormat::Unknown           },
        { kInputMVectors,           "gMotionVector",      "Motion vector buffer (float format)", true /* optional */                               },
        { kInputOuputMeshlet,       "gMeshletID",         "Meshlet ID",                     true /* optional */, ResourceFormat::R32Uint           },
        { kInputOuputMicroPoly,     "gMicroPolyID",       "Micro-polygon ID",               true /* optional */, ResourceFormat::R32Uint           },
    };

    const ChannelList kExtraInputOutputChannels = {
        
    };

    const ChannelList kExtraOutputChannels = {
        // Service outputs
        { "prim_id",                "gPrimID",            "Primitive id buffer",            true /* optional */, ResourceFormat::R32Float          },
        { "op_id",                  "gOpID",              "Operator id buffer",             true /* optional */, ResourceFormat::R32Float          },
        { kOutputMeshletColor,      "gMeshletColor",      "Meshlet false-color buffer",     true /* optional */, ResourceFormat::RGBA16Float       },
        { kOutputMicroPolyColor,    "gMicroPolyColor",    "MicroPolygon false-color buffer",true /* optional */, ResourceFormat::RGBA16Float       },
    };
}

DebugShadingPass::SharedPtr DebugShadingPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    auto pThis = SharedPtr(new DebugShadingPass(pRenderContext->device()));
        
    for (const auto& [key, value] : dict) {
    
    }

    return pThis;
}

Dictionary DebugShadingPass::getScriptingDictionary() {
    Dictionary d;

    return d;
}

DebugShadingPass::DebugShadingPass(Device::SharedPtr pDevice): RenderPass(pDevice, kInfo) {
    mpFalseColorGenerator = nullptr;
}

RenderPassReflection DebugShadingPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;

    const auto& texDims = compileData.defaultTexDims;

    reflector.addInputOutput(kInputColor, "Color buffer").format(ResourceFormat::Unknown);
    reflector.addInput(kInputVBuffer, "Visibility buffer in packed format").format(ResourceFormat::RGBA32Uint);
    
    addRenderPassInputs(reflector, kExtraInputChannels);
    addRenderPassInputOutputs(reflector, kExtraInputOutputChannels);
    addRenderPassOutputs(reflector, kExtraOutputChannels, Resource::BindFlags::UnorderedAccess);

    return reflector;
}

void DebugShadingPass::compile(RenderContext* pRenderContext, const CompileData& compileData) {
    mDirty = true;
    mFrameDim = compileData.defaultTexDims;
    auto pDevice = pRenderContext->device();
}

void DebugShadingPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    if(mpScene == pScene) return;
    mpScene = pScene;
}

void DebugShadingPass::execute(RenderContext* pContext, const RenderData& renderData) {
    if (!mpScene) return;

    generateMeshletColorBuffer(renderData);

    // Prepare program and vars. This may trigger shader compilation.
    // The program should have all necessary defines set at this point.
    if (!mpShadingPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("main");
        desc.addTypeConformances(mpScene->getTypeConformances());

        auto defines = mpScene->getSceneDefines();
        defines.add(getValidResourceDefines(kExtraInputChannels, renderData));
        defines.add(getValidResourceDefines(kExtraInputOutputChannels, renderData));
        defines.add(getValidResourceDefines(kExtraOutputChannels, renderData));
        defines.add("FALSE_COLOR_BUFFER_SIZE", mpMeshletColorBuffer ? std::to_string(meshletColorCycleSize) : "0");

        mpShadingPass = ComputePass::create(mpDevice, desc, defines, true);

        mpShadingPass["gScene"] = mpScene->getParameterBlock();
        mpShadingPass["gFalseColorBuffer"] = mpMeshletColorBuffer;

        // Bind mandatory input channels
        mpShadingPass["gInOutColor"] = renderData[kInputColor]->asTexture();
        mpShadingPass["gVBuffer"] = renderData[kInputVBuffer]->asTexture();

        // Bind extra input channels
        for (const auto& channel : kExtraInputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            mpShadingPass[channel.texname] = pTex;
        }

        // Bind extra input-output channels
        for (const auto& channel : kExtraInputOutputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            mpShadingPass[channel.texname] = pTex;
        }

        // Bind extra output channels as UAV buffers.
        for (const auto& channel : kExtraOutputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            mpShadingPass[channel.texname] = pTex;
        }

        auto pMeshletIDTexture = renderData[kInputOuputMeshlet]->asTexture();
        if(pMeshletIDTexture) {
            LLOG_WRN << "Meshlet id texture size " << pMeshletIDTexture->getWidth() << " " << pMeshletIDTexture->getHeight();
        }

        if (!mpFalseColorGenerator && 
            (   
                (renderData[kInputOuputMeshlet]->asTexture()   && renderData[kOutputMeshletColor]->asTexture()) || 
                (renderData[kInputOuputMicroPoly]->asTexture() && renderData[kOutputMicroPolyColor]->asTexture())
            )
        ) {
            mpFalseColorGenerator = FalseColorGenerator::create(mpDevice,meshletColorCycleSize);
        }
    }

    if(mpFalseColorGenerator) mpFalseColorGenerator->setShaderData(mpShadingPass["gFalseColorGenerator"]);

    auto cb_var = mpShadingPass["PerFrameCB"];
    cb_var["gFrameDim"] = mFrameDim;

    mpShadingPass->execute(pContext, mFrameDim.x, mFrameDim.y);

    mDirty = false;
}

void DebugShadingPass::generateMeshletColorBuffer(const RenderData& renderData) {
    if(mpMeshletColorBuffer || !renderData[kInputOuputMeshlet]->asTexture()) return;

    static bool solidAplha = true;
    mpMeshletColorBuffer = generateRandomColorsBuffer(mpDevice, meshletColorCycleSize, mFalseColorFormat, solidAplha);
}

DebugShadingPass& DebugShadingPass::setColorFormat(ResourceFormat format) {
    return *this;
}