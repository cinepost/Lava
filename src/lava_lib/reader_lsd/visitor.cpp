#include <chrono>
#include <exception>

#include "visitor.h"
#include "session.h"

#include "properties_container.h"

namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;

namespace lava { 

namespace lsd {

bool readInlineBGEO(std::istream* pParserStream, ika::bgeo::Bgeo& bgeo) {
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
    std::istream &in = *pParserStream;
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

    if (!bgeo_json_found)
        return false;

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
    
    LLOG_DBG << "Inline BGEO " << lines << "lines read in: " << duration << " milsec.";
    LLOG_DBG << "Inline BGEO string size: " << bgeo_str.size() << " bytes.";

    t1 = std::chrono::high_resolution_clock::now();
    bgeo.readInlineGeo(bgeo_str, false);
    t2 = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
    LLOG_DBG << "BGEO object parsed in: " << duration << " milsecs.";
    
    return true;
}

Visitor::Visitor(std::unique_ptr<Session>& pSession): mpSession(std::move(pSession)), mpParserStream(nullptr), mIgnoreCommands(false) { } 

void Visitor::setParserStream(std::istream& in) {
    if (!mpParserStream) {
        mpParserStream = &in;
    }
}

void Visitor::operator()(ast::ifthen const& c) {
    if (!c.expr) {
        // false expression evaluation, ignore commands until 'endif'
        mIgnoreCommands = true;
    }
}

void Visitor::operator()(ast::endif const& c) {
    if (mIgnoreCommands)
        mIgnoreCommands = false;
}

void Visitor::operator()(ast::setenv const& c) const {
    std::cout << "Visitor setenv\n";
    mpSession->cmdSetEnv(c.key, c.value);
};

void Visitor::operator()(ast::cmd_image const& c) const {
    mpSession->cmdImage(c.display_type, c.filename);
}

void Visitor::operator()(ast::cmd_quit const& c) const { 
    std::cout << "LSDVisitor cmd_quit\n";
}

void Visitor::operator()(ast::cmd_start const& c) const {
    std::cout << "LSDVisitor cmd_start\n";
    if(!mpSession->cmdStart(c.object_type))
        throw std::runtime_error("Error starting new scope !!!");
}

void Visitor::operator()(ast::cmd_end const& c) const { 
    std::cout << "LSDVisitor cmd_end\n";
    if(!mpSession->cmdEnd())
        throw std::runtime_error("Error ending current scope !!!");
}

void Visitor::operator()(ast::cmd_time const& c) const {
    mpSession->cmdTime(c.time);
}

void Visitor::operator()(ast::cmd_detail const& c) {
    auto pGeo = mpSession->getCurrentGeo();
    pGeo->setDetailFilename(c.filename);
    pGeo->setDetailName(c.name);
    
    if(!pGeo) {
        LLOG_ERR << "Unable to process cmd_detail out of Geo scope !!!";
        throw std::runtime_error("Unable to process cmd_detail out of Geo scope !!!");
    }

    if(c.filename == "stdin") {
        auto& bgeo = pGeo->bgeo();

        bool result = readInlineBGEO(mpParserStream, bgeo);
        if (!result) {
            LLOG_ERR << "Error reading inline bgeo !!!";
            return;
        }
        bgeo.preCachePrimitives();
        //mpSession->pushBgeo(c.name, bgeo);
    } else {
        // ika::bgeo::Bgeo bgeo(mpSession->getExpandedString(c.filename), false); // FIXME: don't check version for now
        //mpSession->pushBgeo(c.name, c.filename);
    }
}

void Visitor::operator()(ast::cmd_version const& c) const {

}

void Visitor::operator()(ast::cmd_config const& c) const {
    mpSession->cmdConfig(c.filename);
}

void Visitor::operator()(ast::cmd_defaults const& c) const {

}

void Visitor::operator()(ast::cmd_transform const& c) const {
    mpSession->cmdTransform(c.m);
}

void Visitor::operator()(ast::cmd_mtransform const& c) const {
    mpSession->cmdMTransform(c.m);
}

void Visitor::operator()(ast::cmd_geometry const& c) const {
    mpSession->cmdGeometry(c.geometry_name);
}

void Visitor::operator()(ast::cmd_deviceoption const& c) const {

}

void Visitor::operator()(ast::cmd_property const& c) const {
    if (c.values.size() == 0)
        return;

    if (c.values.size() != 1) {
        std::vector<std::pair<std::string, Property::Value>> v;
        for(auto const& value: c.values) {
            v.push_back({value.first, value.second.get()});
        }
        mpSession->cmdPropertyV(c.style, v);
        return;
    }
   
   auto const& value = c.values[0];
   mpSession->cmdProperty(c.style, value.first, value.second.get());
}

void Visitor::operator()(ast::cmd_declare const& c) const {
    if(!c.values.size())
        return;

    if (c.values.size() > 1) {
        LLOG_WRN << "Value arrays not supported !!! Ignored for token: " << c.token;
        return;
    }

    mpSession->cmdDeclare(c.style, c.type, c.token, c.values[0]);
}

void Visitor::operator()(ast::cmd_raytrace const& c) const {
    if(!mpSession->cmdRaytrace())
        throw std::runtime_error("Error rendering image !!!");
}

void Visitor::operator()(ast::cmd_reset const& c) const {

}

}  // namespace lsd

}  // namespace lava
