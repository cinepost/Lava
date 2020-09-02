#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include <string>
#include <iostream>
#include <sstream>

#include <locale>
#include <codecvt>

#include "antlr4-runtime.h"
#include "readers/generated/LSDLexer.h"
#include "readers/generated/LSDParser.h"
#include "readers/generated/LSDBaseListener.h"
#include "readers/generated/LSDBaseVisitor.h"

#include "BinaryAntlrInputStream.h"
#include "reader_lsd.h"

using namespace shout;
using namespace antlr4;


namespace lava {

class LSDFileVisitor : public LSDBaseVisitor {
public:
  virtual antlrcpp::Any visitSetenv(IFDParser::SetenvContext *ctx) override {
    std::cout << "visitSetenv: " << ctx->VAR_NAME()->getText() << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitVersion(IFDParser::VersionContext *ctx) override {
    std::cout << "visitVersion" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitDetail(IFDParser::DetailContext *ctx) override {
    std::cout << "visitDetail" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitImage(IFDParser::ImageContext *ctx) override {
    std::cout << "visitImage" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitDeclare(IFDParser::DeclareContext *ctx) override {
    std::cout << "visitDeclare" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitStart(IFDParser::StartContext *ctx) override {
    std::cout << "visitStart" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitEnd(IFDParser::EndContext *ctx) override {
    std::cout << "visitEnd" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitBgeo(IFDParser::BgeoContext *ctx) override {
    std::cout << "visitBgeo:" << ctx->getText() << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitTime(IFDParser::TimeContext *ctx) override {
    std::cout << "visitTime" << std::endl;
    return visitChildren(ctx);
  }
};

ReaderLSD::ReaderLSD() {
  BOOST_LOG_TRIVIAL(debug) << "LSD file reader constructed!";
}


ReaderLSD::~ReaderLSD() {
  BOOST_LOG_TRIVIAL(debug) << "LSD file reader destructed!";
}


const char *ReaderLSD::formatName() const{
    return "Lava LSD";
}


bool ReaderLSD::checkExtension(const char *name) {
    if (strcmp(name, ".lsd")) return true;
    return false;
}


void ReaderLSD::getFileExtensions(std::vector<std::string> &extensions) const{
    extensions.insert(extensions.end(), _ifd_extensions.begin(), _ifd_extensions.end());
}


int ReaderLSD::checkMagicNumber(unsigned magic) {
    return 0;
}


SCN_Scene::IOStatus ReaderLSD::fileLoad(SCN_Scene &scn, std::istream &in, bool ate_magic) {
  //BinaryANTLRInputStream input(in);
  antlr4::ANTLRInputStream input(in);
  std::cout << "stream yes\n";

  IFDLexer lexer(&input);
  std::cout << "lexer yes\n";

  antlr4::CommonTokenStream tokens(&lexer);

  IFDParser parser(&tokens);

  std::cout << "ReaderLSD::fileLoad.. parser.file()" << std::endl;
  //antlr4::tree::ParseTree* tree  = parser.file();
  IFDParser::FileContext* tree = parser.file();

  //IFDFileListener listener;
  //std::cout << "ReaderLSD::fileLoad.. DEFAULT.walk()" << std::endl;
  //antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);

  IFDFileVisitor visitor;
  std::cout << "ReaderLSD::fileLoad.. visitor.visitFile(tree)" << std::endl;
  visitor.visitFile(tree);

  return true;
}


// factory methods

std::vector<std::string> *ReaderLSD::myExtensions() {
    return &_ifd_extensions;
}



ReaderBase::SharedPtr *ReaderLSD::myConstructor() {
    return ReaderBase::SharedPtr(new ReaderLSD());
}
}  // namespace lava