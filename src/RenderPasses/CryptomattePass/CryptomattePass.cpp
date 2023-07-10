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

    const uint32_t gSeed = 0;

    const uint32_t kMaxDataLayersCount = 8;
    constexpr uint32_t kMaxRanks = kMaxDataLayersCount * 2;

    const std::string kInputVBuffer = "vbuffer";
    const std::string kPreviewColorOutput = "preview_color";
    
    const std::string kShaderModel = "6_5";

    const ChannelList kExtraOutputChannels = {
        { kPreviewColorOutput,   "gPreviewColor",          "Cryptomatte preview false color",         true /* optional */, ResourceFormat::RGBA32Float },
    };

    const std::string kOutputPreview = "outputPreview";
    const std::string kTypeName = "typeName";
    const std::string kMode = "outputMode";
    const std::string kRank = "rank";
    const std::string kManifestFilename = "manifestFilename";
    const std::string kSamplesPerFrame = "samplesPerFrame";

    const std::array<std::string, kMaxDataLayersCount> kDataOutputNames = 
        {"output00", "output01", "output02", "output03", "output04", "output05", "output06", "output07"};
}

CryptomattePass::SharedPtr CryptomattePass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    auto pThis = SharedPtr(new CryptomattePass(pRenderContext->device()));
        
    for (const auto& [key, value] : dict) {
        if (key == kMode) pThis->setMode(static_cast<CryptomatteMode>((uint32_t)value));
        else if (key == kRank) pThis->setRank(value);
        else if (key == kOutputPreview) pThis->setOutputPreviewColor(value);
        else if (key == kManifestFilename) pThis->setManifestFilename(value);
        else if (key == kSamplesPerFrame) pThis->setSamplesPerFrame(value);
        else if (key == kTypeName) pThis->setTypeName(value);
    }

    return pThis;
}

Dictionary CryptomattePass::getScriptingDictionary() {
    Dictionary d;
    return d;
}

CryptomattePass::CryptomattePass(Device::SharedPtr pDevice): RenderPass(pDevice, kInfo) {
    setTypeName();
}

RenderPassReflection CryptomattePass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;

    const auto& texDims = compileData.defaultTexDims;

    reflector.addInput(kInputVBuffer, "Visibility buffer in packed format").format(ResourceFormat::RGBA32Uint);
    addRenderPassOutputs(reflector, kExtraOutputChannels, Resource::BindFlags::UnorderedAccess);

    LLOG_DBG << "CryptomattePass " << to_string(mMode) << " data layers count " << dataLayersCount();
    for(uint32_t i = 0; i < dataLayersCount(); i++) {
        reflector.addOutput(kDataOutputNames[i], "Cryptomatte data layer").format(ResourceFormat::RGBA32Float).bindFlags(Resource::BindFlags::UnorderedAccess).flags(RenderPassReflection::Field::Flags::Optional);
    }

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
    // TODO: lazy data buffers write.
    if (!mpScene) return;
    
    calculateHashTables(renderData);
    createSortingBuffers();

    auto pOutputPreviewTex = renderData[kPreviewColorOutput]->asTexture();

    if(mSampleNumber == 0) {
        if(pOutputPreviewTex) pContext->clearUAV(pOutputPreviewTex->getUAV().get(), float4(0.f));
        for (size_t i = 0; i < kMaxDataLayersCount; i++) {
            auto pDataTex = renderData[kDataOutputNames[i]]->asTexture();
            if(pDataTex) pContext->clearUAV(pDataTex->getUAV().get(), float4(0.f));
        }
    }

    // Prepare program and vars. This may trigger shader compilation.
    // The program should have all necessary defines set at this point.
    if (!mpPass || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("main");
        desc.addTypeConformances(mpScene->getTypeConformances());

        auto defines = mpScene->getSceneDefines();
        defines.add(getValidResourceDefines(kExtraOutputChannels, renderData));

        // Cryptomatte layers
        defines.add("_OUTPUT_PREVIEW", mOutputPreview ? "1" : "0");
        defines.add("_MODE", std::to_string(static_cast<uint32_t>(mMode)));
        defines.add("_RANK", std::to_string(mRank));
        defines.add("_DATA_LAYERS_COUNT", std::to_string(dataLayersCount()));

        defines.add("is_valid_gFloatHashBuffer", mpFloatHashBuffer ? "1" : "0");
        defines.add("is_valid_gHashBuffer", mpHashBuffer ? "1" : "0");
        defines.add("is_valid_gPreviewHashColorBuffer", mpPreviewHashColorBuffer ? "1" : "0");
        defines.add("is_valid_gPreviewLayer", pOutputPreviewTex ? "1" : "0");

        std::array<Texture::SharedPtr, kMaxDataLayersCount> dataTextures;

        for (size_t i = 0; i < kMaxDataLayersCount; i++) {
            dataTextures[i] = renderData[kDataOutputNames[i]]->asTexture();
            const std::string layerName = "is_valid_gDataLayer" + (boost::format("%02d") % i).str();
            defines.add(layerName, dataTextures[i] ? "1" : "0");
        }

        mpPass = ComputePass::create(mpDevice, desc, defines, true);

        mpPass["gScene"] = mpScene->getParameterBlock();

        // Bind mandatory input channels
        mpPass["gVbuffer"] = renderData[kInputVBuffer]->asTexture();

        // Bind hash buffers
        mpPass["gFloatHashBuffer"] = mpFloatHashBuffer;
        mpPass["gHashBuffer"] = mpHashBuffer;
        mpPass["gPreviewHashColorBuffer"] = mpPreviewHashColorBuffer;

        // Bind crypto data output layers as UAV buffers.
        for (size_t i = 0; i < dataLayersCount(); i++) {
            mpPass["gDataLayers"][i] = dataTextures[i];
        }

        for (size_t i = 0; i < mRank; i++) {
            mpPass["gSortBuffers"][i] = mDataSortingBuffers[i];
        }

        // Bind preview output
        mpPass["gPreviewColor"] = pOutputPreviewTex;
    }

    mSampleNumber++;
    auto cb_var = mpPass["PerFrameCB"];
    cb_var["gFrameDim"] = mFrameDim;
    cb_var["gRanksCount"] = mRank;
    cb_var["gDataLayersCount"] = dataLayersCount();
    cb_var["gSampleNumber"] = mSampleNumber;
    cb_var["gSumWeight"] = float(mSampleNumber);
    cb_var["gSamplesPerFrame"] = mSamplesPerFrame;

    mpPass->execute(pContext, mFrameDim.x, mFrameDim.y);

    mDirty = false;
}

