
// Generated from /home/max/dev/Falcor/src/lava_lib/LSD.g4 by ANTLR 4.8

#pragma once


#include "antlr4-runtime.h"
#include "LSDVisitor.h"


namespace lava {

/**
 * This class provides an empty implementation of LSDVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  LSDBaseVisitor : public LSDVisitor {
public:

  virtual antlrcpp::Any visitFile(LSDParser::FileContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitLine(LSDParser::LineContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitBgeo(LSDParser::BgeoContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitSetenv(LSDParser::SetenvContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitVersion(LSDParser::VersionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitDeclare(LSDParser::DeclareContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitStart(LSDParser::StartContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitEnd(LSDParser::EndContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitDetail(LSDParser::DetailContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitProperty(LSDParser::PropertyContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitImage(LSDParser::ImageContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitGeomerty(LSDParser::GeomertyContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitTime(LSDParser::TimeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitRaytrace(LSDParser::RaytraceContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitQuit(LSDParser::QuitContext *ctx) override {
    return visitChildren(ctx);
  }


};

}  // namespace lava
