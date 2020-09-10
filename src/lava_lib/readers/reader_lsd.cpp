#include <vector>
#include <fstream>

#include "reader_lsd.h"
#include "grammar_lsd.h"

#include "lava_utils_lib/logging.h"

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

void ReaderLSD::parseLine(const std::string& line, bool echo) {
    typedef std::string::const_iterator It;

	It beg = line.begin(), end = line.end();
    namespace x3 = boost::spirit::x3;
    
    std::vector<lsd::ast::Command> commands;
    bool result = x3::parse(beg, end, lsd::parser::input, commands);

    if (beg != end) {
        std::cout << "Remaining unparsed: " << std::string(beg, end) << "\n";
    }

    auto parsed_percent = 100.0 * std::distance(line.begin(), beg) / line.size();
    if (!result || (parsed_percent < 100.0)) {
        std::cout << "Parsed: " << parsed_percent << "%\n";
        //std::cout << "ok = " << result << std::endl;
    }

    for (auto& cmd : commands) {
        boost::apply_visitor(lsd::EchoVisitor(), cmd);
    }
}

// factory methods
std::vector<std::string> *ReaderLSD::myExtensions() {
    return &_lsd_extensions;
}

ReaderBase::SharedPtr ReaderLSD::myConstructor() {
    return ReaderBase::SharedPtr(new ReaderLSD());
}

}  // namespace lava