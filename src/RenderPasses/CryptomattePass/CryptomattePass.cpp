#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"
#include "Falcor/Utils/Cryptomatte/MurmurHash.h"

#include "glm/gtc/random.hpp"
#include "glm/gtx/string_cast.hpp"

#include "lava_utils_lib/ut_string.h"

#include "CryptomattePass.h"


const RenderPass::Info CryptomattePass::kInfo
{
    "CryptomattePass",

    "Computes direct and indirect illumination and applies shadows for the current scene (if visibility map is provided).\n"
    "The pass can output the world-space normals and screen-space motion vectors, both are optional."
};

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(CryptomattePass::kInfo, CryptomattePass::create);
}

namespace {
    const char kShaderFile[] = "RenderPasses/CryptomattePass/CryptomattePass.cs.slang";

    const uint32_t gSeed = 123456;

    const std::string kInputVBuffer = "vbuffer";

    const std::string kMaterialFalseColorOutput = "material_color";
    const std::string kInstanceFalseColorOutput = "instance_color";
    const std::string kCustattrFalseColorOutput = "custattr_color";
    
    const std::string kShaderModel = "6_5";

    const ChannelList kExtraOutputChannels = {
        { kMaterialFalseColorOutput,   "gMatColor",          "Cryptomatte material false color",         true /* optional */, ResourceFormat::RGBA16Float },
        { kInstanceFalseColorOutput,   "gObjColor",          "Cryptomatte object false color",           true /* optional */, ResourceFormat::RGBA16Float },
        { kCustattrFalseColorOutput,   "gAttribColor",       "Cryptomatte custom attribute false color", true /* optional */, ResourceFormat::RGBA16Float },
    };
}

CryptomattePass::SharedPtr CryptomattePass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    auto pThis = SharedPtr(new CryptomattePass(pRenderContext->device()));
        
    for (const auto& [key, value] : dict) {
        
    }

    return pThis;
}

Dictionary CryptomattePass::getScriptingDictionary() {
    Dictionary d;
    return d;
}

CryptomattePass::CryptomattePass(Device::SharedPtr pDevice): RenderPass(pDevice, kInfo) {

}

RenderPassReflection CryptomattePass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;

    const auto& texDims = compileData.defaultTexDims;

    reflector.addInput(kInputVBuffer, "Visibility buffer in packed format").format(ResourceFormat::RGBA32Uint);
    addRenderPassOutputs(reflector, kExtraOutputChannels, Resource::BindFlags::UnorderedAccess);

    return reflector;
}

void CryptomattePass::compile(RenderContext* pRenderContext, const CompileData& compileData) {
    mFrameDim = compileData.defaultTexDims;   
    mDirty = true;
}

void CryptomattePass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    if(mpScene == pScene) return;
    mpScene = pScene;
    mDirty = true;
}

void CryptomattePass::execute(RenderContext* pContext, const RenderData& renderData) {
    if (!mpScene) return;
    
    // Prepare program and vars. This may trigger shader compilation.
    // The program should have all necessary defines set at this point.
    if (mDirty) {
        calculateHashTables(renderData);

        Program::Desc desc;
        desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("main");
        desc.addTypeConformances(mpScene->getTypeConformances());

        auto defines = mpScene->getSceneDefines();
        defines.add(getValidResourceDefines(kExtraOutputChannels, renderData));
        
        // Cryptomatte layers
        defines.add("_OUTPUT_MATERIALS", mpMaterialHashBuffer ? "1" : "0");
        defines.add("_OUTPUT_INSTANCES", mpInstanceHashBuffer ? "1" : "0");
        defines.add("_OUTPUT_CUSTATTRS", mpCustattrHashBuffer ? "1" : "0");

        mpPass = ComputePass::create(mpDevice, desc, defines, true);

        mpPass["gScene"] = mpScene->getParameterBlock();

        // Bind mandatory input channels
        mpPass["gVbuffer"] = renderData[kInputVBuffer]->asTexture();

        // Bind buffers
        mpPass["gMaterialHashBuffer"] = mpMaterialHashBuffer;
        mpPass["gMaterialPreviewColorBuffer"] = mpMaterialPreviewColorBuffer;

        // Bind extra output channels as UAV buffers.
        for (const auto& channel : kExtraOutputChannels) {
            Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
            mpPass[channel.texname] = pTex;
        }
        mDirty = false;
    }

    auto cb_var = mpPass["PerFrameCB"];

    cb_var["gFrameDim"] = mFrameDim;
    mpPass->execute(pContext, mFrameDim.x, mFrameDim.y);
}

void CryptomattePass::calculateHashTables( const RenderData& renderData) {
    assert(mpScene);

    const auto& pMaterialSystem = mpScene->materialSystem();
    uint32_t materialsCount = pMaterialSystem->getMaterialCount();
    const auto& materials = pMaterialSystem->getMaterials();

    // Calculate material name hashes
    if (materialsCount > 0) {
        bool outputMaterialPreview = renderData[kMaterialFalseColorOutput]->asTexture() ? true : false;
        std::vector<float> materialHashBuffer(materialsCount);
        std::vector<float3> materialPreviewColorBuffer(outputMaterialPreview ? materialsCount : 0);

        for(uint32_t materialID = 0; materialID < materials.size(); materialID++) {
            const auto& pMaterial = materials[materialID];
            const auto& name = pMaterial->getName();

            unsigned char trap[256];
            std::copy( name.begin(), name.end(), trap );
            trap[name.length()] = 0;

            uint32_t hash = util_murmur_hash3(static_cast<const void *>(trap), name.size(), gSeed);
           // uint32_t hash = util_murmur_hash3(static_cast<const void *>(name.c_str()), name.size(), gSeed);
            float fhash = util_hash_to_float(hash);
            materialHashBuffer[materialID] = fhash;

            LLOG_WRN << "Hash for material name " << pMaterial->getName() << " uint " << std::to_string(hash) << " float " << boost::lexical_cast<std::string>(fhash);

            if (outputMaterialPreview) materialPreviewColorBuffer[materialID] = util_hash_to_rgb(hash);
        }

        mpMaterialHashBuffer = Buffer::createTyped<float>(mpDevice, materialHashBuffer.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, materialHashBuffer.data());
        if (outputMaterialPreview) mpMaterialPreviewColorBuffer = 
            Buffer::createTyped<float3>(mpDevice, materialPreviewColorBuffer.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, materialPreviewColorBuffer.data());
    }

    // Calculate instance name hashes
    {
        
    }
}

/*

float util_hash_to_float(uint32_t hash);
float3 util_hash_name_to_rgb(const unsigned char *name);
float3 util_hash_to_rgb(uint32_t hash);

*/