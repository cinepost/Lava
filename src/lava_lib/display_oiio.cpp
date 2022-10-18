#include <memory>
#include <array>

#include <dlfcn.h>
#include <stdlib.h>
#include <boost/format.hpp>

#include "display_oiio.h"
#include "lava_utils_lib/logging.h"

namespace lava {

inline static oiio::TypeDesc channelFormatToOIIO(Display::TypeFormat display_format) {
    switch(display_format) {
        case Display::TypeFormat::FLOAT16:
            return oiio::TypeDesc::BASETYPE::HALF;
        case Display::TypeFormat::FLOAT32:
            return oiio::TypeDesc::BASETYPE::FLOAT;
        case Display::TypeFormat::UNSIGNED16:
            return oiio::TypeDesc::BASETYPE::UINT16;
        case Display::TypeFormat::SIGNED16:
            return oiio::TypeDesc::BASETYPE::INT16;
        case Display::TypeFormat::UNSIGNED8:
            return oiio::TypeDesc::BASETYPE::UINT8;
        case Display::TypeFormat::SIGNED8:
            return oiio::TypeDesc::BASETYPE::INT8;
        default:
            break;
    }
    return oiio::TypeDesc::BASETYPE::UINT8;
}

DisplayOIIO::DisplayOIIO() {
    mCurrentImageID = 0;
    mImages.clear();
}

DisplayOIIO::~DisplayOIIO() {
    if (!closeAll())
        std::cerr << "Error closing images !" << std::endl;
}


Display::SharedPtr DisplayOIIO::create(Display::DisplayType display_type) {
	DisplayOIIO* pDisplay = new DisplayOIIO();

    pDisplay->mInteractiveSupport = false;
    pDisplay->mDisplayType = display_type;

    return SharedPtr((Display*)pDisplay);
}

bool DisplayOIIO::_open(uint imageHandle) {
    if( !hasImage(imageHandle) ) return false;

    auto& imData = mImages.at(imageHandle);
    const std::string& image_name = imData.name;

    if(imData.isOpened() && !imData.isMultiImage() && !imData.isSubImage()) {
        LLOG_WRN << "Image " << imData.name << " already opened !";
        return false;
    }

    if(imData.isMultiImage()) {
        // Open multiimage
        std::vector<oiio::ImageSpec> specs;
        specs.push_back(imData.spec);
        for (uint subImageHandle: imData.subImageHandles) {
            auto& subImData = mImages.at(subImageHandle);
            specs.push_back(subImData.spec);
        }

        LLOG_WRN << "Opening multiimage for " << std::to_string(specs.size()) << " sub images";
        if(!imData.pOut->open(image_name, specs.size(), specs.data())) {
            LLOG_ERR << "Error opening multi-image " << image_name << " !!!";
            return false;
        } 
    } else if(imData.isSubImage()) { 
        // Open subimage
        auto& masterImData = mImages.at(imData.masterImageHandle);
        if(!masterImData.pOut->open(image_name, imData.spec, oiio::ImageOutput::AppendSubimage)) {
            LLOG_ERR << "Error opening sub-image " << image_name << " !!!";
            return false;
        }
        
    } else {
        // Open ordinary image
        if(!imData.pOut->open(image_name, imData.spec)) {
            LLOG_ERR << "Error opening image " << image_name << " !!!";
            return false;
        }
    }

    return true;
}

bool DisplayOIIO::openImage(const std::string& image_name, uint width, uint height, Falcor::ResourceFormat format, uint &imageHandle, std::string channel_prefix) {
    std::vector<Channel> channels;

    Falcor::FormatType format_type = Falcor::getFormatType(format);
    uint32_t numChannels = Falcor::getFormatChannelCount(format);

    for( uint32_t i = 0; i < numChannels; i++) {
        uint32_t numChannelBits = Falcor::getNumChannelBits(format, (int)i);
        channels.push_back(makeDisplayChannel(channel_prefix, i, format_type, numChannelBits, NamingScheme::RGBA));
    }

    return openImage(image_name, width, height, channels, imageHandle);
}

bool DisplayOIIO::openImage(const std::string& image_name, uint width, uint height, const std::vector<Channel>& channels, uint &imageHandle) {
    if( channels.size() < 1) { LLOG_FTL << "No image channels specified !!!"; return false; }
    if( width == 0 || height == 0) { LLOG_FTL << "Wrong image dimensions !!!"; return false; }

    uint masterImageHandle = 0;
    ImageData* pExistingImageData = nullptr;

    for(auto& entry: mImages ) {
        const auto& existingImData = entry.second;
        if((existingImData.name == image_name) && !existingImData.isSubImage()) { // && existingImData.isOpened()) {
            if(existingImData.supportsMultiImage) {
                // Try to add subimage
                LLOG_WRN << "Adding subimage to " << image_name;
                for(auto const& channel: channels) {
                    LLOG_WRN << "subimage channel " << channel.name;
                }

                masterImageHandle = entry.first;
                pExistingImageData = &entry.second;
                break;    
            } else {
                // Try to output subimage into different file
            
                break;
            }

            LLOG_ERR << "Image " << image_name << " already opened!";
            return false;
        }
    }

    uint entrySize = 0;
    bool allChannelFormalsAreEqual = true;

    auto _tmp_fmt = Display::TypeFormat::UNKNOWN;
    for(const auto& channel: channels){
        if(_tmp_fmt == Display::TypeFormat::UNKNOWN) { 
            _tmp_fmt = channel.format;
        } else {
            if (_tmp_fmt != channel.format) allChannelFormalsAreEqual = false;
        }
        entrySize += getFormatSizeInBytes(channel.format);
    }

    ImageData imData = {};
    imData.name = image_name;
    imData.width = width;
    imData.height = height;
    imData.opened = false;
    imData.closed = false;
    imData.entrySize = entrySize;
    imData.channels = channels;

    if(pExistingImageData) {
        imData.masterImageHandle = masterImageHandle;
        imData._isSubImage = true;
    } else {
        imData.pOut = oiio::ImageOutput::create(image_name);
        imData.supportsRandomAccess = imData.pOut->supports("random_access");
        imData.supportsTiles = imData.pOut->supports("tiles");
        imData.supportsMultiImage =  imData.pOut->supports("multiimage");
        imData.supportsAppendSubImage =  imData.pOut->supports("appendsubimage");
        imData.supportsPerChannelFormats = imData.pOut->supports("channelformats");
        imData.supportsRectangles = imData.pOut->supports("rectangles");
        imData.supportsMipMaps = imData.pOut->supports("mipmap");

        LLOG_DBG << "imData.supportsRandomAccess " << (imData.supportsRandomAccess ? "true" : "false");
        LLOG_DBG << "imData.supportsTiles " << (imData.supportsTiles ? "true" : "false");
        LLOG_DBG << "imData.supportsMultiImage " << (imData.supportsMultiImage ? "true" : "false");
        LLOG_DBG << "imData.supportsAppendSubImage " << (imData.supportsAppendSubImage ? "true" : "false");
        LLOG_DBG << "imData.supportsPerChannelFormats " << (imData.supportsPerChannelFormats ? "true" : "false");
        LLOG_DBG << "imData.supportsRectangles " << (imData.supportsRectangles ? "true" : "false");

    }

    auto& spec = imData.spec;
    spec = oiio::ImageSpec(width, height, channels.size(), channelFormatToOIIO(channels[0].format));
    spec.channelnames.clear();

    if(!allChannelFormalsAreEqual || imData.supportsPerChannelFormats || (pExistingImageData && pExistingImageData->supportsPerChannelFormats)) {
        for(const auto& channel: channels) {
            LLOG_WRN << "Channel " << channel.name << " format is " << to_string(channel.format);
            spec.channelformats.push_back(channelFormatToOIIO(channel.format));
            spec.channelnames.push_back(channel.name);
        }
    }

    if(!imData.supportsTiles) {
        imData.forceScanlines = true;

        // Allocate temporary image buffer here
        imData.tmpDataBuffer.resize(imData.width * imData.height * imData.entrySize);
        std::fill(imData.tmpDataBuffer.begin(), imData.tmpDataBuffer.end(), (uint8_t)0);
    }

    imageHandle = mCurrentImageID++;
    mImages[imageHandle] = std::move(imData);

    if(pExistingImageData) {
        pExistingImageData->subImageHandles.push_back(imageHandle);
    }

    return true;
}

bool DisplayOIIO::closeAll() {
    bool ret = true;

    for (uint i = 0; i < mImages.size(); i++) {
        if (!closeImage(i)) ret = false;
    }
    
    mImages.clear();
    return ret;
}

bool DisplayOIIO::opened(uint imageHandle) const {
    if( hasImage(imageHandle) ) return mImages.at(imageHandle).isOpened();
    
    return false; 
}

bool DisplayOIIO::closed(uint imageHandle) const {
    if( hasImage(imageHandle) ) return mImages.at(imageHandle).isClosed();
    
    return false; 
}

const std::string& DisplayOIIO::imageName(uint imageHandle) const {
    if( hasImage(imageHandle) ) return mImages.at(imageHandle).name;
    throw std::runtime_error("Invalid imageHandle !!!");
}

uint DisplayOIIO::imageWidth(uint imageHandle) const {
    if( hasImage(imageHandle) ) return mImages.at(imageHandle).width;
    throw std::runtime_error("Invalid imageHandle !!!");
}

uint DisplayOIIO::imageHeight(uint imageHandle) const {
    if( hasImage(imageHandle) ) return mImages.at(imageHandle).height;
    throw std::runtime_error("Invalid imageHandle !!!");
}

bool DisplayOIIO::closeImage(uint imageHandle) {
    auto found = mImages.find(imageHandle);
    if(found == mImages.end()) {
        LLOG_ERR << "Image width handle " << std::to_string(imageHandle) << " does not exist!";
        return false;
    }

    auto& imData = found->second;

    if(imData.closed) // already closed
        return true;

    if(!imData.pOut) // open was unsuccessfull
        return false;

    bool result = true;

    // If driver expects ONLY scan lines then the data is stored in temporary buffer
    if (imData.forceScanlines) {
        LLOG_DBG << "Sending " <<  std::to_string(imData.height) << " forced scan lines from temporary buffer";
        

    }

    imData.pOut->close();

    imData.opened = false;
    imData.closed = true;
    return result;
}

bool DisplayOIIO::sendImageRegion(uint imageHandle, uint x, uint y, uint width, uint height, const uint8_t *pData) {
    auto const& imData = mImages[imageHandle];

    if(!imData.opened) {
        LLOG_ERR << "Can't send image data. Display not opened !!!";
        return false;
    }

    if(!imData.supportsRectangles) {
        // Send tile data 
        imData.pOut->write_rectangle(x, x + width, y, y + width, 0, 1, oiio::TypeDesc::UINT8, pData);
    } else {
        // Store data to temporary buffer
        size_t src_data_line_size = imData.entrySize * width;
        size_t dst_data_line_size = imData.entrySize * imData.width;
        
        const uint8_t* pSrcData = pData;
        uint8_t* pDstData = (uint8_t *)imData.tmpDataBuffer.data() + ((y * imData.width) + x) * imData.entrySize;

        for(uint i = y; i < (y + height); i++) {
            ::memcpy(pDstData, pSrcData, src_data_line_size);
            pSrcData += src_data_line_size;
            pDstData += dst_data_line_size;
        }
    }

    return true;
}

bool DisplayOIIO::sendImage(uint imageHandle, uint width, uint height, const uint8_t *pData) {
    auto const& imData = mImages[imageHandle];

    //if(!imData.opened) {
    //    LLOG_ERR << "Can't send image data. Display not opened !!!";
    //    return false;
    //}

    if( width != imData.width || height != imData.height) {
        LLOG_ERR << "Display and sended image sizes are different !!!";
        return false;
    }

    if(!_open(imageHandle)) return false;
    
    if(!imData.isSubImage() && imData.pOut) {
        imData.pOut->write_image(oiio::TypeDesc::UNKNOWN, pData, imData.entrySize);
    } else {
        auto pMasterImData = masterImageData(imageHandle);
        if(!pMasterImData) { LLOG_ERR << "Unable to find master image for multi-image " << imData.name << "!!!"; return false; }
        pMasterImData->pOut->write_image(oiio::TypeDesc::UNKNOWN, pData, imData.entrySize);
    } 

    return true;
}

bool DisplayOIIO::setStringParameter(const std::string& name, const std::vector<std::string>& strings) {
    return true;
}

bool DisplayOIIO::setIntParameter(const std::string& name, const std::vector<int>& ints) {
    return true;
}

bool DisplayOIIO::setFloatParameter(const std::string& name, const std::vector<float>& floats) {
    return true;
}

/* static */
bool DisplayOIIO::isDiplayTypeSupported(Display::DisplayType display_type) {
    switch(display_type) {
        case Display::DisplayType::OPENEXR:
        case Display::DisplayType::PNG:
        case Display::DisplayType::JPEG:
            return true;
        default:
            break;
    }
    return false;
}

}  // namespace lava