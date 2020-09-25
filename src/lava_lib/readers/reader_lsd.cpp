#include <vector>
#include <fstream>
#include <iterator>
#include <regex>

#include <boost/spirit/include/support_istream_iterator.hpp>

#include "reader_lsd.h"
#include "grammar_lsd.h"

#include "lava_utils_lib/logging.h"

namespace x3 = boost::spirit::x3;

namespace lava {


ReaderLSD::ReaderLSD(): ReaderBase() { }

ReaderLSD::~ReaderLSD() { }

void ReaderLSD::init(std::unique_ptr<RendererIfaceBase> pInterface, bool echo) {
    ReaderBase::init(std::move(pInterface), echo);

    std::unique_ptr<RendererIfaceLSD> pIface = std::unique_ptr<RendererIfaceLSD>{static_cast<RendererIfaceLSD*>(mpInterface.release())};

    if (!echo) {
        // standard LSD visitor
        mpVisitor = std::make_unique<lsd::Visitor>(pIface);
    } else {
        // LSD visitor with parsed console echo (for debug purposes)
        mpVisitor = std::make_unique<lsd::EchoVisitor>(pIface);
    }

    LLOG_DBG << "ReaderLSD::init done";
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
    //std::regex re("setenv|cmd_version|cmd_start|cmd_");
    
    //while(in >> word) { //take word and print
    //  std::cout << word;
    //}

    in.unsetf(std::ios_base::skipws);
    boost::spirit::istream_iterator iter(in), end;

    std::vector<lsd::ast::Command> commands; // ast tree
    bool result = x3::parse(iter, end, lsd::parser::input, commands); 

    if (!result) {
        LLOG_ERR << "Parsing LSD scene failed !!!" << std::endl;
        return false;
    }

    if (iter != end) {
        //unparsed = std::string(iter, end);
        LLOG_DBG << "Remaining unparsed: " << std::string(iter, end);
        return true;
    }

    for (auto& cmd : commands) {
        boost::apply_visitor(*mpVisitor, cmd);
    }

    return true;
}

bool ReaderLSD::parseLine(const std::string& line, std::string& unparsed) {
    unparsed = "";
    auto iter = line.begin(), end = line.end();
    
    std::vector<lsd::ast::Command> commands; // ast tree
    bool result = x3::parse(iter, end, lsd::parser::input, commands);

    if (!result) {
        LLOG_ERR << "Parsing LSD scene failed !!!" << std::endl;
        return false;
    }

    if (iter != end) {
        unparsed = std::string(iter, end);
        //LOG_DBG << "Parsed: " << (100.0 * std::distance(line.begin(), iter) / line.size()) << "%";
        //LOG_DBG << "Remaining unparsed: " << unparsed << "\n";
        return true;
    }

    for (auto& cmd : commands) {
        boost::apply_visitor(*mpVisitor, cmd);
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