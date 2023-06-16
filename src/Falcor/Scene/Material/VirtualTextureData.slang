#pragma once

#include "Utils/HostDeviceShared.slangh"

#ifdef HOST_CODE
#include "Scene/Material/TextureHandle.slang"
#else
__exported import Scene.Material.TextureHandle;
#endif

// Basic virtual texture data needed
struct VirtualTextureData {
    uint32_t textureID;
    uint32_t empty = true;
    uint32_t textureResolveID;
    uint32_t pagesStartOffset; // Pages start at this index in output pages buffer

    uint16_t width;
    uint16_t height;

    //

    uint16_t pageSizeW;
    uint16_t pageSizeH;    
    
    uint16_t pageSizeD;

    uint8_t mipLevelsCount;
    uint8_t uvSetID;
    
    uint8_t mipTailStart;     // Mip level at which tail data starts
    uint8_t _pad0;

    uint8_t _pad1;
    uint8_t _pad2;

    uint32_t mipBases[16];
};
