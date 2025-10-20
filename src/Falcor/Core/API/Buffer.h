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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#ifndef SRC_FALCOR_CORE_API_BUFFER_H_
#define SRC_FALCOR_CORE_API_BUFFER_H_

#include <string>

#include "Falcor/Core/API/Common.h"
#include "Falcor/Core/API/Resource.h"
#include "Falcor/Core/API/ResourceViews.h"

#include "gfx_lib/slang-gfx.h"


namespace Falcor {

class Device;
class Program;
struct ShaderVar;

/** Low-level buffer object
    This class abstracts the API's buffer creation and management
*/
class FALCOR_API Buffer : public Resource {
    FALCOR_OBJECT(Buffer)
  public:
    static constexpr uint64_t kEntireBuffer = ResourceViewInfo::kEntireBuffer;

    /// Constructor.
    Buffer(
        Falcor::SharedPtr<Device> pDevice,
        size_t size,
        size_t structSize,
        ResourceFormat format,
        ResourceBindFlags bindFlags,
        MemoryType memoryType,
        const void* pInitData
    );

    /// Constructor for raw buffer.
    Buffer(Falcor::SharedPtr<Device> pDevice, size_t size, ResourceBindFlags bindFlags, MemoryType memoryType, const void* pInitData);

    /// Constructor for typed buffer.
    Buffer(
        Falcor::SharedPtr<Device> pDevice,
        ResourceFormat format,
        uint32_t elementCount,
        ResourceBindFlags bindFlags,
        MemoryType memoryType,
        const void* pInitData
    );

    /// Constructor for structured buffer.
    Buffer(
        Falcor::SharedPtr<Device> pDevice,
        uint32_t structSize,
        uint32_t elementCount,
        ResourceBindFlags bindFlags,
        MemoryType memoryType,
        const void* pInitData,
        bool createCounter
    );

    /// Constructor with existing resource.
    Buffer(Falcor::SharedPtr<Device> pDevice, gfx::IBufferResource* pResource, size_t size, ResourceBindFlags bindFlags, MemoryType memoryType);

    /// Constructor with native handle.
    Buffer(Falcor::SharedPtr<Device> pDevice, VkBuffer handle, size_t size, ResourceBindFlags bindFlags, MemoryType memoryType);

    ~Buffer();


    gfx::IBufferResource* getGfxBufferResource() const { return mGfxBufferResource; }

    virtual gfx::IResource* getGfxResource() const override;

    /** Get a shader-resource view.
        \param[in] firstElement The first element of the view. For raw buffers, an element is a single float
        \param[in] elementCount The number of elements to bind
    */
    ShaderResourceView::SharedPtr getSRV(uint64_t offset, uint64_t size = kEntireBuffer);

    /** Get an unordered access view.
        \param[in] firstElement The first element of the view. For raw buffers, an element is a single float
        \param[in] elementCount The number of elements to bind
    */
    UnorderedAccessView::SharedPtr getUAV(uint64_t offset, uint64_t size = kEntireBuffer);

    /** Get a shader-resource view for the entire resource
    */
    virtual ShaderResourceView::SharedPtr getSRV() override;

    /** Get an unordered access view for the entire resource
    */
    virtual UnorderedAccessView::SharedPtr getUAV() override;

    /** Get the size of each element in this buffer.

        For a typed buffer, this will be the size of the format.
        For a structured buffer, this will be the same value as `getStructSize()`.
        For a raw buffer, this will be the number of bytes.
    */
    uint32_t getElementSize() const;

    /** Update the buffer's data
        \param[in] pData Pointer to the source data.
        \param[in] offset Byte offset into the destination buffer, indicating where to start copy into.
        \param[in] size Number of bytes to copy.
        If offset and size will cause an out-of-bound access to the buffer, an error will be logged and the update will fail.
    */
    virtual void setBlob(const void* pData, size_t offset, size_t size);

    /**
     * Read the buffer's data
     * @param pData Pointer to the destination data.
     * @param offset Byte offset into the source buffer, indicating where to start copy from.
     * @param size Number of bytes to copy.
     */
    void getBlob(void* pData, size_t offset, size_t size) const;

    /** Get the offset from the beginning of the GPU resource
    */
    uint64_t getGpuAddressOffset() const { return mGpuVaOffset; };

    /** Get the GPU address (this includes the offset)
    */
    uint64_t getGpuAddress() const;

    /** Get the size of the buffer
    */
    size_t getSize() const { return mSize; }

    /** Get the element count. For structured-buffers, this is the number of structs. For typed-buffers, this is the number of elements. For other buffer, will return 0
    */
    uint32_t getElementCount() const { return mElementCount; }

    /** Get the size of a single struct. This call is only valid for structued-buffer. For other buffer types, it will return 0
    */
    uint32_t getStructSize() const { return mStructSize; }

