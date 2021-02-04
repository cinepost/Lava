
// Generated from /home/max/dev/Falcor/src/lava_lib/LSD.g4 by ANTLR 4.8

#pragma once


#include "antlr4-runtime.h"
#include "LSDParser.h"


namespace lava {

/**
 * This interface defines an abstract listener for a parse tree produced by LSDParser.
 */
class  LSDListener : public antlr4::tree::ParseTreeListener {
public:

  virtual void enterFile(LSDParser::FileContext *ctx) = 0;
  virtual void exitFile(LSDParser::FileContext *ctx) = 0;

  virtual void enterLine(LSDParser::LineContext *ctx) = 0;
  virtual void exitLine(LSDParser::LineContext *ctx) = 0;

  virtual void enterBgeo(LSDParser::BgeoContext *ctx) = 0;
  virtual void exitBgeo(LSDParser::BgeoContext *ctx) = 0;

  virtual void enterSetenv(LSDParser::SetenvContext *ctx) = 0;
  virtual void exitSetenv(LSDParser::SetenvContext *ctx) = 0;

  virtual void enterVersion(LSDParser::VersionContext *ctx) = 0;
  virtual void exitVersion(LSDParser::VersionContext *ctx) = 0;

  virtual void enterDefaults(LSDParser::DefaultsContext *ctx) = 0;
  virtual void exitDefaults(LSDParser::DefaultsContext *ctx) = 0;

  virtual void enterDeclare(LSDParser::DeclareContext *ctx) = 0;
  virtual void exitDeclare(LSDParser::DeclareContext *ctx) = 0;

  virtual void enterStart(LSDParser::StartContext *ctx) = 0;
  virtual void exitStart(LSDParser::StartContext *ctx) = 0;

  virtual void enterEnd(LSDParser::EndContext *ctx) = 0;
  virtual void exitEnd(LSDParser::EndContext *ctx) = 0;

  virtual void enterDetail(LSDParser::DetailContext *ctx) = 0;
  virtual void exitDetail(LSDParser::DetailContext *ctx) = 0;

  virtual void enterProperty(LSDParser::PropertyContext *ctx) = 0;
  virtual void exitProperty(LSDParser::PropertyContext *ctx) = 0;

  virtual void enterImage(LSDParser::ImageContext *ctx) = 0;
  virtual void exitImage(LSDParser::ImageContext *ctx) = 0;

  virtual void enterGeomerty(LSDParser::GeomertyContext *ctx) = 0;
  virtual void exitGeomerty(LSDParser::GeomertyContext *ctx) = 0;

  virtual void enterTime(LSDParser::TimeContext *ctx) = 0;
  virtual void exitTime(LSDParser::TimeContext *ctx) = 0;

  virtual void enterRaytrace(LSDParser::RaytraceContext *ctx) = 0;
  virtual void exitRaytrace(LSDParser::RaytraceContext *ctx) = 0;

  virtual void enterQuit(LSDParser::QuitContext *ctx) = 0;
  virtual void exitQuit(LSDParser::QuitContext *ctx) = 0;


};

}  // namespace lava
