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
    auto pSession = std::make_unique<SessionLSD>(std::move(pRendererInterface));

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
/*
bool ReaderLSD::parseStream(std::istream& in) {
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
*/

// performance notes on eagle model
// no storage no preallocation- 1235 milsec
// no storage 1mb string prealloc- 1165 milsec
// no storage 10mb string prealloc- 1192 milsec
// no storage 100mb string prealloc- 1158 milsec

bool readInlineBGEO(std::istream& in) {
    using namespace rapidjson;

    //IStreamWrapper is(in);

    //Document document;
    //document.ParseStream(is);

    auto t1 = std::chrono::high_resolution_clock::now();
    
    uint lines = 0;
    std::string bgeo_str;
    bgeo_str.reserve(104857600); // 100MB

    std::string str;
    str.reserve(10485760); // 10MB

    uint oc = 0; // open brackets count
    uint cc = 0; // closing brackets count
    
    bool bgeo_json_found = false;
    char* char_ptr = nullptr;
    while( std::getline(in, str) ){
        bgeo_str += str;
        lines += 1;
        char_ptr = str.data();
        
        for (uint i = 0; i < str.size(); i++)  {
            if (*(char_ptr) == '[') { oc++; }    
            else if (*(char_ptr) == ']') { cc++; }
            if((oc > 0) && (oc == cc)){
                bgeo_json_found = true;
                break;
            }
        }

        if(bgeo_json_found)
            break;
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
    
    if (bgeo_json_found) {
        LLOG_DBG << "Inline BGEO " << lines << "lines read in: " << duration << " milsec.";
        LLOG_DBG << "Inline BGEO size: " << bgeo_str.size() << " bytes.";

        //t1 = std::chrono::high_resolution_clock::now();
        //Document document;
        //document.Parse(bgeo_str.c_str());
        //t2 = std::chrono::high_resolution_clock::now();
        //duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
        //LLOG_DBG << "Inline BGEO rapidjson document parsed in " << duration << " milsec.";
    }

    return bgeo_json_found;
}

bool readInlineBGEO2(std::istream& in) {
    auto t1 = std::chrono::high_resolution_clock::now();
    
    uint lines = 0;
    std::string bgeo_str;
    bgeo_str.reserve(104857600); // 100MB

    std::string str;
    str.reserve(10485760); // 10MB

    uint oc = 0; // open brackets count
    uint cc = 0; // closing brackets count
    
    bool bgeo_json_found = false;
    char* char_ptr = nullptr;
    while( std::getline(in, str) ){
        bgeo_str += str;
        lines += 1;
        char_ptr = str.data();
        
        for (uint i = 0; i < str.size(); i++)  {
            if (*(char_ptr) == '[') { oc++; }    
            else if (*(char_ptr) == ']') { cc++; }
            if((oc > 0) && (oc == cc)){
                bgeo_json_found = true;
                break;
            }
        }

        if(bgeo_json_found)
            break;
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
    
    if (bgeo_json_found) {
        LLOG_DBG << "Inline BGEO " << lines << "lines read in: " << duration << " milsec.";
        LLOG_DBG << "Inline BGEO string size: " << bgeo_str.size() << " bytes.";

        t1 = std::chrono::high_resolution_clock::now();
        ika::bgeo::Bgeo geom(bgeo_str, false);
        t2 = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
        LLOG_DBG << "BGEO object parsed in: " << duration << " milsecs.";
        geom.printSummary(std::cout);
    }

    return bgeo_json_found;
}


bool ReaderLSD::parseStream(std::istream& in) {
    in.unsetf(std::ios_base::skipws);
    
    std::string str;
    std::string::iterator begin, end;

    while( std::getline(in, str) ){
        begin = str.begin(); end = str.end();
        
        std::vector<lsd::ast::Command> commands; // ast tree
        //bool result = x3::parse(begin, end, lsd::parser::input, commands);
        bool result = x3::phrase_parse(begin, end, lsd::parser::input, lsd::parser::skipper, commands); 

        if (!result) {
            LLOG_ERR << "Parsing LSD scene failed !!!" << std::endl;
            return false;
        }

        if (begin != end) {
            LLOG_DBG << "Remaining unparsed: " << std::string(begin, end);
        }

        for (auto& cmd : commands) {
            if (mpVisitor->current_flag() != lsd::Visitor::Flag::IGNORE_CMDS) {
                boost::apply_visitor(*mpVisitor, cmd);
                if (mpVisitor->current_flag() == lsd::Visitor::Flag::READ_INLINE_GEO) {
                    bool geo_read_result = readInlineBGEO2(in);
                    if (!geo_read_result) {
                        return false;
                    }
                    mpVisitor->inlineGeoReadDone();
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