void CryptomattePass::reset() {
    mSampleNumber = 0;
    mDirty = true;
}

void CryptomattePass::calculateHashTables( const RenderData& renderData) {
    if(!mDirty) return;

    assert(mpScene);

    const bool outputPreview = renderData[kPreviewColorOutput]->asTexture() && mOutputPreview ? true : false;
    if (!outputPreview) mpPreviewHashColorBuffer = nullptr;

    std::string typeName = getTypeName();
    typeName.resize(7);
    const uint32_t typeNameHash = util_murmur_hash3(typeName.c_str(), typeName.size(), 0u);
    const std::string typeNameHashString = hash_float_to_hexidecimal(util_hash_to_float(typeNameHash));

    static const std::string hashTypeName = "MurmurHash3_32";
    static const std::string convTypeName = "uint32_to_float32";

    mMetaData[(boost::format("cryptomatte/%s/name") % typeNameHashString).str()] = getTypeName();
    mMetaData[(boost::format("cryptomatte/%s/hash") % typeNameHashString).str()] = hashTypeName;
    mMetaData[(boost::format("cryptomatte/%s/conversion") % typeNameHashString).str()] = convTypeName;

    // Calculate material name hashes
    if (mMode == CryptomatteMode::Material) {
        const auto& pMaterialSystem = mpScene->materialSystem();
        const uint32_t materialsCount = pMaterialSystem->getMaterialCount();
        const auto& materials = pMaterialSystem->getMaterials();

        if (materialsCount > 0) {
            Falcor::Dictionary manifest;
            std::vector<uint32_t> materialHashBuffer(materialsCount);
            std::vector<float> materialHashFloatBuffer(materialsCount);

            char clean_name_buffer[Cryptomatte::MAX_STRING_LENGTH];
            for(uint32_t materialID = 0; materialID < materials.size(); materialID++) {
                const auto& pMaterial = materials[materialID];

                memset(clean_name_buffer,0,Cryptomatte::MAX_STRING_LENGTH);
                const std::string& materialName = pMaterial->getName();
                Cryptomatte::getCleanMaterialName(materialName.c_str(), clean_name_buffer, mMaterialNameCleaningFlags);

                const uint32_t hash = util_murmur_hash3(clean_name_buffer, strlen(clean_name_buffer), gSeed);
                const float fhash = util_hash_to_float(hash);
                manifest[materialName] = hash_float_to_hexidecimal(fhash);
                materialHashBuffer[materialID] = hash;
                materialHashFloatBuffer[materialID] = fhash;

                LLOG_DBG << "Hash for material name " << pMaterial->getName() << " clean name " << std::string(clean_name_buffer) 
                    << " uint " << std::to_string(hash) << " hex " << hash_float_to_hexidecimal(fhash) 
                    << " float " << boost::lexical_cast<std::string>(fhash);
            }

            if (outputPreview) {
                std::vector<float3> materialPreviewColorBuffer(materialHashBuffer.size());
                for(size_t i = 0; i < materialHashBuffer.size(); i++) {
                    materialPreviewColorBuffer[i] = saturate(util_hash_to_rgb(materialHashBuffer[i]));
                }
                mpPreviewHashColorBuffer = Buffer::createTyped<float3>(mpDevice, materialPreviewColorBuffer.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, materialPreviewColorBuffer.data());
            }

            mpHashBuffer = Buffer::createTyped<uint32_t>(mpDevice, materialHashBuffer.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, materialHashBuffer.data());
            mpFloatHashBuffer = Buffer::createTyped<float>(mpDevice, materialHashFloatBuffer.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, materialHashFloatBuffer.data());
            mMetaData[(boost::format("cryptomatte/%s/manifest") % typeNameHashString).str()] = manifest.toJsonString();
        }
    } else if (mMode == CryptomatteMode::Instance) {
        // Calculate instance name hashes
        const uint32_t instancesCount = mpScene->getGeometryInstanceCount();

        if(instancesCount > 0) {
            Falcor::Dictionary manifest;
            std::vector<uint32_t> instanceHashBuffer(instancesCount);
            std::vector<float> instanceHashFloatBuffer(instancesCount);

            char clean_name_buffer[Cryptomatte::MAX_STRING_LENGTH];
            for(uint32_t i = 0; i < instancesCount; i++) {
                const auto& instance = mpScene->getGeometryInstance(i);
                
                memset(clean_name_buffer,0,Cryptomatte::MAX_STRING_LENGTH);
                const std::string& instanceName = mpScene->getInstanceName(instance.internalID);
                Cryptomatte::getCleanMaterialName(instanceName.c_str(), clean_name_buffer, mInstanceNameCleaningFlags);

                const uint32_t hash = util_murmur_hash3(clean_name_buffer, strlen(clean_name_buffer), gSeed);
                const float fhash = util_hash_to_float(hash);
                manifest[instanceName] = hash_float_to_hexidecimal(fhash);
                instanceHashBuffer[instance.internalID] = hash;
                instanceHashFloatBuffer[instance.internalID] = fhash;

                LLOG_DBG << "Hash for instance name " << mpScene->getInstanceName(i) << " clean name " << std::string(clean_name_buffer) 
                    << " uint " << std::to_string(hash) << " hex " << hash_float_to_hexidecimal(fhash)
                    << " float " << boost::lexical_cast<std::string>(fhash);
            }

            if (outputPreview) {
                std::vector<float3> instancePreviewColorBuffer(instanceHashBuffer.size());
                for(size_t i = 0; i < instanceHashBuffer.size(); i++) {
                    instancePreviewColorBuffer[i] = saturate(util_hash_to_rgb(instanceHashBuffer[i]));
                }
                mpPreviewHashColorBuffer = Buffer::createTyped<float3>(mpDevice, instancePreviewColorBuffer.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, instancePreviewColorBuffer.data());
            }

            mpHashBuffer = Buffer::createTyped<uint32_t>(mpDevice, instanceHashBuffer.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, instanceHashBuffer.data());
            mpFloatHashBuffer = Buffer::createTyped<float>(mpDevice, instanceHashFloatBuffer.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, instanceHashFloatBuffer.data());
            mMetaData[(boost::format("cryptomatte/%s/manifest") % typeNameHashString).str()] = manifest.toJsonString();
        }
    } else {
        mpHashBuffer = nullptr;
        mpFloatHashBuffer = nullptr;
        mpPreviewHashColorBuffer = nullptr;
    }
}

