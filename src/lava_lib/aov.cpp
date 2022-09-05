#include <chrono>

#include "aov.h"
#include "renderer.h"

#include "lava_utils_lib/logging.h"


namespace lava {

AOVPlane::SharedPtr AOVPlane::create(const AOVPlaneInfo& info) {
    return SharedPtr(new AOVPlane(info));
}


AOVPlane::AOVPlane(const AOVPlaneInfo& info): mInfo(info) { }

bool AOVPlane::bindToTexture(Falcor::Texture::SharedPtr pTexture) {
    assert(pTexture);

    if(pTexture->getFormat() != mInfo.format) {
        LLOG_ERR << "Falcor resource format (" << to_string(pTexture->getFormat()) << ") and AOV plane format (" << to_string(mInfo.format) << ") mismatch !!!";
        return false;
    }

    mpTexture = pTexture;
    return true;
}

bool AOVPlane::getImageData(uint8_t* pData) const {
    assert(pData);

    auto start = std::chrono::high_resolution_clock::now();

    if (!mpTexture) {
        LLOG_ERR << "No texture associated to AOV plane " << mInfo.name << " !!!";
        return false;
    }

    mpTexture->readTextureData(0, 0, pData);

    auto stop = std::chrono::high_resolution_clock::now();
    std::cout << "AOV plane " << name() << " data read time: " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " ms." << std::endl;

    return true;
}

bool AOVPlane::getAOVPlaneGeometry(AOVPlaneGeometry& aov_plane_geometry) const {
    if (!mpTexture) {
        LLOG_ERR << "No texture associated to AOV plane " << mInfo.name << " !!!";
        return false;
    }

    auto resourceFormat = mpTexture->getFormat();

    aov_plane_geometry.width = mpTexture->getWidth(0);
    aov_plane_geometry.height = mpTexture->getHeight(0);
    aov_plane_geometry.resourceFormat = resourceFormat;
    aov_plane_geometry.bytesPerPixel = Falcor::getFormatBytesPerBlock(resourceFormat);
    aov_plane_geometry.channelsCount = Falcor::getFormatChannelCount(resourceFormat);
    aov_plane_geometry.bitsPerComponent[0] = Falcor::getNumChannelBits(resourceFormat, 0);
    aov_plane_geometry.bitsPerComponent[1] = Falcor::getNumChannelBits(resourceFormat, 1);
    aov_plane_geometry.bitsPerComponent[2] = Falcor::getNumChannelBits(resourceFormat, 2);
    aov_plane_geometry.bitsPerComponent[3] = Falcor::getNumChannelBits(resourceFormat, 3);

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
        LLOG_ERR << "Acccumulation pass for AOV plane " << mInfo.name << " already created !!!";
        return mpAccumulatePass;
    }

    mpRenderGraph = pGraph;
    mpAccumulatePass = AccumulatePass::create(pContext);
    if (!mpAccumulatePass) {
        LLOG_ERR << "Error creating accumulation pass for AOV plane " << mInfo.name << " !!!";
        return nullptr;
    }

    mpAccumulatePass->enableAccumulation(true);
    mpAccumulatePass->setOutputFormat(format());

    mAccumulatePassName = "AccumulatePass_" + mInfo.name;
    mAccumulatePassInputName  = mAccumulatePassName + ".input";
    mAccumulatePassOutputName = mAccumulatePassName + ".output";

    mpRenderGraph->addPass(mpAccumulatePass, mAccumulatePassName);
    mpRenderGraph->markOutput(mAccumulatePassOutputName);
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

// HYDRA section end

}  // namespace lava