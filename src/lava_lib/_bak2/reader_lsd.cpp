#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include <string>
#include <iostream>
#include <sstream>

#include <locale>
#include <codecvt>

#include "antlr4-runtime.h"
#include "tree/IterativeParseTreeWalker.h"

#include "readers/generated/LSDLexer.h"
#include "readers/generated/LSDParser.h"
#include "readers/generated/LSDBaseListener.h"
#include "readers/generated/LSDBaseVisitor.h"

//#include "BinaryAntlrInputStream.h"
#include "reader_lsd.h"
#include "input_stream.h"

using namespace antlr4;


namespace lava {

class LSDFileVisitor : public LSDBaseVisitor {
public:
  virtual antlrcpp::Any visitFile(LSDParser::FileContext *ctx) override {
    std::cout << "visitFile " << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitDefaults(LSDParser::DefaultsContext *ctx) override {
    std::cout << "visitDefaults " << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitSetenv(LSDParser::SetenvContext *ctx) override {
    std::cout << "visitSetenv: " << ctx->VAR_NAME()->getText() << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitVersion(LSDParser::VersionContext *ctx) override {
    std::cout << "visitVersion" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitDetail(LSDParser::DetailContext *ctx) override {
    std::cout << "visitDetail" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitImage(LSDParser::ImageContext *ctx) override {
    std::cout << "visitImage" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitDeclare(LSDParser::DeclareContext *ctx) override {
    std::cout << "visitDeclare" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitStart(LSDParser::StartContext *ctx) override {
    std::cout << "visitStart" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitEnd(LSDParser::EndContext *ctx) override {
    std::cout << "visitEnd" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitBgeo(LSDParser::BgeoContext *ctx) override {
    std::cout << "visitBgeo:" << ctx->getText() << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitTime(LSDParser::TimeContext *ctx) override {
    std::cout << "visitTime" << std::endl;
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitLine(LSDParser::LineContext *ctx) override {
    std::cout << "visitLine" << std::endl;
    return visitChildren(ctx);
  }
};


class  LSDFileListener : public LSDListener {
public:

  virtual void enterFile(LSDParser::FileContext * /*ctx*/) override { std::cout << "file enter:\n"; }
  virtual void exitFile(LSDParser::FileContext * /*ctx*/) override { std::cout << "file exit:\n"; }

  virtual void enterLine(LSDParser::LineContext * /*ctx*/) override { std::cout << "line enter:\n"; }
  virtual void exitLine(LSDParser::LineContext * /*ctx*/) override { std::cout << "line exit:\n"; }

  virtual void enterBgeo(LSDParser::BgeoContext * /*ctx*/) override { }
  virtual void exitBgeo(LSDParser::BgeoContext * /*ctx*/) override { }

  virtual void enterSetenv(LSDParser::SetenvContext * /*ctx*/) override { }
  virtual void exitSetenv(LSDParser::SetenvContext * /*ctx*/) override { }

  virtual void enterVersion(LSDParser::VersionContext * /*ctx*/) override { std::cout << "version enter:\n"; }
  virtual void exitVersion(LSDParser::VersionContext * /*ctx*/) override { std::cout << "version exit:\n"; }

  virtual void enterDefaults(LSDParser::DefaultsContext * /*ctx*/) override { }
  virtual void exitDefaults(LSDParser::DefaultsContext * /*ctx*/) override { }

  virtual void enterDeclare(LSDParser::DeclareContext * /*ctx*/) override { }
  virtual void exitDeclare(LSDParser::DeclareContext * /*ctx*/) override { }

  virtual void enterStart(LSDParser::StartContext * /*ctx*/) override { }
  virtual void exitStart(LSDParser::StartContext * /*ctx*/) override { }

  virtual void enterEnd(LSDParser::EndContext * /*ctx*/) override { }
  virtual void exitEnd(LSDParser::EndContext * /*ctx*/) override { }

  virtual void enterDetail(LSDParser::DetailContext * /*ctx*/) override { }
  virtual void exitDetail(LSDParser::DetailContext * /*ctx*/) override { }

  virtual void enterProperty(LSDParser::PropertyContext * /*ctx*/) override { }
  virtual void exitProperty(LSDParser::PropertyContext * /*ctx*/) override { }

  virtual void enterImage(LSDParser::ImageContext * /*ctx*/) override { }
  virtual void exitImage(LSDParser::ImageContext * /*ctx*/) override { }

  virtual void enterGeomerty(LSDParser::GeomertyContext * /*ctx*/) override { }
  virtual void exitGeomerty(LSDParser::GeomertyContext * /*ctx*/) override { }

  virtual void enterTime(LSDParser::TimeContext * /*ctx*/) override { }
  virtual void exitTime(LSDParser::TimeContext * /*ctx*/) override { }

  virtual void enterRaytrace(LSDParser::RaytraceContext * /*ctx*/) override { }
  virtual void exitRaytrace(LSDParser::RaytraceContext * /*ctx*/) override { }

  virtual void enterQuit(LSDParser::QuitContext * /*ctx*/) override { std::cout << "quit enter:\n"; }
  virtual void exitQuit(LSDParser::QuitContext * /*ctx*/) override { std::cout << "quit enter:\n"; }


  virtual void enterEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void exitEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void visitTerminal(antlr4::tree::TerminalNode * /*node*/) override { }
  virtual void visitErrorNode(antlr4::tree::ErrorNode * /*node*/) override { }

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
    extensions.insert(extensions.end(), _lsd_extensions.begin(), _lsd_extensions.end());
}


bool ReaderLSD::checkMagicNumber(unsigned magic) {
  return true;
}

class adapting_wistreambuf : public std::wstreambuf {
  std::streambuf *parent;
public:
  adapting_wistreambuf(std::streambuf *parent_) : parent(parent_) { }
  int_type underflow() {
    return (int_type)parent->snextc();
  }
};

class MyUnbufferedCharStream: public antlr4::UnbufferedCharStream {
  public:
    MyUnbufferedCharStream(std::wistream &input): antlr4::UnbufferedCharStream(input) {}

    virtual std::string toString() const override{
      return antlrcpp::utf32_to_utf8(_data);
    }
};

bool ReaderLSD::read(SharedPtr iface, std::istream& in, bool ate_magic) {
  antlr4::ANTLRInputStream input(in);
  
  LSDLexer lexer(&input);
  antlr4::UnbufferedTokenStream tokens(&lexer);
  LSDParser parser(&tokens);
  //parser.setBuildParseTree(false);

  LSDParser::FileContext* tree = parser.file();
  
  LSDFileListener listener;  
  antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);

  //LSDFileVisitor visitor;
  //visitor.visitFile(tree);

  return true;
}

bool ReaderLSD::read(SharedPtr iface, std::wistream& in, bool ate_magic) {
  InputStream input(in);

  LSDLexer lexer(&input);
  antlr4::UnbufferedTokenStream tokens(&lexer);
  //antlr4::CommonTokenStream tokens(&lexer);
  LSDParser parser(&tokens);
  parser.setBuildParseTree(false);

  //LSDParser::FileContext* tree = parser.file();
  //LSDParser::LineContext* line = parser.line();
  
  LSDFileListener listener;  
  parser.addParseListener(&listener);
  
  parser.top_level_rule();

  //antlr4::tree::IterativeParseTreeWalker::DEFAULT.walk(&listener, tree);
  //antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, line);
  
  //LSDFileVisitor visitor;
  //visitor.visitFile(tree);
  
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