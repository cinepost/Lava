#include "NullShadingPass.h"

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/IndirectCommands.h"

#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/Utils/Scripting/ScriptBindings.h"

#include "Falcor/Utils/Timing/SimpleProfiler.h"


static const uint32_t meshletColorCycleSize = 1024;

static void regNullShadingPass(pybind11::module& m) {
    pybind11::class_<NullShadingPass, RenderPass> pass(m, "NullShadingPass");
    //pybind11::class_<NullShadingPass, RenderPass, NullShadingPass::SharedPtr> pass(m, "NullShadingPass");
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry) {
    //registry.registerClass<RenderPass, NullShadingPass>();
    ScriptBindings::registerBinding(regNullShadingPass);
}

namespace {

const char kShaderFile[] = "RenderPasses/NullShadingPass/NullShadingPass.cs.slang";

const std::string kVisibilityContainerParameterBlockName = "gVisibilityContainer";

const std::string kInputColor           = "color";
const std::string kInputVBuffer         = "vbuffer";
const std::string kInputDepth           = "depth";


const ChannelList kExtraInputChannels = {
    { kInputVBuffer,            "gVBuffer",             "Visibility buffer in packed format",       true /* optional */, ResourceFormat::RGBA32Uint     },
    { kInputDepth,              "gDepth",               "Depth buffer",                             true /* optional */, ResourceFormat::Unknown        },
};

} // namespace

NullShadingPass::SharedPtr NullShadingPass::create(RenderContext* pRenderContext, const Properties& props) {
    return SharedPtr(new NullShadingPass(pRenderContext->getDevice()));
}

NullShadingPass::NullShadingPass(Device::SharedPtr pDevice): RenderPass(pDevice) {
    if (!mpDevice->isShaderModelSupported(ShaderModel::SM6_5)) {
        FALCOR_THROW("NullShadingPass requires Shader Model 6.5 support.");
    }
}

RenderPassReflection NullShadingPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;

    reflector.addInputOutput(kInputColor, "Color buffer").format(ResourceFormat::Unknown);
    addRenderPassInputs(reflector, kExtraInputChannels);

    return reflector;
}

void NullShadingPass::compile(RenderContext* pRenderContext, const CompileData& compileData) {
    mDirty = true;
}

void NullShadingPass::execute(RenderContext* pContext, const RenderData& renderData) {
    LLOG_WRN << "NullShadingPass::execute";

    SimpleProfiler profile("NullShadingPass::execute");

    auto pColorInOutTex = renderData[kInputColor]->asTexture();
    if(!pColorInOutTex) {
        LLOG_ERR << "NullShadingPass error: no color input-output texture!";
        return;
    }

    if(!mpShadingPass || mDirty) {
        auto defines = DefineList();
        defines.add(getValidResourceDefines(kExtraInputChannels, renderData));
        
        Program::Desc desc;
        desc.addShaderLibrary(kShaderFile).csEntry("main");

        mpShadingPass = ComputePass::create(mpDevice, desc, defines, true);

        auto var = mpShadingPass->getRootVar();

        // Bind mandatory input channels
        var["gInOutColor"] = pColorInOutTex;

        // Bind extra input channels
        for (const auto& channel : kExtraInputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            var[channel.texname] = pTex;
        }
    };

    uint2 frameDim = {pColorInOutTex->getWidth(), pColorInOutTex->getHeight()};

    auto cb_var = mpShadingPass->getRootVar()["PerFrameCB"];
    cb_var["gFrameDim"] = frameDim;

    mpShadingPass->execute(pContext, frameDim.x, frameDim.y);

    mDirty = false;
}