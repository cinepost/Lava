#include "aov.h"
#include "renderer.h"

#include "lava_utils_lib/logging.h"


namespace lava {

AOVPlane::SharedPtr AOVPlane::create(const AOVPlaneInfo& info) {
    return SharedPtr(new AOVPlane(info));
}


AOVPlane::AOVPlane(const AOVPlaneInfo& info): mInfo(info) {

}

bool AOVPlane::bindToResource(Falcor::Resource::SharedPtr pResource) {
    assert(pResource);

    if(pResource->getType() != Falcor::Resource::Type::Texture2D) {
        LLOG_ERR << "At this moment only Texture2D resources can be bound to AOV planes !!!";
        return false;
    }

    if(pResource->asTexture()->getFormat() != mInfo.format) {
        LLOG_ERR << "Falcor resource format (" << to_string(pResource->asTexture()->getFormat()) << ") and AOV plane format (" << to_string(mInfo.format) << ") mismatch !!!";
        return false;
    }

    mpResource = pResource;
    return true;
}

bool AOVPlane::getImageData(uint8_t* pData) const {
    assert(pData);

    if(!mpResource) {
        LLOG_ERR << "AOV plane \"" << mInfo.name << "\" is not bound to resource !!!";
        return false;
    }

    Falcor::Texture::SharedPtr pOutputTexture = mpResource->asTexture();
    if (!pOutputTexture) {
        LLOG_ERR << "Buffer AOV outputs not supported (yet) !";
        return false;
    }

    pOutputTexture->readTextureData(0, 0, pData);
    return true;
}

bool AOVPlane::getAOVPlaneGeometry(AOVPlaneGeometry& aov_plane_geometry) const {
    if(!mpResource) {
        LLOG_ERR << "AOV plane \"" << mInfo.name << "\" is not bound to resource !!!";
        return false;
    }

    auto const pTexture = mpResource->asTexture();
    if (!pTexture) {
        LLOG_ERR << "Buffer AOV outputs not supported (yet) !";
        return false;
    }

    auto resourceFormat = pTexture->getFormat();

    aov_plane_geometry.width = pTexture->getWidth(0);
    aov_plane_geometry.height = pTexture->getHeight(0);
    aov_plane_geometry.resourceFormat = resourceFormat;
    aov_plane_geometry.bytesPerPixel = Falcor::getFormatBytesPerBlock(resourceFormat);
    aov_plane_geometry.channelsCount = Falcor::getFormatChannelCount(resourceFormat);
    aov_plane_geometry.bitsPerComponent[0] = Falcor::getNumChannelBits(resourceFormat, 0);
    aov_plane_geometry.bitsPerComponent[1] = Falcor::getNumChannelBits(resourceFormat, 1);
    aov_plane_geometry.bitsPerComponent[2] = Falcor::getNumChannelBits(resourceFormat, 2);
    aov_plane_geometry.bitsPerComponent[3] = Falcor::getNumChannelBits(resourceFormat, 3);

    return true;
}


// HYDRA section end

}  // namespace lava