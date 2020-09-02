#ifndef SRC_LAVA_LIB_READER_LSD_H_
#define SRC_LAVA_LIB_READER_LSD_H_

#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <boost/spirit.hpp>

#include "../scene_reader_base.h"


namespace lava {}

static std::vector<std::string> _lsd_extensions = {".lsd",".lsd.gz",".lsd.zip"};

class ReaderLSD : public ReaderBase {
  public:
    ReaderLSD();
    ~ReaderLSD();

    const char*			formatName() const;
    int                 checkExtension(const char *name);

    void                getFileExtensions(std::vector<std::string> &extensions) const;

    // Method to check if the given magic number matches the magic number. Return true on a match.
    int                 checkMagicNumber(unsigned magic);

    SCN_Scene::IOStatus fileLoad(SCN_Scene &scn, std::istream &in, bool ate_magic) override;

 public:
 	// factory methods
    static ReaderBase* 					myConstructor();
    static std::vector<std::string>*	myExtensions();
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_H_