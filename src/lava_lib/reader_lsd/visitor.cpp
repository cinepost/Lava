#include <chrono>
#include <exception>

#include "visitor.h"
#include "session.h"
#include "uudecode.h"

#include "properties_container.h"

namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;

namespace lava { 

namespace lsd {

bool readEmbeddedFileUU(std::istream* pParserStream, size_t size, std::vector<unsigned char>& decoded_data) {
    LLOG_DBG << "Reading " << size << " bytes of embedded data";

    bool result = true;
    std::istream &in = *pParserStream;

    std::vector<char> buff(size);
 
    // read size amount of bytes from stream into buff
    in.unsetf(std::ios::skipws);
    in.read((char *)buff.data(), size);
    in.setf(std::ios::skipws);

    // decode data
    FILE* inMemFile = fmemopen((void *)buff.data(), size, "rw");
    
    FILE* outTestFile = fopen("/home/max/Desktop/mistery_file_decoded", "w");

    if(!uu::decodeUU(inMemFile, outTestFile)) {
        LLOG_DBG << "Error decoding embedded data !!!";
    }

    fclose(inMemFile);
    
    if(outTestFile)
        fclose(outTestFile);
    // test write
    //std::ofstream fout("/home/max/Desktop/mistery_file", std::ios::out | std::ios::binary);
    //fout.write((char*)&buff[0], buff.size() * sizeof(unsigned char));
    //fout.close();

    return result;
}

bool readInlineBGEO(std::istream* pParserStream, ika::bgeo::Bgeo::SharedPtr pBgeo) {
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
    pBgeo->readInlineGeo(bgeo_str, false);
    t2 = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
    LLOG_DBG << "BGEO object parsed in: " << duration << " milsecs.";
    
    return true;
}

Visitor::Visitor(std::unique_ptr<Session>& pSession): mpSession(std::move(pSession)), mpParserStream(nullptr), mIgnoreCommands(false), mQuit(false) { } 

void Visitor::setParserStream(std::istream& in) {
    if (!mpParserStream) {
        mpParserStream = &in;
    }
}

bool Visitor::failed() const { return mpSession ? mpSession->failed() : false; };

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
    mpSession->cmdSetEnv(c.key, c.value);
};

void Visitor::operator()(ast::cmd_image const& c) const {
    mpSession->cmdImage(c.display_type, c.filename);
}

void Visitor::operator()(ast::cmd_quit const& c) const { 
    //mpSession->cmdQuit();
    //mQuit = true;
    //printf("mQuit set to true\n");
}

void Visitor::operator()(ast::cmd_start const& c) const {
    if(!mpSession->cmdStart(c.object_type))
        throw std::runtime_error("Error starting new scope !!!");
}

void Visitor::operator()(ast::cmd_end const& c) const { 
    if(!mpSession->cmdEnd()) {
        LLOG_ERR << "Error ending current scope !!!";
    }
}

void Visitor::operator()(ast::cmd_edge const& c) const { 
    mpSession->cmdEdge(c.src_node_uuid, c.src_node_output_socket, c.dst_node_uuid, c.dst_node_input_socket);
}

void Visitor::operator()(ast::cmd_socket const& c) const { 
    if(!mpSession->cmdSocket(c.direction, c.data_type, c.name))
        throw std::runtime_error("Error adding node socket !!!");
}

void Visitor::operator()(ast::cmd_time const& c) const {
    mpSession->cmdTime(c.time);
}

void Visitor::operator()(ast::cmd_detail const& c) {
    auto pGeo = mpSession->getCurrentGeo();

    if(!pGeo) {
        LLOG_ERR << "Unable to process cmd_detail out of Geo scope !!!";
        throw std::runtime_error("Unable to process cmd_detail out of Geo scope !!!");
    }

    pGeo->setDetailFilePath(mpSession->getExpandedString(c.filename));
    pGeo->setDetailName(c.name);
    pGeo->setTemporary(c.temporary);

    if(c.filename == "stdin") {
        ika::bgeo::Bgeo::SharedPtr pBgeo = pGeo->bgeo();
        bool result = readInlineBGEO(mpParserStream, pBgeo);
        if (!result) {
            LLOG_ERR << "Error reading inline bgeo !!!";
            return;
        }
        pBgeo->preCachePrimitives();
    }
}

void Visitor::operator()(ast::cmd_version const& c) const {

}

void Visitor::operator()(ast::cmd_config const& c) const {
    mpSession->cmdConfig(c.prop_type, c.prop_name, c.prop_value);
}

void Visitor::operator()(ast::cmd_defaults const& c) const {

}

void Visitor::operator()(ast::cmd_transform const& c) const {
    mpSession->cmdTransform(c.m);
}

void Visitor::operator()(ast::cmd_iprmode const& c) const {
    mpSession->cmdIPRmode(c.mode, c.stash);
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

void Visitor::operator()(ast::ray_embeddedfile const& c) const {
    auto pScope = mpSession->getCurrentScope();
    if(!pScope)
        return;

    if(c.encoding == ast::EmbedDataEncoding::UUENCODED) {
        if(readEmbeddedFileUU(mpParserStream, c.size, pScope->getEmbeddedData(c.name))) {
            LLOG_DBG << "Read embedded data size: " << pScope->getEmbeddedData(c.name).size();
        }
    } else {
        LLOG_WRN << "Unknown embedded data encoding !!!";
        return;
    }
}

}  // namespace lsd

}  // namespace lava
