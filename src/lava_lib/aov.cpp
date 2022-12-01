#include <chrono>

#include "aov.h"
#include "renderer.h"

#include "RenderPasses/ToneMapperPass/ToneMapperPass.h"

#include "lava_utils_lib/logging.h"


namespace lava {


static Falcor::ResourceFormat getClosestAvailableFormat(Falcor::ResourceFormat in_format) {
    // TODO: make proper checking against available GPU formats

    // TODO: allow lowering bit depths using the same number of channels. e.g. R11G11B10Float

    auto out_format = in_format;
    switch(in_format) {
        case Falcor::ResourceFormat::RGB32Float:
            out_format = Falcor::ResourceFormat::RGBA32Float;
            break;
        case Falcor::ResourceFormat::RGB16Float:
            out_format = Falcor::ResourceFormat::RGBA16Float;
            break;
        case Falcor::ResourceFormat::RGB8Unorm:
            out_format = Falcor::ResourceFormat::RGBA8Unorm;
            break;
        case Falcor::ResourceFormat::RGB8Uint:
            out_format = Falcor::ResourceFormat::RGBA8Uint;
            break;
        case Falcor::ResourceFormat::RGB16Int:
            out_format = Falcor::ResourceFormat::RGBA16Int;
            break;
        case Falcor::ResourceFormat::RGB16Uint:
            out_format = Falcor::ResourceFormat::RGBA16Uint;
            break;
        case Falcor::ResourceFormat::RGB16Unorm:
            out_format = Falcor::ResourceFormat::RGBA16Unorm;
            break;
        case Falcor::ResourceFormat::RGB16Snorm:
            out_format = Falcor::ResourceFormat::RGBA16Snorm;
            break;
        default:
            break;
    }
    if (in_format != out_format) LLOG_WRN << "Resource format " << to_string(in_format) << " is unavailable for rendering. Changing to " << to_string(out_format) << " !!!";
    return out_format;
}


AOVPlane::SharedPtr AOVPlane::create(const AOVPlaneInfo& info) {
    return SharedPtr(new AOVPlane(info));
}


AOVPlane::AOVPlane(const AOVPlaneInfo& info): mInfo(info) { }

bool AOVPlane::bindToTexture(Falcor::Texture::SharedPtr pTexture) {
    assert(pTexture);

    if(pTexture->getFormat() != mInfo.format) {
        LLOG_WRN << "Performance warning! Render pass resource format (" << to_string(pTexture->getFormat()) << ") and AOV plane " << name() << " format (" 
                 << to_string(mInfo.format) << ") mismatch !";
    }

    mpTexture = pTexture;
    return true;
}

bool AOVPlane::getTextureData(Texture* pTexture, uint8_t* pData) const {
    assert(pData);
    assert(pTexture);
    assert(mFormat != Falcor::ResourceFormat::Unknown);
    assert(mInfo.format != Falcor::ResourceFormat::Unknown);

    if (mInfo.format == pTexture->getFormat()) {
        // Requested and available resource formats are the same
        pTexture->readTextureData(0, 0, pData);
    } else {
        LLOG_WRN << "Do blit !";
        // Requested and available resource formats are different. Do conversion/blit here
        pTexture->readConvertedTextureData(0, 0, pData, mInfo.format);
    }

    return true;
}

bool AOVPlane::getProcessedImageData(uint8_t* pData) const {
    auto start = std::chrono::high_resolution_clock::now();

    if (!mpInternalRenderGraph || mProcessedPassOutputName.empty() || !mpInternalRenderGraph->isGraphOutput(mProcessedPassOutputName)) {
        LLOG_WRN << "No AOV plane " << mInfo.name << " post effects exist. Reading raw image data.";
        return getImageData(pData);
    }

    mpInternalRenderGraph->execute();
    auto pResource = mpInternalRenderGraph->getOutput(mProcessedPassOutputName);
    auto pEffectsGraphTexture = pResource ? pResource->asTexture() : nullptr;
    if (!pEffectsGraphTexture) {
        LLOG_WRN << "No effects chain output texture associated with AOV plane " << mInfo.name << "!!! Unable to read data !!!";
        return false;
    }

    bool result = getTextureData(pEffectsGraphTexture.get(), pData);

    auto stop = std::chrono::high_resolution_clock::now();
    LLOG_DBG << "AOV plane " << name() << " processed image data read from " << mProcessedPassOutputName << " time: " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " ms.";

    return result;
}

bool AOVPlane::getImageData(uint8_t* pData) const {
    auto start = std::chrono::high_resolution_clock::now();

    if (!mpTexture) {
        LLOG_WRN << "No output texture associated with AOV plane " << mInfo.name << "!!! Unable to read data !!!";
        return false;
    }
    bool result = getTextureData(mpTexture.get(), pData);

    auto stop = std::chrono::high_resolution_clock::now();
    LLOG_DBG << "AOV plane " << name() << " image data read time: " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " ms.";

    return result;
}

bool AOVPlane::getAOVPlaneGeometry(AOVPlaneGeometry& aov_plane_geometry) const {
    if (!mpTexture) {
        LLOG_ERR << "No texture associated to AOV plane " << mInfo.name << " !!!";
        return false;
    }

    //auto resourceFormat = mpTexture->getFormat();

    auto requestedResourceFormat = format();

    aov_plane_geometry.width = mpTexture->getWidth(0);
    aov_plane_geometry.height = mpTexture->getHeight(0);
    aov_plane_geometry.resourceFormat = requestedResourceFormat;
    aov_plane_geometry.bytesPerPixel = Falcor::getFormatBytesPerBlock(requestedResourceFormat);
    aov_plane_geometry.channelsCount = Falcor::getFormatChannelCount(requestedResourceFormat);
    aov_plane_geometry.bitsPerComponent[0] = Falcor::getNumChannelBits(requestedResourceFormat, 0);
    aov_plane_geometry.bitsPerComponent[1] = Falcor::getNumChannelBits(requestedResourceFormat, 1);
    aov_plane_geometry.bitsPerComponent[2] = Falcor::getNumChannelBits(requestedResourceFormat, 2);
    aov_plane_geometry.bitsPerComponent[3] = Falcor::getNumChannelBits(requestedResourceFormat, 3);

    return true;
}

void AOVPlane::setFormat(Falcor::ResourceFormat format) {
    auto pResource = mpRenderGraph->getOutput(mAccumulatePassOutputName);
    if(!pResource) {
        LLOG_ERR << "AOV plane \"" << mInfo.name << "\" is not bound to resource !!!";
        return;
    }

    auto pTexture = pResource->asTexture();
    if (!pTexture) {
        LLOG_ERR << "Unable to set format on non-texture resource!";
        return;
    }
    if (format == pTexture->getFormat()) return;
}

AccumulatePass::SharedPtr AOVPlane::createAccumulationPass( Falcor::RenderContext* pContext, Falcor::RenderGraph::SharedPtr pGraph, const Falcor::Dictionary& dict) {
    assert(pGraph);

    if (mpAccumulatePass) {
        LLOG_WRN << "Accumulation pass for AOV plane " << mInfo.name << " already created !!!";
        return mpAccumulatePass;
    }

    mpRenderGraph = pGraph;
    
    mpAccumulatePass = AccumulatePass::create(pContext, dict);
    if (!mpAccumulatePass) {
        LLOG_ERR << "Error creating accumulation pass for AOV plane " << mInfo.name << " !!!";
        return nullptr;
    }

    mpAccumulatePass->enableAccumulation(true);

    mFormat = getClosestAvailableFormat(format());

    LLOG_DBG << "createAccumulationPass for " << mInfo.name << " requested format " << to_string(format()) << " closest available " << to_string(mFormat);

    mpAccumulatePass->setOutputFormat(mFormat);

    mAccumulatePassName = "AccumulatePass_" + mInfo.name;
    mAccumulatePassInputName  = mAccumulatePassName + ".input";
    mAccumulatePassOutputName = mAccumulatePassName + ".output";

    mpRenderGraph->addPass(mpAccumulatePass, mAccumulatePassName);
    mpRenderGraph->markOutput(mAccumulatePassOutputName);

    return mpAccumulatePass;
}

ToneMapperPass::SharedPtr AOVPlane::createTonemappingPass(Falcor::RenderContext* pContext, const Falcor::Dictionary& dict) {
    if (mpToneMapperPass) {
        LLOG_WRN << "Accumulation pass for AOV plane " << mInfo.name << " already created !!!";
        return mpToneMapperPass;
    }

    if(!mpInternalRenderGraph) {
        createInternalRenderGraph(pContext);
        if(!mpInternalRenderGraph) return nullptr;
    }

    LLOG_DBG << "Creating ToneMapperPass";

    mpToneMapperPass = ToneMapperPass::create(pContext, dict);
    if (!mpToneMapperPass) {
        LLOG_ERR << "Error creating tonemapper pass for AOV plane " << mInfo.name << " !!!";
        return nullptr;
    }

    std::string tonemapperPassName = "ToneMapperPass_" + mInfo.name;

    LLOG_DBG << tonemapperPassName << " created";
    mpToneMapperPass->setOutputFormat(mFormat);

    // Unmark previously marked output
    if(!mProcessedPassOutputName.empty() && mpInternalRenderGraph->isGraphOutput(mProcessedPassOutputName)) {
        mpInternalRenderGraph->unmarkOutput(mProcessedPassOutputName);
        LLOG_DBG << "Unmarked output " << mProcessedPassOutputName;
    }

    mpInternalRenderGraph->addPass(mpToneMapperPass, "ToneMapperPass_" + mInfo.name);
    mpInternalRenderGraph->addEdge(mProcessedPassOutputName, tonemapperPassName + ".input");

    mProcessedPassOutputName = tonemapperPassName + ".output";
    mpInternalRenderGraph->markOutput(mProcessedPassOutputName);

    LLOG_DBG << "Marked output " << mProcessedPassOutputName;

    if(!compileInternalRenderGraph(pContext)) {
        mpToneMapperPass = nullptr;
    }

    return mpToneMapperPass;
}

OpenDenoisePass::SharedPtr AOVPlane::createOpenDenoisePass( Falcor::RenderContext* pContext, const Falcor::Dictionary& dict) {
    if (mpDenoiserPass) {
        LLOG_WRN << "Denoiser pass for AOV plane " << mInfo.name << " already created !!!";
        return mpDenoiserPass;
    }

    return mpDenoiserPass;
}

void AOVPlane::createInternalRenderGraph(Falcor::RenderContext* pContext, bool force) {
    if (mpInternalRenderGraph && !force) return;

    std::string internalGraphName = name() + " internal graph";

    mpInternalRenderGraph = RenderGraph::create(pContext->device(), mpRenderGraph->dims(), internalGraphName);
    if (! mpInternalRenderGraph) {
        LLOG_ERR << "Error creating internal render graph " << internalGraphName;
    }

    if(!mpImageLoaderPass) {
        //Falcor::Dictionary dict;
        //dict["filename"] = fs::path("/home/max/Desktop/Screenshot from 2022-10-31 16-27-52.png");

        mpImageLoaderPass = ImageLoaderPass::create(pContext);
        if(!mAccumulatePassOutputName.empty() && mpRenderGraph->isGraphOutput(mAccumulatePassOutputName)) {
            auto pResource = mpRenderGraph->getOutput(mAccumulatePassOutputName);
            auto pTex = pResource ? pResource->asTexture() : nullptr;
            if(pTex) {
                mpImageLoaderPass->setSourceTexture(pTex);
            } else {
                LLOG_WRN << "No accumulation pass texture exist for processing !";
            }
        }
    }

    std::string imageLoaderPassName = "ImageLoaderPass_" + mInfo.name;

    mProcessedPassOutputName = imageLoaderPassName + ".output";
    mpInternalRenderGraph->addPass(mpImageLoaderPass, "ImageLoaderPass_" + mInfo.name);
    mpInternalRenderGraph->markOutput(mProcessedPassOutputName);
}

bool AOVPlane::compileInternalRenderGraph(Falcor::RenderContext* pContext) {
    if(!mpInternalRenderGraph) {
        LLOG_WRN << "No internal render graph exist for plane " << name() << "! Nothing to compile.";
        return false;
    }

    // Compile internal rendering graph
    std::string log;
    bool result = mpInternalRenderGraph->compile(pContext, log);
    if(!result) {
        LLOG_ERR << "Error compiling internal render graph for plane " << name() << " !\n" << log;
        mpInternalRenderGraph = nullptr;
        return false;
    }

    LLOG_DBG << name() << " internal render graph done";
    return true;
}

void AOVPlane::setOutputFormat(Falcor::ResourceFormat format) {
    auto _format = getClosestAvailableFormat(format);

    if (mpAccumulatePass && (mpAccumulatePass->format() != _format)) {
        mpAccumulatePass->setOutputFormat(_format);
        mFormat = _format;
    }
}


bool AOVPlane::isBound() const {
    if (!mpTexture) return false;
    return true;
}

std::string aov_name_visitor::operator()(AOVBuiltinName name) const {
    return to_string(name);
}
    
const std::string& aov_name_visitor::operator()(const std::string& str) const {
    return str;
}

AOVBuiltinName aov_builtin_name_visitor::operator()(AOVBuiltinName name) const {
    return name;
}

AOVBuiltinName aov_builtin_name_visitor::operator()(const std::string& str) const {
    if(str == to_string(AOVBuiltinName::MAIN)) return AOVBuiltinName::MAIN;
    if(str == to_string(AOVBuiltinName::NORMAL)) return AOVBuiltinName::NORMAL;
    if(str == to_string(AOVBuiltinName::POSITION)) return AOVBuiltinName::POSITION;
    if(str == to_string(AOVBuiltinName::DEPTH)) return AOVBuiltinName::DEPTH;
    if(str == to_string(AOVBuiltinName::ALBEDO)) return AOVBuiltinName::ALBEDO;
    if(str == to_string(AOVBuiltinName::SHADOW)) return AOVBuiltinName::SHADOW;
    if(str == to_string(AOVBuiltinName::OCCLUSION)) return AOVBuiltinName::OCCLUSION;
    if(str == to_string(AOVBuiltinName::OBJECT_ID)) return AOVBuiltinName::OBJECT_ID;
    if(str == to_string(AOVBuiltinName::MATERIAL_ID)) return AOVBuiltinName::MATERIAL_ID;
    if(str == to_string(AOVBuiltinName::INSTANCE_ID)) return AOVBuiltinName::INSTANCE_ID;
    if(str == to_string(AOVBuiltinName::Prim_Id)) return AOVBuiltinName::Prim_Id;
    if(str == to_string(AOVBuiltinName::Op_Id)) return AOVBuiltinName::Op_Id;
    if(str == to_string(AOVBuiltinName::CRYPTOMATTE_MAT)) return AOVBuiltinName::CRYPTOMATTE_MAT;
    if(str == to_string(AOVBuiltinName::CRYPTOMATTE_OBJ)) return AOVBuiltinName::CRYPTOMATTE_OBJ;

    return AOVBuiltinName::UNKNOWN;
}

// HYDRA section end

}  // namespace lava