#ifndef SRC_LAVA_LIB_READER_LSD_READER_LSD_H_
#define SRC_LAVA_LIB_READER_LSD_READER_LSD_H_

#include <memory>

#include "../scene_reader_base.h"
#include "session.h"
#include "visitor.h"


namespace lava {

static std::vector<std::string> _lsd_extensions = {".lsd",".lsd.gz",".lsd.zip"};

typedef std::string::const_iterator It;

class ReaderLSD: public ReaderBase {
 public:
 	ReaderLSD();
 	~ReaderLSD() override;

    virtual void    init(std::shared_ptr<Renderer> pRenderer, bool echo) override;

    const char*     formatName() const override;
    bool            checkExtension(const char *name) override;
    void            getFileExtensions(std::vector<std::string> &extensions) const override;

    // Method to check if the given magic number matches the magic number. Return true on a match.
    bool            checkMagicNumber(unsigned magic) override;

  private:
    virtual bool    isInitialized() override;
 	  virtual bool    parseStream(std::istream& in) override;

 private:
    std::unique_ptr<lsd::Visitor>   mpVisitor;
    bool mInitialized;

    std::shared_ptr<std::istream>   mpStream; // used in inline bgeo read callback. a bit ugly.

 public:
    bool readInlineBGEO(ika::bgeo::Bgeo& bgeo);

 public:
    // factory methods
    static ReaderBase::SharedPtr        myConstructor();
    static std::vector<std::string>*    myExtensions();

    bool mEchoInput = false;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_READER_LSD_H_