void CryptomattePass::createSortingBuffers() {
    if(!mDirty) return;
    
    mDataSortingBuffers.clear();
    mDataSortingBuffers.resize(mRank);

    //SortingPair clearValue;
    std::vector<SortingPair> clearBuffer(mFrameDim.x * mFrameDim.y);
    //std::fill (clearBuffer.begin(),clearBuffer.end(),clearValue);
    memset(clearBuffer.data(), 0, clearBuffer.size() * sizeof(SortingPair));

    for(size_t i = 0; i < mDataSortingBuffers.size(); ++i) {
        mDataSortingBuffers[i] = 
            Buffer::createStructured(mpDevice, sizeof(SortingPair), clearBuffer.size(), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, clearBuffer.data());
    }   
}

void CryptomattePass::setMode(CryptomatteMode mode) {
    if (mMode == mode) return;
    mMode = mode;
    mDirty = true;
}

void CryptomattePass::setRank(uint32_t rank) {
    if(mRank == rank) return;
    mRank = std::min(kMaxRanks, rank);
    mDirty = true;
}

void CryptomattePass::setOutputPreviewColor(bool value) {
    if(mOutputPreview == value) return;
    mOutputPreview = value;
    mDirty = true;
}

void CryptomattePass::setManifestFilename(const std::string& name) {
    if(mManifestFilename == name) return;
    mManifestFilename = name;
    mDirty = true;
}

void CryptomattePass::setTypeName(const std::string& type_name) {
    std::string _name = type_name;
    if(_name == "") {
        if(mMode == CryptomatteMode::Material) _name = "CryptoMaterial";
        else if(mMode == CryptomatteMode::Instance) _name = "CryptoObject";
        else _name = "CryptoAsset";
    }
    if(mTypeName == _name) return;
    mTypeName = _name;
    mDirty = true;
}

void CryptomattePass::setSamplesPerFrame(uint32_t value) {
    if(mSamplesPerFrame == value) return;
    mSamplesPerFrame = value;
    mDirty = true;
}