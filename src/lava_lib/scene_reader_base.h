#ifndef SRC_LAVA_LIB_SCENE_READER_BASE_H_
#define SRC_LAVA_LIB_SCENE_READER_BASE_H_

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <memory>

#include "renderer_iface_base.h"


namespace lava {

class ReaderBase {
 public:
  	using SharedPtr = std::shared_ptr<ReaderBase>;

    ReaderBase();

    bool                        readStream(std::istream &input);
    bool                        read(std::istream &input);
    bool                        isInitialized() { return mIsInitialized; }                       
    virtual void                init(std::unique_ptr<RendererIfaceBase> pInterface, bool echo);

 public:
    virtual ~ReaderBase();

    virtual const char*			formatName() const = 0;
    virtual bool                checkExtension(const char *name) = 0;

    virtual void                getFileExtensions(std::vector<std::string>& extensions) const = 0;

    // Method to check if the given magic number matches the magic number. Return true on a match.
    virtual bool                checkMagicNumber(unsigned magic) = 0;

 private:
    virtual bool                parseStream(std::istream &input) = 0;
    virtual bool                parseLine(const std::string& line, std::string& unparsed) = 0;

 protected:
    bool    mIsInitialized;
    bool    mEcho;
    std::unique_ptr<RendererIfaceBase> mpInterface;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_SCENE_READER_BASE_H_