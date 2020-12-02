#ifndef SRC_FALCOR_UTILS_IMAGE_LTX_BITMAP_H_
#define SRC_FALCOR_UTILS_IMAGE_LTX_BITMAP_H_

#include <stdio.h>
#include "Falcor/Core/Framework.h"


namespace Falcor {

class Device;
class Texture;
class TextureManager;

const unsigned char gLtxFileMagic[12] = {0xAB, 'L', 'T', 'X', ' ', ' ', ' ', 0xBB, '\r', '\n', '\x1A', '\n'};  // indices 5,6 used to store major,minor versions

struct LTX_Header {
    unsigned char   magic[12] = {0xAB, 'L', 'T', 'X', ' ', '1', '0', 0xBB, '\r', '\n', '\x1A', '\n'};
    time_t          srcWriteTime;
    uint32_t        width = 0;
    uint32_t        height = 0;
    uint32_t        depth = 0;

    struct PageDims {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    } pageDims = {0, 0, 0};

    uint32_t        pageDataSize = 0;
    uint32_t        arrayLayersCount = 0;
    uint8_t         mipLevelsCount = 0;

    ResourceFormat  format;
    
    struct CompressionRatio {
        uint32_t width;
        uint32_t height;
    } compressionRatio = {1, 1};
};

/** A class representing a memory bitmap with sparse storage
*/
class dlldecl LTX_Bitmap : public std::enable_shared_from_this<LTX_Bitmap> {
 public:
    enum class ExportFlags : uint32_t {
        None = 0u,              //< Default
        Lossy = 1u << 0,        //< Try to store in a lossy format
        Uncompressed = 1u << 1, //< Prefer faster load to a more compact file size
    };

    enum class FileFormat {
        PngFile,    //< PNG file for lossless compressed 8-bits images with optional alpha
        JpegFile,   //< JPEG file for lossy compressed 8-bits images without alpha
        TgaFile,    //< TGA file for lossless uncompressed 8-bits images with optional alpha
        BmpFile,    //< BMP file for lossless uncompressed 8-bits images with optional alpha
        PfmFile,    //< PFM file for floating point HDR images with 32-bit float per channel
        ExrFile,    //< EXR file for floating point HDR images with 16-bit float per channel
    };

    using UniquePtr = std::unique_ptr<LTX_Bitmap>;
    using UniqueConstPtr = std::unique_ptr<const LTX_Bitmap>;

    /** Create a new object from file.
        \param[in] filename Filename, including a path. If the file can't be found relative to the current directory, Falcor will search for it in the common directories.
        \param[in] isTopDown Control the memory layout of the image. If true, the top-left pixel is the first pixel in the buffer, otherwise the bottom-left pixel is first.
        \return If loading was successful, a new object. Otherwise, nullptr.
    */
    static UniquePtr createFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool isTopDown = true);

    
    /** Store a memory buffer to a sparse file.
        \param[in] filename Output filename. Can include a path - absolute or relative to the executable directory.
        \param[in] width The width of the image.
        \param[in] height The height of the image.
        \param[in] ResourceFormat the format of the resource data
        \param[in] pData Pointer to the buffer containing the image
    */
    static void saveToFile(const std::string& filename, uint32_t width, uint32_t height, ResourceFormat resourceFormat, void* pData);

    ~LTX_Bitmap();

    /** Get data size in bytes
    */
    size_t getDataSize() const { return mDataSize; }

    /** Get resident data size in bytes
    */
    size_t getResidentDataSize() const;

    /** Get a pointer to the bitmap's data store
    */
    uint8_t* getData() const { return mpData; }

    /** Get the width of the bitmap
    */
    uint32_t getWidth() const { return mHeader.width; }

    /** Get the height of the bitmap
    */
    uint32_t getHeight() const { return mHeader.height; }

    /** Get the depth of the bitmap
    */
    uint32_t getDepth() const { return mHeader.depth; }

    /** Get mip levels count of ltx bitmap
    */
    uint8_t getMipLevelsCount() const { return mHeader.mipLevelsCount; }

    /** Get the number of bytes per pixel
    */
    ResourceFormat getFormat() const { return mHeader.format; }

    
 protected:
    static void convertToKtxFile(std::shared_ptr<Device> pDevice, const std::string& srcFilename, const std::string& dstFilename, bool isTopDown);

    void readPageData (size_t pageNum, void *pData);
    void readPagesData (std::vector<std::pair<size_t, void*>>& pages, bool unsorted = true);

    friend class TextureManager;

 private:
    LTX_Bitmap();

    static bool checkMagic(const unsigned char* magic);
    static void makeMagic(uint8_t minor, uint8_t major, unsigned char *magic);

    FILE*       mpFile = nullptr;
    uint8_t*    mpData = nullptr;
    size_t      mDataSize = 0;
    
    uint32_t    mWidth = 0;
    uint32_t    mHeight = 0;
    uint32_t    mDepth = 0;

    uint8_t     mMipLevelsCount = 0;
    uint32_t    mArrayLayersCount = 1;

    std::string mFilename;

    ResourceFormat mFormat;

    LTX_Header mHeader;
};

enum_class_operators(LTX_Bitmap::ExportFlags);

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_IMAGE_BITMAP_H_
