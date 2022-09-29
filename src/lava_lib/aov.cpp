#include <chrono>

#include "aov.h"
#include "renderer.h"

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

bool AOVPlane::getImageData(uint8_t* pData) const {
    assert(pData);
    assert(mFormat != Falcor::ResourceFormat::Unknown);
    assert(mInfo.format != Falcor::ResourceFormat::Unknown);

    auto start = std::chrono::high_resolution_clock::now();

    if (!mpTexture) {
        LLOG_ERR << "No texture associated to AOV plane " << mInfo.name << " !!!";
        return false;
    }

    if (mInfo.format == mpTexture->getFormat()) {
        // Requested and available resource formats are the same
        mpTexture->readTextureData(0, 0, pData);
    } else {
        LLOG_WRN << "Do blit !";
        // Requested and available resource formats are different. Do conversion/blit here
        mpTexture->readConvertedTextureData(0, 0, pData, mInfo.format);
    }

    auto stop = std::chrono::high_resolution_clock::now();
    LLOG_DBG << "AOV plane " << name() << " data read time: " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " ms.";

    return true;
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

AccumulatePass::SharedPtr AOVPlane::createAccumulationPass( Falcor::RenderContext* pContext, Falcor::RenderGraph::SharedPtr pGraph) {
    assert(pGraph);

    if (mpAccumulatePass) {
        LLOG_WRN << "Accumulation pass for AOV plane " << mInfo.name << " already created !!!";
        return mpAccumulatePass;
    }

    mpRenderGraph = pGraph;
    
    mpAccumulatePass = AccumulatePass::create(pContext);
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
    if(str == to_string(AOVBuiltinName::OBJECT_ID)) return AOVBuiltinName::OBJECT_ID;
    if(str == to_string(AOVBuiltinName::MATERIAL_ID)) return AOVBuiltinName::MATERIAL_ID;
    if(str == to_string(AOVBuiltinName::INSTANCE_ID)) return AOVBuiltinName::INSTANCE_ID;

    return AOVBuiltinName::UNKNOWN;
}

// HYDRA section end

}  // namespace lava