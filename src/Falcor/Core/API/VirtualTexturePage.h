#ifndef SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_
#define SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_

#include <vector>
#include <string>
#include <memory>

namespace Falcor {

class Device;
class Texture;

// Virtual texture page as a part of the partially resident texture
// Contains memory bindings, offsets and status information
class dlldecl VirtualTexturePage: public std::enable_shared_from_this<VirtualTexturePage>  {
 public:
    using SharedPtr = std::shared_ptr<VirtualTexturePage>;
    using SharedConstPtr = std::shared_ptr<const VirtualTexturePage>;

    /** Create a new vertex buffer layout object.
        \return New object, or throws an exception on error.
    */
    static SharedPtr create(const std::shared_ptr<Device>& pDevice, const std::shared_ptr<Texture>& pTexture);

    bool isResident();
    void allocate();
    void release();

    uint3 offset() const { return {mOffset.x, mOffset.y, mOffset.z}; }
    uint3 extent() const { return {mExtent.width, mExtent.height, mExtent.depth}; }

    uint32_t width() const { return mExtent.width; }
    uint32_t height() const { return mExtent.height; }
    uint32_t depth() const { return mExtent.depth; }

    const std::shared_ptr<Texture>  texture() { return mpTexture; }

 protected:
    VirtualTexturePage(const std::shared_ptr<Device>& pDevice, const std::shared_ptr<Texture>& pTexture);

    const std::shared_ptr<Device>   mpDevice;
    const std::shared_ptr<Texture>  mpTexture;

    VkOffset3D mOffset;
    VkExtent3D mExtent;
    VkSparseImageMemoryBind mImageMemoryBind;                           // Sparse image memory bind for this page
    VkDeviceSize mDevMemSize;                                           // Page (memory) size in bytes
    uint32_t mMipLevel;                                                 // Mip level that this page belongs to
    uint32_t mLayer;                                                    // Array layer that this page belongs to
    uint32_t mIndex;

    friend class Texture;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_
