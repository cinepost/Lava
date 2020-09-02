
// Generated from /home/max/dev/Falcor/src/lava_lib/LSD.g4 by ANTLR 4.8

#pragma once


#include "antlr4-runtime.h"
#include "LSDParser.h"


namespace lava {

/**
 * This class defines an abstract visitor for a parse tree
 * produced by LSDParser.
 */
class  LSDVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by LSDParser.
   */
    virtual antlrcpp::Any visitFile(LSDParser::FileContext *context) = 0;

    virtual antlrcpp::Any visitLine(LSDParser::LineContext *context) = 0;

    virtual antlrcpp::Any visitBgeo(LSDParser::BgeoContext *context) = 0;

    virtual antlrcpp::Any visitSetenv(LSDParser::SetenvContext *context) = 0;

    virtual antlrcpp::Any visitVersion(LSDParser::VersionContext *context) = 0;

    virtual antlrcpp::Any visitDeclare(LSDParser::DeclareContext *context) = 0;

    virtual antlrcpp::Any visitStart(LSDParser::StartContext *context) = 0;

    virtual antlrcpp::Any visitEnd(LSDParser::EndContext *context) = 0;

    virtual antlrcpp::Any visitDetail(LSDParser::DetailContext *context) = 0;

    virtual antlrcpp::Any visitProperty(LSDParser::PropertyContext *context) = 0;

    virtual antlrcpp::Any visitImage(LSDParser::ImageContext *context) = 0;

    virtual antlrcpp::Any visitGeomerty(LSDParser::GeomertyContext *context) = 0;

    virtual antlrcpp::Any visitTime(LSDParser::TimeContext *context) = 0;

    virtual antlrcpp::Any visitRaytrace(LSDParser::RaytraceContext *context) = 0;

    virtual antlrcpp::Any visitQuit(LSDParser::QuitContext *context) = 0;


};

}  // namespace lava
