#include <vector>
#include <fstream>
#include <iterator>
#include <regex>
#include <chrono>

#include <stdio.h>

#include <boost/spirit/include/support_istream_iterator.hpp>

#include "rapidjson/document.h"

#include "reader_lsd.h"
#include "grammar_lsd.h"
#include "../reader_bgeo/bgeo/Bgeo.h"

#include "lava_utils_lib/logging.h"

namespace x3 = boost::spirit::x3;

namespace lava {


ReaderLSD::ReaderLSD(): ReaderBase(), mInitialized(false) { }

ReaderLSD::~ReaderLSD() { }

void ReaderLSD::init(std::unique_ptr<RendererIface> pRendererInterface, bool echo) {
    auto pSession = lsd::Session::create(std::move(pRendererInterface));
    if (!pSession) {
        LLOG_ERR << "Error initializing session !!!";
        return;
    }

    if (!echo) {
        // standard LSD visitor
        mpVisitor = std::make_unique<lsd::Visitor>(pSession);
    } else {
        // LSD visitor with parsed console echo (for debug purposes)
        mpVisitor = std::make_unique<lsd::EchoVisitor>(pSession);
    }

    mInitialized = true;

    LLOG_DBG << "ReaderLSD::init done";
}

bool ReaderLSD::isInitialized() {
    return mInitialized;
}

const char *ReaderLSD::formatName() const{
    return "Lava LSD";
}

bool ReaderLSD::checkExtension(const char *name) {
    if (strcmp(name, ".lsd")) return true;
    return false;
}

void ReaderLSD::getFileExtensions(std::vector<std::string> &extensions) const{
    extensions.insert(extensions.end(), _lsd_extensions.begin(), _lsd_extensions.end());
}

bool ReaderLSD::checkMagicNumber(unsigned magic) {
  return true;
}

bool ReaderLSD::parseStream(std::istream& in) {
    if (!isInitialized()) {
        LLOG_ERR << "Readed not initialized !!!";
        return false;
    }

    mpVisitor->setParserStream(in);

    in.unsetf(std::ios_base::skipws);
    
    std::string str;
    std::string::iterator begin, end;

    bool eof = false;
    while(!eof) {
        if(!std::getline(in, str)) {
            eof = true;
            break;
        }

        begin = str.begin(); end = str.end();
        
        std::vector<lsd::ast::Command> commands; // ast tree
        bool result = x3::phrase_parse(begin, end, lsd::parser::input, lsd::parser::skipper, commands); 

        if (!result) {
            LLOG_ERR << "Parsing LSD scene failed !!!" << std::endl;
            return false;
        }

        if (begin != end) {
            LLOG_DBG << "Remaining unparsed: " << std::string(begin, end);
        }

        for (auto& cmd : commands) {
            if (!mpVisitor->ignoreCommands()) {
                try {
                    boost::apply_visitor(*mpVisitor, cmd);
                } catch (...) {
                    return false;
                }
            }
        }
    }

    return true;
}

// factory methods
std::vector<std::string> *ReaderLSD::myExtensions() {
    return &_lsd_extensions;
}

ReaderBase::SharedPtr ReaderLSD::myConstructor() {
    return ReaderBase::SharedPtr(new ReaderLSD());
}

}  // namespace lava