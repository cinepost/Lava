
// Generated from /home/max/dev/Falcor/src/lava_lib/LSD.g4 by ANTLR 4.8

#pragma once


#include "antlr4-runtime.h"
#include "LSDListener.h"


namespace lava {

/**
 * This class provides an empty implementation of LSDListener,
 * which can be extended to create a listener which only needs to handle a subset
 * of the available methods.
 */
class  LSDBaseListener : public LSDListener {
public:

  virtual void enterFile(LSDParser::FileContext * /*ctx*/) override { }
  virtual void exitFile(LSDParser::FileContext * /*ctx*/) override { }

  virtual void enterLine(LSDParser::LineContext * /*ctx*/) override { }
  virtual void exitLine(LSDParser::LineContext * /*ctx*/) override { }

  virtual void enterBgeo(LSDParser::BgeoContext * /*ctx*/) override { }
  virtual void exitBgeo(LSDParser::BgeoContext * /*ctx*/) override { }

  virtual void enterSetenv(LSDParser::SetenvContext * /*ctx*/) override { }
  virtual void exitSetenv(LSDParser::SetenvContext * /*ctx*/) override { }

  virtual void enterVersion(LSDParser::VersionContext * /*ctx*/) override { }
  virtual void exitVersion(LSDParser::VersionContext * /*ctx*/) override { }

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

  virtual void enterQuit(LSDParser::QuitContext * /*ctx*/) override { }
  virtual void exitQuit(LSDParser::QuitContext * /*ctx*/) override { }


  virtual void enterEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void exitEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void visitTerminal(antlr4::tree::TerminalNode * /*node*/) override { }
  virtual void visitErrorNode(antlr4::tree::ErrorNode * /*node*/) override { }

};

}  // namespace lava
