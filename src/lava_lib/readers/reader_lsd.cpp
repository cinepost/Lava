#include <vector>
#include <fstream>
#include <iterator>

#include "reader_lsd.h"
#include "grammar_lsd.h"

#include "lava_utils_lib/logging.h"

namespace x3 = boost::spirit::x3;

namespace lava {

ReaderLSD::ReaderLSD(): ReaderBase() {
    //commands<std::string::const_iterator>();
}

ReaderLSD::~ReaderLSD() { }

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

bool ReaderLSD::parseLine(const std::string& line, bool echo, std::string& unparsed) {
    unparsed = "";
    auto iter = line.begin(), end = line.end();
    
    std::vector<lsd::ast::Command> commands; // ast tree
    bool result = x3::parse(iter, end, lsd::parser::input, commands);

    if (!result) {
        LOG_ERR << "Parsing LSD scene failed !!!" << std::endl;
        return false;
    }

    if (iter != end) {
        unparsed = std::string(iter, end);
        LOG_DBG << "Parsed: " << (100.0 * std::distance(line.begin(), iter) / line.size()) << "%\n";
        LOG_DBG << "Remaining unparsed: " << unparsed << "\n";
        return true;
    }

    for (auto& cmd : commands) {
        boost::apply_visitor(lsd::EchoVisitor(), cmd);
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