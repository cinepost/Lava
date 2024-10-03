#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/IndirectCommands.h"

#include "Falcor/Utils/Color/ColorGenerationUtils.h"
#include "Falcor/Utils/SampleGenerators/StratifiedSamplePattern.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"

#include "Falcor/Utils/Timing/SimpleProfiler.h"

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

    const std::string kVisibilityContainerParameterBlockName = "gVisibilityContainer";

    const std::string kInputColor           = "color";
    const std::string kInputVBuffer         = "vbuffer";
    const std::string kInputDepth           = "depth";
    const std::string kInputTexGrads        = "texGrads";
    const std::string kInputMVectors        = "mvec";
    const std::string kInputDrawCount       = "drawCount";
    const std::string kInputNormalW         = "normW";

    const std::string kInputOuputMeshlet        = "meshlet_id";
    const std::string kInputOuputMicroPoly      = "micropoly_id";
    const std::string kInputOuputTime           = "time";
    const std::string kInputOuputAUX            = "aux";

    const std::string kOutputMeshletColor       = "meshlet_color";
    const std::string kOutputMicroPolyColor     = "micropoly_color";
    const std::string kOutputTimeFalseColor     = "time_color";
    const std::string kOutputMeshletDrawColor   = "meshlet_draw";

    const std::string kShaderModel = "6_5";

    const ChannelList kExtraInputChannels = {
        { kInputDepth,              "gDepth",               "Depth buffer",                             true /* optional */, ResourceFormat::Unknown        },
        { kInputTexGrads,           "gTextureGrads",        "Texture gradients",                        true /* optional */, ResourceFormat::Unknown        },
        { kInputMVectors,           "gMotionVector",        "Motion vector buffer (float format)",      true /* optional */                                 },
        { kInputDrawCount,          "gDrawCount",           "Draw count debug buffer",                  true /* optional */, ResourceFormat::R32Uint        },
        { kInputNormalW,            "gNormW",               "Shading normal in world space",            true /* optional */, ResourceFormat::RGBA32Uint     },
    };

    const ChannelList kExtraInputOutputChannels = {
        { kInputOuputMeshlet,       "gMeshletID",           "Meshlet ID",                               true /* optional */, ResourceFormat::R32Uint        },
        { kInputOuputMicroPoly,     "gMicroPolyID",         "Micro-polygon ID",                         true /* optional */, ResourceFormat::R32Uint        },
        { kInputOuputAUX,           "gAUX",                 "Auxiliary debug buffer",                   true /* optional */, ResourceFormat::RGBA32Float    },
        { kInputOuputTime,          "gTime",                "Per-pixel execution time",                 true /* optional */, ResourceFormat::R32Uint        },        
    };

    const ChannelList kExtraOutputChannels = {
        // Service outputs
        { "normals",                "gOutNormals",          "Normals buffer",                           true /* optional */, ResourceFormat::RGBA16Float    },
        { "face_normals",           "gOutFaceNormals",      "Face Normals buffer",                      true /* optional */, ResourceFormat::RGBA16Float    },
        { "tangent_normals",        "gOutTangentNormals",   "Tangent space normals buffer",             true /* optional */, ResourceFormat::RGBA16Float    },
        { "prim_id",                "gPrimID",              "Primitive id buffer",                      true /* optional */, ResourceFormat::R32Float       },
        { "op_id",                  "gOpID",                "Operator id buffer",                       true /* optional */, ResourceFormat::R32Float       },
        { kOutputMeshletColor,      "gMeshletColor",        "Meshlet false-color buffer",               true /* optional */, ResourceFormat::RGBA16Float    },
        { kOutputMicroPolyColor,    "gMicroPolyColor",      "MicroPolygon false-color buffer",          true /* optional */, ResourceFormat::RGBA16Float    },
        { kOutputTimeFalseColor,    "gTimeFalseColor",      "GPU time false-color buffer",              true /* optional */, ResourceFormat::RGBA16Float    },
        { kOutputMeshletDrawColor,  "gMeshletDrawHeatMap",  "Meshlet draw heat map",                    true /* optional */, ResourceFormat::RGBA16Float    },
        { "uv",                     "gUV",                  "Texture coordinates buffer",               true /* optional */, ResourceFormat::RG16Float      },
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

    SimpleProfiler profile("DebugShadingPass::execute");

    generateMeshletColorBuffer(renderData);

    auto createShadingPass = [this, renderData](const Program::Desc& desc, bool transparentPass = false) {
        if(transparentPass && !mpVisibilitySamplesContainer) return ComputePass::SharedPtr(nullptr);

        auto defines = mpScene->getSceneDefines();
        defines.add(getValidResourceDefines(kExtraInputChannels, renderData));
        defines.add(getValidResourceDefines(kExtraInputOutputChannels, renderData));
        defines.add(getValidResourceDefines(kExtraOutputChannels, renderData));
        defines.add("FALSE_COLOR_BUFFER_SIZE", mpMeshletColorBuffer ? std::to_string(meshletColorCycleSize) : "0");

        if(mpVisibilitySamplesContainer) {
            if(transparentPass) defines.add("TRANSPARENT_SHADING_PASS");
            defines.add("USE_VISIBILITY_CONTAINER", "1");
            defines.add("GROUP_SIZE_X", to_string(mpVisibilitySamplesContainer->getShadingThreadGroupSize().x));
            defines.add(mpVisibilitySamplesContainer->getDefines());
        } else {
            defines.remove("TRANSPARENT_SHADING_PASS");
            defines.remove("USE_VISIBILITY_CONTAINER");
        }

        ComputePass::SharedPtr pPass = ComputePass::create(mpDevice, desc, defines, true);

        pPass["gScene"] = mpScene->getParameterBlock();
        pPass["gFalseColorBuffer"] = mpMeshletColorBuffer;

        // Bind mandatory input channels
        pPass["gInOutColor"] = renderData[kInputColor]->asTexture();
        pPass["gVBuffer"] = renderData[kInputVBuffer]->asTexture();

        // Bind extra input channels
        for (const auto& channel : kExtraInputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            pPass[channel.texname] = pTex;
        }

        // Bind extra input-output channels
        for (const auto& channel : kExtraInputOutputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            pPass[channel.texname] = pTex;
        }

        // Bind extra output channels as UAV buffers.
        for (const auto& channel : kExtraOutputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            pPass[channel.texname] = pTex;
        }

        auto pMeshletIDTexture = renderData[kInputOuputMeshlet]->asTexture();
        

        if (!mpFalseColorGenerator && 
            (   
                (renderData[kInputOuputMeshlet]->asTexture()   && renderData[kOutputMeshletColor]->asTexture()) || 
                (renderData[kInputOuputMicroPoly]->asTexture() && renderData[kOutputMicroPolyColor]->asTexture())
            )
        ) {
            static const uint32_t seed = 23456u;
            mpFalseColorGenerator = FalseColorGenerator::create(mpDevice, meshletColorCycleSize, &seed);
        }

        if (!mpHeatMapColorGenerator && (renderData[kOutputMeshletDrawColor]->asTexture() && renderData[kInputDrawCount]->asTexture())) mpHeatMapColorGenerator = HeatMapColorGenerator::create(mpDevice);
        if (mpVisibilitySamplesContainer) pPass[kVisibilityContainerParameterBlockName].setParameterBlock(mpVisibilitySamplesContainer->getParameterBlock());
    
        return pPass;
    };

    // Prepare program and vars. This may trigger shader compilation.
    // The program should have all necessary defines set at this point.
    if (!mpShadingPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("main");
        desc.addTypeConformances(mpScene->getTypeConformances());

        mpShadingPass = createShadingPass(desc);
        mpTransparentShadingPass = createShadingPass(desc, true);
    }

    if(mpFalseColorGenerator) mpFalseColorGenerator->setShaderData(mpShadingPass["gFalseColorGenerator"]);
    if(mpHeatMapColorGenerator) mpHeatMapColorGenerator->setShaderData(mpShadingPass["gHeatMapColorGenerator"]);

    auto cb_var = mpShadingPass["PerFrameCB"];
    cb_var["gFrameDim"] = mFrameDim;

    if(mpVisibilitySamplesContainer) mpVisibilitySamplesContainer->beginFrame();

    if(mpVisibilitySamplesContainer) {
        // Visibility container mode opaque samples shading
        
        //static const DispatchArguments baseIndirectArgs = { 3600, 1, 1 };
        //if(!mpOpaquePassIndirectionArgsBuffer) {
        //    mpOpaquePassIndirectionArgsBuffer = Buffer::create(mpDevice, sizeof(DispatchArguments), ResourceBindFlags::IndirectArg, Buffer::CpuAccess::None, &baseIndirectArgs);
        //}
        //mpShadingPass->executeIndirect(pContext, mpOpaquePassIndirectionArgsBuffer.get());

        mpShadingPass->executeIndirect(pContext, mpVisibilitySamplesContainer->getOpaquePassIndirectionArgsBuffer().get());
    } else {
        // Legacy (visibility buffer) mode shading
        mpShadingPass->execute(pContext, mFrameDim.x, mFrameDim.y);
    }

    if(mpTransparentShadingPass) {
        // Visibility container mode transparent samples shading
        auto cb_var = mpTransparentShadingPass["PerFrameCB"];
        cb_var["gFrameDim"] = mFrameDim;

        //mpTransparentShadingPass->execute(pContext, mFrameDim.x * mFrameDim.y, 1, 1);
    }

    if(mpVisibilitySamplesContainer) mpVisibilitySamplesContainer->endFrame();

    mDirty = false;
}

void DebugShadingPass::generateMeshletColorBuffer(const RenderData& renderData) {
    if(mpMeshletColorBuffer || !renderData[kInputOuputMeshlet]->asTexture()) return;

    static bool solidAplha = true;
    static const uint32_t seed = 12345u;
    mpMeshletColorBuffer = generateRandomColorsBuffer(mpDevice, meshletColorCycleSize, mFalseColorFormat, solidAplha, &seed);
}

DebugShadingPass& DebugShadingPass::setColorFormat(ResourceFormat format) {
    return *this;
}

void DebugShadingPass::setVisibilitySamplesContainer(VisibilitySamplesContainer::SharedConstPtr pVisibilitySamplesContainer) {
    if(mpVisibilitySamplesContainer == pVisibilitySamplesContainer) return;
    mpVisibilitySamplesContainer = pVisibilitySamplesContainer;
    mDirty = true;
}