#ifndef SRC_FALCOR_UTILS_IMAGE_LTX_BITMAP_H_
#define SRC_FALCOR_UTILS_IMAGE_LTX_BITMAP_H_

#include <stdio.h>
#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Formats.h"


namespace Falcor {

class Device;
class Texture;
class TextureManager;
class ResourceManager;

const size_t kLtxPageSize = 65536;
const uint8_t kLtxVersionMajor = 1;
const uint8_t kLtxVersionMinor = 2;
const uint8_t kLtxVersionBuild = 2;

const unsigned char gLtxFileMagic[12] = {0xAB, 'L', 'T', 'X', '0', '0', '0', 0xBB, '\r', '\n', '\x1A', '\n'};  // indices 4,5,6 used to store major,minor,build versions

struct dlldecl LTX_Header {
    unsigned char   magic[12];
    time_t          srcLastWriteTime; // Source texture creation time
    uint32_t        width = 0;
    uint32_t        height = 0;
    uint32_t        depth = 0;

    struct PageDims {
        uint16_t width;
        uint16_t height;
        uint16_t depth;
    } pageDims = {0, 0, 0};

    uint32_t        pagesCount = 0; // number of mem pages that were physycally written
    uint32_t        pageDataSize = 0;
    uint32_t        arrayLayersCount = 0;
    uint8_t         mipLevelsCount = 0;
    uint8_t         mipTailStart = 0;
    uint32_t        tailDataOffset = 0;
    uint32_t        tailDataSize = 0;
    uint32_t        mipBases[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // starting tile id at each mip level

    ResourceFormat  format;
    
    enum class Flags: uint8_t {
        NONE              = 0x0,
        ONE_PAGE_MIP_TAIL = 0x1,
    } flags = Flags::NONE;

    struct CompressionRatio {
        uint8_t width;
        uint8_t height;
    } textureCompressionRatio = {1, 1}; // texture data compression ratio

    enum TopLevelCompression: uint8_t {
        NONE,
        BLOSC_LZ,
        LZ4,
        LZ4HC,
        SNAPPY,
        ZLIB,
        ZSTD
    } topLevelCompression = TopLevelCompression::LZ4;
    uint8_t topLevelCompressionLevel = 5; // 0 - 9

    uint32_t dataOffset = sizeof(LTX_Header);

    std::string versionString() const;
};

struct LTX_MipInfo {
    uint8_t mipLevelsCount = 0;
    uint8_t mipTailStart = 0;
    uint3   pageDims;
    std::vector<uint3> mipLevelsDims;
};


/** A class representing a memory bitmap with sparse storage
*/
class dlldecl LTX_Bitmap : public std::enable_shared_from_this<LTX_Bitmap> {
 public:
    struct TLCParms {
        std::string compressorName = "";
        uint8_t compressionLevel = 0;   
    };

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

    using SharedPtr = std::shared_ptr<LTX_Bitmap>;
    using SharedConstPtr = std::shared_ptr<const LTX_Bitmap>;

    /** Create a new object from file.
        \param[in] filename Filename, including a path. If the file can't be found relative to the current directory, Falcor will search for it in the common directories.
        \param[in] isTopDown Control the memory layout of the image. If true, the top-left pixel is the first pixel in the buffer, otherwise the bottom-left pixel is first.
        \return If loading was successful, a new object. Otherwise, nullptr.
    */
    static SharedConstPtr createFromFile(std::shared_ptr<Device> pDevice, const fs::path& path, bool isTopDown = true);
    static SharedConstPtr createFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool isTopDown = true);

    
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

    const LTX_Header& header() const { return mHeader; }

    uint32_t getMipTailStart() const { return mHeader.mipTailStart; }

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

    uint8_t getPagesCount() const { return mHeader.pagesCount; }

    LTX_Header::Flags getFlags() const { return mHeader.flags; }

    /** Get the number of bytes per pixel
    */
    ResourceFormat getFormat() const { return mHeader.format; }

    const fs::path& getFilePath() const { return mFilePath; }
    const std::string getFileName() const { return mFilePath.string(); }

    
 protected:
    bool readPageData(size_t pageNum, uint8_t *pData) const;
    bool readPageData(size_t pageNum, uint8_t *pData, FILE *pFile, uint8_t *pScratchBuffer) const;
    bool readTailData(std::vector<uint8_t>& data) const;
    bool readTailData(FILE *pFile, std::vector<uint8_t>& data, uint8_t *pScratchBuffer) const;
    bool readTailData(FILE *pFile, uint8_t *pData, uint8_t *pScratchBuffer) const;

    friend class ResourceManager;
    friend class TextureManager;

 public:
    static bool convertToLtxFile(std::shared_ptr<Device> pDevice, const std::string& srcFilename, const std::string& dstFilename, const TLCParms& compParms, bool isTopDown = true);
    static LTX_Header::TopLevelCompression getTLCFromString(const std::string& name);
    static bool checkFileMagic(const std::string& filename, bool strict = false);
    static bool checkFileMagic(const fs::path& path, bool strict = false);

 private:
    LTX_Bitmap();
    
    uint8_t*    mpData = nullptr;
    size_t      mDataSize = 0;
    
    uint32_t    mWidth = 0;
    uint32_t    mHeight = 0;
    uint32_t    mDepth = 0;

    uint8_t     mMipLevelsCount = 0;
    uint32_t    mArrayLayersCount = 1;

    fs::path    mFilePath;

    ResourceFormat mFormat;

    LTX_Header mHeader;

    bool mTopLevelCompression = LTX_Header::TopLevelCompression::NONE;
    std::vector<uint32_t> mCompressedPageDataOffset;
    std::vector<uint16_t> mCompressedPageDataSize;
};

enum_class_operators(LTX_Bitmap::ExportFlags);
enum_class_operators(LTX_Header::Flags);

const std::string dlldecl to_string(LTX_Header::TopLevelCompression);
const char* getBloscCompressionName(LTX_Header::TopLevelCompression);

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_IMAGE_BITMAP_H_
