#ifndef SRC_LAVA_LIB_LOADER_LSD_H_
#define SRC_LAVA_LIB_LOADER_LSD_H_

//#include <boost/lambda/bind.hpp>
//#include <boost/spirit/include/qi.hpp>
//#include <boost/spirit/include/classic.hpp>

#include <memory>

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

    void        init(std::unique_ptr<RendererIfaceBase> pInterface, bool echo) override;

    const char* formatName() const override;
    bool        checkExtension(const char *name) override;
    void        getFileExtensions(std::vector<std::string> &extensions) const override;

    // Method to check if the given magic number matches the magic number. Return true on a match.
    bool        checkMagicNumber(unsigned magic) override;

 private:
 	virtual bool parseStream(std::istream& in) override;
    virtual bool parseLine(const std::string& line, std::string& unparsed) override;

 private:
 	std::unique_ptr<lsd::LSDVisitor>  mpLSDVisitor;

 public:
    // factory methods
    static ReaderBase::SharedPtr        myConstructor();
    static std::vector<std::string>*    myExtensions();
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_LOADER_LSD_H_
