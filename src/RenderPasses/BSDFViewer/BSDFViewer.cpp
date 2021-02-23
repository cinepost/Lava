/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "BSDFViewer.h"
#include "Experimental/Scene/Material/BxDFConfig.slangh"

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("BSDFViewer", BSDFViewer::sDesc, BSDFViewer::create);
}

namespace
{
    const char kFileViewerPass[] = "RenderPasses/BSDFViewer/BSDFViewer.cs.slang";
    const char kOutput[] = "output";
}

const char* BSDFViewer::sDesc = "BSDF Viewer";

BSDFViewer::SharedPtr BSDFViewer::create(RenderContext* pRenderContext, const Dictionary& dict)
{ 
    return SharedPtr(new BSDFViewer(pRenderContext->device(), dict));
}

BSDFViewer::BSDFViewer(Device::SharedPtr pDevice, const Dictionary& dict): RenderPass(pDevice)
{
    // Create a high-quality pseudorandom number generator.
    mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_UNIFORM);

    // Defines to disable discard and gradient operations in Falcor's material system.
    Program::DefineList defines =
    {
        {"_MS_DISABLE_ALPHA_TEST", ""},
        {"_DEFAULT_ALPHA_TEST", ""},
        {"MATERIAL_COUNT", "1"},
    };

    defines.add(mpSampleGenerator->getDefines());

    // Create programs.
    mpViewerPass = ComputePass::create(pDevice, kFileViewerPass, "main", defines);

    // Create readback buffer.
    mPixelDataBuffer = Buffer::createStructured(pDevice, mpViewerPass->getProgram().get(), "gPixelData", 1u, ResourceBindFlags::UnorderedAccess);

    mpPixelDebug = PixelDebug::create(pDevice);
}

Dictionary BSDFViewer::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection BSDFViewer::reflect(const CompileData& compileData)
{
    RenderPassReflection r;
    r.addOutput(kOutput, "Output buffer").format(ResourceFormat::RGBA32Float).bindFlags(ResourceBindFlags::UnorderedAccess);
    return r;
}

void BSDFViewer::compile(RenderContext* pContext, const CompileData& compileData)
{
    mParams.frameDim = compileData.defaultTexDims;

    // Place a square viewport centered in the frame.
    uint32_t extent = std::min(mParams.frameDim.x, mParams.frameDim.y);
    uint32_t xOffset = (mParams.frameDim.x - extent) / 2;
    uint32_t yOffset = (mParams.frameDim.y - extent) / 2;

    mParams.viewportOffset = float2(xOffset, yOffset);
    mParams.viewportScale = float2(1.f / extent);
}

void BSDFViewer::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;
    mpEnvMap = nullptr;
    mMaterialList.clear();
    mParams.materialID = 0;

    if (pScene == nullptr)
    {
        mParams.useSceneMaterial = false;
        mParams.useEnvMap = false;
    }
    else
    {
        mParams.useSceneMaterial = true;

        // Bind the scene to our program.
        mpViewerPass->getProgram()->addDefines(mpScene->getSceneDefines());
        mpViewerPass->setVars(nullptr); // Trigger vars creation
        mpViewerPass["gScene"] = mpScene->getParameterBlock();

        // Load and bind environment map.
        if (const auto &pEnvMap = mpScene->getEnvMap()) loadEnvMap(pEnvMap->getFilename());
        mParams.useEnvMap = mpEnvMap != nullptr;

        // Prepare UI list of materials.
        mMaterialList.reserve(mpScene->getMaterialCount());
        for (uint32_t i = 0; i < mpScene->getMaterialCount(); i++)
        {
            auto mtl = mpScene->getMaterial(i);
            std::string name = std::to_string(i) + ": " + mtl->getName();
            mMaterialList.push_back({ i, name });
        }
        assert(mMaterialList.size() > 0);
    }
}

void BSDFViewer::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // Update refresh flag if options that affect the output have changed.
    if (mOptionsChanged)
    {
        auto& dict = renderData.getDictionary();
        auto flags = dict.getValue(kRenderPassRefreshFlags, RenderPassRefreshFlags::None);
        dict[Falcor::kRenderPassRefreshFlags] = flags | Falcor::RenderPassRefreshFlags::RenderOptionsChanged;
        mOptionsChanged = false;
    }

    // Set compile-time constants.
    mpViewerPass->addDefine("_USE_LEGACY_SHADING_CODE", mParams.useLegacyBSDF ? "1" : "0");

    if (mParams.useDisneyDiffuse) mpViewerPass->addDefine("DiffuseBrdf", "DiffuseBrdfDisney");
    else mpViewerPass->removeDefine("DiffuseBrdf");
    if (mParams.useSeparableMaskingShadowing) mpViewerPass->addDefine("SpecularMaskingFunction", "SpecularMaskingFunctionSmithGGXSeparable");
    else mpViewerPass->removeDefine("SpecularMaskingFunction");

    // Setup constants.
    mParams.cameraViewportScale = std::tan(glm::radians(mParams.cameraFovY / 2.f)) * mParams.cameraDistance;

    // Set resources.
    if (!mpSampleGenerator->setShaderData(mpViewerPass->getVars()->getRootVar())) throw std::runtime_error("Failed to bind sample generator");
    mpViewerPass["gOutput"] = renderData[kOutput]->asTexture();
    mpViewerPass["gPixelData"] = mPixelDataBuffer;
    mpViewerPass["PerFrameCB"]["gParams"].setBlob(mParams);

    mpPixelDebug->beginFrame(pRenderContext, renderData.getDefaultTextureDims());
    mpPixelDebug->prepareProgram(mpViewerPass->getProgram(), mpViewerPass->getRootVar());

    // Execute pass.
    mpViewerPass->execute(pRenderContext, uint3(mParams.frameDim, 1));

    mpPixelDebug->endFrame(pRenderContext);

    mPixelDataValid = false;
    if (mParams.readback)
    {
        const PixelData* pData = static_cast<const PixelData*>(mPixelDataBuffer->map(Buffer::MapType::Read));
        mPixelData = *pData;
        mPixelDataBuffer->unmap();
        mPixelDataValid = true;

        // Copy values from selected pixel.
        mParams.texCoords = mPixelData.texC;
    }

    mParams.frameCount++;
}

bool BSDFViewer::loadEnvMap(const std::string& filename)
{
    mpEnvMap = EnvMap::create(mpDevice, filename);
    if (!mpEnvMap)
    {
        logWarning("Failed to load environment map from " + filename);
        return false;
    }

    auto pVars = mpViewerPass->getVars();
    mpEnvMap->setShaderData(pVars["PerFrameCB"]["gEnvMap"]);

    return true;
}
