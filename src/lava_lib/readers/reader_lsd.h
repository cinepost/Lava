#ifndef SRC_LAVA_LIB_LOADER_LSD_H_
#define SRC_LAVA_LIB_LOADER_LSD_H_

//#include <boost/lambda/bind.hpp>
//#include <boost/spirit/include/qi.hpp>
//#include <boost/spirit/include/classic.hpp>

#include "../scene_reader_base.h"
#include "renderer_iface_lsd.h"
#include "grammar_lsd.h"


namespace lava {

static std::vector<std::string> _lsd_extensions = {".lsd",".lsd.gz",".lsd.zip"};

typedef std::string::const_iterator It;

class ReaderLSD: public ReaderBase {
 public:
 	ReaderLSD();
 	~ReaderLSD();

    const char* formatName() const;
    bool        checkExtension(const char *name);
    void        getFileExtensions(std::vector<std::string> &extensions) const;

    // Method to check if the given magic number matches the magic number. Return true on a match.
    bool        checkMagicNumber(unsigned magic);

 private:
 	virtual void parseLine(const std::string& line, bool echo);

 private:
 	//command_grammar<std::string::const_iterator> const         commands;
 	RendererIfaceLSD::SharedPtr   mpIface;

 public:
    // factory methods
    static ReaderBase::SharedPtr        myConstructor();
    static std::vector<std::string>*    myExtensions();
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_LOADER_LSD_H_
