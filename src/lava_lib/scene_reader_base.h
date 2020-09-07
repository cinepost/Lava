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
    
    bool                        read(SharedPtr iface, std::istream &input, bool echo);

 public:
    virtual ~ReaderBase();

    virtual const char*			formatName() const = 0;
    virtual bool                checkExtension(const char *name) = 0;

    virtual void                getFileExtensions(std::vector<std::string>& extensions) const = 0;

    // Method to check if the given magic number matches the magic number. Return true on a match.
    virtual bool                checkMagicNumber(unsigned magic) = 0;

 private:
    virtual void                parseLine(const std::string& line, bool echo) = 0;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_SCENE_READER_BASE_H_