    /** Get the buffer format. This call is only valid for typed-buffers, for other buffer types it will return ResourceFormat::Unknown
    */
    ResourceFormat getFormat() const { return mFormat; }

    /** Get the UAV counter buffer
    */
    const Buffer::SharedPtr& getUAVCounter() const { return mpUAVCounter; }

    /** Map the buffer.
    */
    void* map() const;

    /** Unmap the buffer
    */
    void unmap() const;

    /** Get safe offset and size values
    */
    bool adjustSizeOffsetParams(size_t& size, size_t& offset) const;

    /**
     * Get the memory type
     */
    MemoryType getMemoryType() const { return mMemoryType; }

    /** Check if this is a typed buffer
    */
    bool isTyped() const { return mFormat != ResourceFormat::Unknown; }

    /** Check if this is a structured-buffer
    */
    inline bool isStructured() const { return mStructSize != 0; }

    template<typename T>
    void setElement(uint32_t index, T const& value) {
        setBlob(&value, sizeof(T)*index, sizeof(T));
    }

    template<typename T>
    std::vector<T> getElements(uint32_t firstElement = 0, uint32_t elementCount = 0) const {
        if (elementCount == 0) {
            elementCount = (mSize / sizeof(T)) - firstElement;
        }

        std::vector<T> data(elementCount);
        getBlob(data.data(), firstElement * sizeof(T), elementCount * sizeof(T));
        return data;
    }

    template<typename T>
    T getElement(uint32_t index) const {
        T data;
        getBlob(&data, index * sizeof(T), sizeof(T));
        return data;
    }

  protected:
    virtual void apiSetName() override;

    Slang::ComPtr<gfx::IBufferResource> mGfxBufferResource;

    MemoryType mMemoryType;
    uint32_t mElementCount = 0;
    ResourceFormat mFormat = ResourceFormat::Unknown;
    uint32_t mStructSize = 0;
    Buffer::SharedPtr mpUAVCounter; // For structured-buffers
    mutable void* mMappedPtr = nullptr;

};

template<typename T>
struct FormatForElementType
{};

#define CASE(TYPE, FORMAT) template<> struct FormatForElementType<TYPE> { static const ResourceFormat kFormat = FORMAT; }

    // Guaranteed supported formats on D3D12.
    CASE(float,     ResourceFormat::R32Float);
    CASE(uint32_t,  ResourceFormat::R32Uint);
    CASE(int32_t,   ResourceFormat::R32Int);

    // Optionally supported formats as a set on D3D12. If one is supported all are supported.
    CASE(float4,    ResourceFormat::RGBA32Float);
    CASE(uint4,     ResourceFormat::RGBA32Uint);
    CASE(int4,      ResourceFormat::RGBA32Int);
    //R16G16B16A16_FLOAT
    //R16G16B16A16_UINT
    //R16G16B16A16_SINT
    //R8G8B8A8_UNORM
    //R8G8B8A8_UINT
    //R8G8B8A8_SINT
    //R16_FLOAT
    CASE(uint16_t,  ResourceFormat::R16Uint);
    CASE(int16_t,   ResourceFormat::R16Int);
    //R8_UNORM
    CASE(uint8_t,   ResourceFormat::R8Uint);
    CASE(int8_t,    ResourceFormat::R8Int);

    // Optionally and individually supported formats on D3D12. Query for support individually.
    //R16G16B16A16_UNORM
    //R16G16B16A16_SNORM
    CASE(float2,    ResourceFormat::RG32Float);
    CASE(uint2,     ResourceFormat::RG32Uint);
    CASE(int2,      ResourceFormat::RG32Int);
    //R10G10B10A2_UNORM
    //R10G10B10A2_UINT
    //R11G11B10_FLOAT
    //R8G8B8A8_SNORM
    //R16G16_FLOAT
    //R16G16_UNORM
    //R16G16_UINT
    //R16G16_SNORM
    //R16G16_SINT
    //R8G8_UNORM
    //R8G8_UINT
    //R8G8_SNORM
    //8G8_SINT
    //R16_UNORM
    //R16_SNORM
    //R8_SNORM
    //A8_UNORM
    //B5G6R5_UNORM
    //B5G5R5A1_UNORM
    //B4G4R4A4_UNORM

    // Additional formats that may be supported on some hardware.
    CASE(float3,    ResourceFormat::RGB32Float);
    
#undef CASE

inline std::string to_string(const Buffer::SharedPtr& buff) {
    std::string s = "Buffer: ";
    s += "cpu access " + to_string(buff->getType());
    return s;
}

inline std::string to_string(Buffer *buff) {
    std::string s = "Buffer: ";
    s += "cpu access " + to_string(buff->getType());
    return s;
}

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_BUFFER_H_
