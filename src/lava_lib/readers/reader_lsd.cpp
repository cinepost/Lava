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

/*
struct LSDEcho : public boost::static_visitor<> {
    void operator()(Command a) const {
        printf("Command: %d, Month: %d, Day: %d\n", a.year, a.month, a.day);
    }
    void operator()(Params a) const {
        //printf("Hour: %d, Minute: %d, Second: %d\n", a.hour, a.minute, a.second);
    }
};
*/


void ReaderLSD::parseLine(const std::string& line, bool echo) {
    typedef std::string::const_iterator It;

	It beg = line.begin(), end = line.end();

    //command_grammar<It> const commands;

	//Ast::Commands parsed;
    namespace x3 = boost::spirit::x3;
    
    //bool result = qi::parse(beg, end, commands, parsed);

    std::vector<lsd::ast::commands> commands;
    bool result = x3::parse(beg, end, lsd::parser::input, commands);

    /*
    if (result) {
        for (auto& cmd : parsed) {
            std::cout << "Parsed\n";
            //std::cout << "Parsed " << cmd << "\n";
        }
    } else {
            std::cout << "Parse failed\n";
    }
    */

    if (beg != end) {
        std::cout << "Remaining unparsed: " << std::string(beg, end) << "'\n";
    }

    std::cout << "Parsed: " << (100.0 * std::distance(line.begin(), beg) / line.size()) << "%\n";
    std::cout << "ok = " << result << std::endl;

    //for (auto& cmd : commands) {
    //    boost::apply_visitor([](auto& v) { std::cout << boost::fusion::as_deque(v) << "\n"; }, cmd);
    //}

    //if (beg != end)
    //    LOG_ERR << "Parse failed !!!";
    //else
    //    boost::apply_visitor(LSDEcho(), ret);
}

// factory methods
std::vector<std::string> *ReaderLSD::myExtensions() {
    return &_lsd_extensions;
}

ReaderBase::SharedPtr ReaderLSD::myConstructor() {
    return ReaderBase::SharedPtr(new ReaderLSD());
}

}  // namespace lava