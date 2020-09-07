
// Generated from /home/max/dev/Falcor/src/lava_lib/LSD.g4 by ANTLR 4.8


#include "LSDListener.h"
#include "LSDVisitor.h"

#include "LSDParser.h"


using namespace antlrcpp;
using namespace lava;
using namespace antlr4;

LSDParser::LSDParser(TokenStream *input) : Parser(input) {
  _interpreter = new atn::ParserATNSimulator(this, _atn, _decisionToDFA, _sharedContextCache);
}

LSDParser::~LSDParser() {
  delete _interpreter;
}

std::string LSDParser::getGrammarFileName() const {
  return "LSD.g4";
}

const std::vector<std::string>& LSDParser::getRuleNames() const {
  return _ruleNames;
}

dfa::Vocabulary& LSDParser::getVocabulary() const {
  return _vocabulary;
}


//----------------- FileContext ------------------------------------------------------------------

LSDParser::FileContext::FileContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<LSDParser::LineContext *> LSDParser::FileContext::line() {
  return getRuleContexts<LSDParser::LineContext>();
}

LSDParser::LineContext* LSDParser::FileContext::line(size_t i) {
  return getRuleContext<LSDParser::LineContext>(i);
}


size_t LSDParser::FileContext::getRuleIndex() const {
  return LSDParser::RuleFile;
}

void LSDParser::FileContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterFile(this);
}

void LSDParser::FileContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitFile(this);
}


antlrcpp::Any LSDParser::FileContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitFile(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::FileContext* LSDParser::file() {
  FileContext *_localctx = _tracker.createInstance<FileContext>(_ctx, getState());
  enterRule(_localctx, 0, LSDParser::RuleFile);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(35);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & ((1ULL << LSDParser::T__0)
      | (1ULL << LSDParser::T__1)
      | (1ULL << LSDParser::T__3)
      | (1ULL << LSDParser::T__4)
      | (1ULL << LSDParser::T__5)
      | (1ULL << LSDParser::T__7)
      | (1ULL << LSDParser::T__8)
      | (1ULL << LSDParser::T__13)
      | (1ULL << LSDParser::T__14)
      | (1ULL << LSDParser::T__15)
      | (1ULL << LSDParser::T__16)
      | (1ULL << LSDParser::T__17)
      | (1ULL << LSDParser::T__18)
      | (1ULL << LSDParser::COMMENT)
      | (1ULL << LSDParser::BGEO_START))) != 0)) {
      setState(32);
      line();
      setState(37);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- LineContext ------------------------------------------------------------------

LSDParser::LineContext::LineContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

LSDParser::VersionContext* LSDParser::LineContext::version() {
  return getRuleContext<LSDParser::VersionContext>(0);
}

LSDParser::DeclareContext* LSDParser::LineContext::declare() {
  return getRuleContext<LSDParser::DeclareContext>(0);
}

LSDParser::SetenvContext* LSDParser::LineContext::setenv() {
  return getRuleContext<LSDParser::SetenvContext>(0);
}

LSDParser::StartContext* LSDParser::LineContext::start() {
  return getRuleContext<LSDParser::StartContext>(0);
}

LSDParser::EndContext* LSDParser::LineContext::end() {
  return getRuleContext<LSDParser::EndContext>(0);
}

LSDParser::PropertyContext* LSDParser::LineContext::property() {
  return getRuleContext<LSDParser::PropertyContext>(0);
}

LSDParser::DetailContext* LSDParser::LineContext::detail() {
  return getRuleContext<LSDParser::DetailContext>(0);
}

LSDParser::ImageContext* LSDParser::LineContext::image() {
  return getRuleContext<LSDParser::ImageContext>(0);
}

LSDParser::GeomertyContext* LSDParser::LineContext::geomerty() {
  return getRuleContext<LSDParser::GeomertyContext>(0);
}

LSDParser::TimeContext* LSDParser::LineContext::time() {
  return getRuleContext<LSDParser::TimeContext>(0);
}

LSDParser::BgeoContext* LSDParser::LineContext::bgeo() {
  return getRuleContext<LSDParser::BgeoContext>(0);
}

LSDParser::RaytraceContext* LSDParser::LineContext::raytrace() {
  return getRuleContext<LSDParser::RaytraceContext>(0);
}

LSDParser::QuitContext* LSDParser::LineContext::quit() {
  return getRuleContext<LSDParser::QuitContext>(0);
}

LSDParser::DefaultsContext* LSDParser::LineContext::defaults() {
  return getRuleContext<LSDParser::DefaultsContext>(0);
}

tree::TerminalNode* LSDParser::LineContext::COMMENT() {
  return getToken(LSDParser::COMMENT, 0);
}


size_t LSDParser::LineContext::getRuleIndex() const {
  return LSDParser::RuleLine;
}

void LSDParser::LineContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterLine(this);
}

void LSDParser::LineContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitLine(this);
}


antlrcpp::Any LSDParser::LineContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitLine(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::LineContext* LSDParser::line() {
  LineContext *_localctx = _tracker.createInstance<LineContext>(_ctx, getState());
  enterRule(_localctx, 2, LSDParser::RuleLine);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    setState(53);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case LSDParser::T__3: {
        enterOuterAlt(_localctx, 1);
        setState(38);
        version();
        break;
      }

      case LSDParser::T__5: {
        enterOuterAlt(_localctx, 2);
        setState(39);
        declare();
        break;
      }

      case LSDParser::T__1: {
        enterOuterAlt(_localctx, 3);
        setState(40);
        setenv();
        break;
      }

      case LSDParser::T__7: {
        enterOuterAlt(_localctx, 4);
        setState(41);
        start();
        break;
      }

      case LSDParser::T__0: {
        enterOuterAlt(_localctx, 5);
        setState(42);
        end();
        break;
      }

      case LSDParser::T__13: {
        enterOuterAlt(_localctx, 6);
        setState(43);
        property();
        break;
      }

      case LSDParser::T__8: {
        enterOuterAlt(_localctx, 7);
        setState(44);
        detail();
        break;
      }

      case LSDParser::T__14: {
        enterOuterAlt(_localctx, 8);
        setState(45);
        image();
        break;
      }

      case LSDParser::T__15: {
        enterOuterAlt(_localctx, 9);
        setState(46);
        geomerty();
        break;
      }

      case LSDParser::T__16: {
        enterOuterAlt(_localctx, 10);
        setState(47);
        time();
        break;
      }

      case LSDParser::BGEO_START: {
        enterOuterAlt(_localctx, 11);
        setState(48);
        bgeo();
        break;
      }

      case LSDParser::T__17: {
        enterOuterAlt(_localctx, 12);
        setState(49);
        raytrace();
        break;
      }

      case LSDParser::T__18: {
        enterOuterAlt(_localctx, 13);
        setState(50);
        quit();
        break;
      }

      case LSDParser::T__4: {
        enterOuterAlt(_localctx, 14);
        setState(51);
        defaults();
        break;
      }

      case LSDParser::COMMENT: {
        enterOuterAlt(_localctx, 15);
        setState(52);
        match(LSDParser::COMMENT);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- BgeoContext ------------------------------------------------------------------

LSDParser::BgeoContext::BgeoContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* LSDParser::BgeoContext::BGEO_START() {
  return getToken(LSDParser::BGEO_START, 0);
}


size_t LSDParser::BgeoContext::getRuleIndex() const {
  return LSDParser::RuleBgeo;
}

void LSDParser::BgeoContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterBgeo(this);
}

void LSDParser::BgeoContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitBgeo(this);
}


antlrcpp::Any LSDParser::BgeoContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitBgeo(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::BgeoContext* LSDParser::bgeo() {
  BgeoContext *_localctx = _tracker.createInstance<BgeoContext>(_ctx, getState());
  enterRule(_localctx, 4, LSDParser::RuleBgeo);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(55);
    match(LSDParser::BGEO_START);
    setState(59);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 2, _ctx);
    while (alt != 1 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1 + 1) {
        setState(56);
        matchWildcard(); 
      }
      setState(61);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 2, _ctx);
    }
    setState(62);
    _la = _input->LA(1);
    if (_la == 0 || _la == Token::EOF || (_la == LSDParser::T__0)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- SetenvContext ------------------------------------------------------------------

LSDParser::SetenvContext::SetenvContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* LSDParser::SetenvContext::VAR_NAME() {
  return getToken(LSDParser::VAR_NAME, 0);
}

tree::TerminalNode* LSDParser::SetenvContext::VALUE() {
  return getToken(LSDParser::VALUE, 0);
}


size_t LSDParser::SetenvContext::getRuleIndex() const {
  return LSDParser::RuleSetenv;
}

void LSDParser::SetenvContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterSetenv(this);
}

void LSDParser::SetenvContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitSetenv(this);
}


antlrcpp::Any LSDParser::SetenvContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitSetenv(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::SetenvContext* LSDParser::setenv() {
  SetenvContext *_localctx = _tracker.createInstance<SetenvContext>(_ctx, getState());
  enterRule(_localctx, 6, LSDParser::RuleSetenv);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(64);
    match(LSDParser::T__1);
    setState(65);
    match(LSDParser::VAR_NAME);
    setState(66);
    match(LSDParser::T__2);
    setState(67);
    match(LSDParser::VALUE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- VersionContext ------------------------------------------------------------------

LSDParser::VersionContext::VersionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* LSDParser::VersionContext::VEX_VERSION() {
  return getToken(LSDParser::VEX_VERSION, 0);
}


size_t LSDParser::VersionContext::getRuleIndex() const {
  return LSDParser::RuleVersion;
}

void LSDParser::VersionContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterVersion(this);
}

void LSDParser::VersionContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitVersion(this);
}


antlrcpp::Any LSDParser::VersionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitVersion(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::VersionContext* LSDParser::version() {
  VersionContext *_localctx = _tracker.createInstance<VersionContext>(_ctx, getState());
  enterRule(_localctx, 8, LSDParser::RuleVersion);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(69);
    match(LSDParser::T__3);
    setState(70);
    match(LSDParser::VEX_VERSION);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- DefaultsContext ------------------------------------------------------------------

LSDParser::DefaultsContext::DefaultsContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* LSDParser::DefaultsContext::STRING() {
  return getToken(LSDParser::STRING, 0);
}


size_t LSDParser::DefaultsContext::getRuleIndex() const {
  return LSDParser::RuleDefaults;
}

void LSDParser::DefaultsContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterDefaults(this);
}

void LSDParser::DefaultsContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitDefaults(this);
}


antlrcpp::Any LSDParser::DefaultsContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitDefaults(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::DefaultsContext* LSDParser::defaults() {
  DefaultsContext *_localctx = _tracker.createInstance<DefaultsContext>(_ctx, getState());
  enterRule(_localctx, 10, LSDParser::RuleDefaults);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(72);
    match(LSDParser::T__4);
    setState(73);
    match(LSDParser::STRING);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- DeclareContext ------------------------------------------------------------------

LSDParser::DeclareContext::DeclareContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* LSDParser::DeclareContext::OBJECT() {
  return getToken(LSDParser::OBJECT, 0);
}

tree::TerminalNode* LSDParser::DeclareContext::TYPE() {
  return getToken(LSDParser::TYPE, 0);
}

std::vector<tree::TerminalNode *> LSDParser::DeclareContext::VAR_NAME() {
  return getTokens(LSDParser::VAR_NAME);
}

tree::TerminalNode* LSDParser::DeclareContext::VAR_NAME(size_t i) {
  return getToken(LSDParser::VAR_NAME, i);
}

std::vector<tree::TerminalNode *> LSDParser::DeclareContext::VALUE() {
  return getTokens(LSDParser::VALUE);
}

tree::TerminalNode* LSDParser::DeclareContext::VALUE(size_t i) {
  return getToken(LSDParser::VALUE, i);
}


size_t LSDParser::DeclareContext::getRuleIndex() const {
  return LSDParser::RuleDeclare;
}

void LSDParser::DeclareContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterDeclare(this);
}

void LSDParser::DeclareContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitDeclare(this);
}


antlrcpp::Any LSDParser::DeclareContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitDeclare(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::DeclareContext* LSDParser::declare() {
  DeclareContext *_localctx = _tracker.createInstance<DeclareContext>(_ctx, getState());
  enterRule(_localctx, 12, LSDParser::RuleDeclare);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(75);
    match(LSDParser::T__5);
    setState(76);
    match(LSDParser::OBJECT);
    setState(77);
    match(LSDParser::TYPE);
    setState(78);
    match(LSDParser::VAR_NAME);
    setState(79);
    match(LSDParser::VALUE);
    setState(85);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == LSDParser::T__6) {
      setState(80);
      match(LSDParser::T__6);
      setState(81);
      match(LSDParser::VAR_NAME);
      setState(82);
      match(LSDParser::VALUE);
      setState(87);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StartContext ------------------------------------------------------------------

LSDParser::StartContext::StartContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* LSDParser::StartContext::OBJECT() {
  return getToken(LSDParser::OBJECT, 0);
}


size_t LSDParser::StartContext::getRuleIndex() const {
  return LSDParser::RuleStart;
}

void LSDParser::StartContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterStart(this);
}

void LSDParser::StartContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitStart(this);
}


antlrcpp::Any LSDParser::StartContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitStart(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::StartContext* LSDParser::start() {
  StartContext *_localctx = _tracker.createInstance<StartContext>(_ctx, getState());
  enterRule(_localctx, 14, LSDParser::RuleStart);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(88);
    match(LSDParser::T__7);
    setState(89);
    match(LSDParser::OBJECT);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- EndContext ------------------------------------------------------------------

LSDParser::EndContext::EndContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t LSDParser::EndContext::getRuleIndex() const {
  return LSDParser::RuleEnd;
}

void LSDParser::EndContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterEnd(this);
}

void LSDParser::EndContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitEnd(this);
}


antlrcpp::Any LSDParser::EndContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitEnd(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::EndContext* LSDParser::end() {
  EndContext *_localctx = _tracker.createInstance<EndContext>(_ctx, getState());
  enterRule(_localctx, 16, LSDParser::RuleEnd);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(91);
    match(LSDParser::T__0);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- DetailContext ------------------------------------------------------------------

LSDParser::DetailContext::DetailContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* LSDParser::DetailContext::OBJNAME() {
  return getToken(LSDParser::OBJNAME, 0);
}

tree::TerminalNode* LSDParser::DetailContext::STRING() {
  return getToken(LSDParser::STRING, 0);
}

std::vector<tree::TerminalNode *> LSDParser::DetailContext::VALUE() {
  return getTokens(LSDParser::VALUE);
}

tree::TerminalNode* LSDParser::DetailContext::VALUE(size_t i) {
  return getToken(LSDParser::VALUE, i);
}


size_t LSDParser::DetailContext::getRuleIndex() const {
  return LSDParser::RuleDetail;
}

void LSDParser::DetailContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterDetail(this);
}

void LSDParser::DetailContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitDetail(this);
}


antlrcpp::Any LSDParser::DetailContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitDetail(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::DetailContext* LSDParser::detail() {
  DetailContext *_localctx = _tracker.createInstance<DetailContext>(_ctx, getState());
  enterRule(_localctx, 18, LSDParser::RuleDetail);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(93);
    match(LSDParser::T__8);
    setState(102);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case LSDParser::T__9: {
        setState(94);
        match(LSDParser::T__9);
        break;
      }

      case LSDParser::T__10:
      case LSDParser::T__11: {
        setState(100);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case LSDParser::T__10: {
            setState(95);
            match(LSDParser::T__10);
            setState(96);
            match(LSDParser::VALUE);
            break;
          }

          case LSDParser::T__11: {
            setState(97);
            match(LSDParser::T__11);
            setState(98);
            match(LSDParser::VALUE);
            setState(99);
            match(LSDParser::VALUE);
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case LSDParser::OBJNAME: {
        break;
      }

    default:
      break;
    }
    setState(104);
    match(LSDParser::OBJNAME);
    setState(105);
    _la = _input->LA(1);
    if (!(_la == LSDParser::T__12

    || _la == LSDParser::STRING)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- PropertyContext ------------------------------------------------------------------

LSDParser::PropertyContext::PropertyContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* LSDParser::PropertyContext::OBJECT() {
  return getToken(LSDParser::OBJECT, 0);
}

tree::TerminalNode* LSDParser::PropertyContext::VAR_NAME() {
  return getToken(LSDParser::VAR_NAME, 0);
}

tree::TerminalNode* LSDParser::PropertyContext::VALUE() {
  return getToken(LSDParser::VALUE, 0);
}


size_t LSDParser::PropertyContext::getRuleIndex() const {
  return LSDParser::RuleProperty;
}

void LSDParser::PropertyContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterProperty(this);
}

void LSDParser::PropertyContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitProperty(this);
}


antlrcpp::Any LSDParser::PropertyContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitProperty(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::PropertyContext* LSDParser::property() {
  PropertyContext *_localctx = _tracker.createInstance<PropertyContext>(_ctx, getState());
  enterRule(_localctx, 20, LSDParser::RuleProperty);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(107);
    match(LSDParser::T__13);
    setState(108);
    match(LSDParser::OBJECT);
    setState(109);
    match(LSDParser::VAR_NAME);
    setState(111);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == LSDParser::VALUE) {
      setState(110);
      match(LSDParser::VALUE);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ImageContext ------------------------------------------------------------------

LSDParser::ImageContext::ImageContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> LSDParser::ImageContext::VALUE() {
  return getTokens(LSDParser::VALUE);
}

tree::TerminalNode* LSDParser::ImageContext::VALUE(size_t i) {
  return getToken(LSDParser::VALUE, i);
}


size_t LSDParser::ImageContext::getRuleIndex() const {
  return LSDParser::RuleImage;
}

void LSDParser::ImageContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterImage(this);
}

void LSDParser::ImageContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitImage(this);
}


antlrcpp::Any LSDParser::ImageContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitImage(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::ImageContext* LSDParser::image() {
  ImageContext *_localctx = _tracker.createInstance<ImageContext>(_ctx, getState());
  enterRule(_localctx, 22, LSDParser::RuleImage);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(113);
    match(LSDParser::T__14);
    setState(114);
    match(LSDParser::VALUE);
    setState(116);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == LSDParser::VALUE) {
      setState(115);
      match(LSDParser::VALUE);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- GeomertyContext ------------------------------------------------------------------

LSDParser::GeomertyContext::GeomertyContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* LSDParser::GeomertyContext::VALUE() {
  return getToken(LSDParser::VALUE, 0);
}


size_t LSDParser::GeomertyContext::getRuleIndex() const {
  return LSDParser::RuleGeomerty;
}

void LSDParser::GeomertyContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterGeomerty(this);
}

void LSDParser::GeomertyContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitGeomerty(this);
}


antlrcpp::Any LSDParser::GeomertyContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitGeomerty(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::GeomertyContext* LSDParser::geomerty() {
  GeomertyContext *_localctx = _tracker.createInstance<GeomertyContext>(_ctx, getState());
  enterRule(_localctx, 24, LSDParser::RuleGeomerty);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(118);
    match(LSDParser::T__15);
    setState(119);
    match(LSDParser::VALUE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TimeContext ------------------------------------------------------------------

LSDParser::TimeContext::TimeContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* LSDParser::TimeContext::VALUE() {
  return getToken(LSDParser::VALUE, 0);
}


size_t LSDParser::TimeContext::getRuleIndex() const {
  return LSDParser::RuleTime;
}

void LSDParser::TimeContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterTime(this);
}

void LSDParser::TimeContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitTime(this);
}


antlrcpp::Any LSDParser::TimeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitTime(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::TimeContext* LSDParser::time() {
  TimeContext *_localctx = _tracker.createInstance<TimeContext>(_ctx, getState());
  enterRule(_localctx, 26, LSDParser::RuleTime);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(121);
    match(LSDParser::T__16);
    setState(122);
    match(LSDParser::VALUE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- RaytraceContext ------------------------------------------------------------------

LSDParser::RaytraceContext::RaytraceContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t LSDParser::RaytraceContext::getRuleIndex() const {
  return LSDParser::RuleRaytrace;
}

void LSDParser::RaytraceContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterRaytrace(this);
}

void LSDParser::RaytraceContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitRaytrace(this);
}


antlrcpp::Any LSDParser::RaytraceContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitRaytrace(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::RaytraceContext* LSDParser::raytrace() {
  RaytraceContext *_localctx = _tracker.createInstance<RaytraceContext>(_ctx, getState());
  enterRule(_localctx, 28, LSDParser::RuleRaytrace);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(124);
    match(LSDParser::T__17);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- QuitContext ------------------------------------------------------------------

LSDParser::QuitContext::QuitContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t LSDParser::QuitContext::getRuleIndex() const {
  return LSDParser::RuleQuit;
}

void LSDParser::QuitContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterQuit(this);
}

void LSDParser::QuitContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<LSDListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitQuit(this);
}


antlrcpp::Any LSDParser::QuitContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<LSDVisitor*>(visitor))
    return parserVisitor->visitQuit(this);
  else
    return visitor->visitChildren(this);
}

LSDParser::QuitContext* LSDParser::quit() {
  QuitContext *_localctx = _tracker.createInstance<QuitContext>(_ctx, getState());
  enterRule(_localctx, 30, LSDParser::RuleQuit);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(126);
    match(LSDParser::T__18);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

// Static vars and initialization.
std::vector<dfa::DFA> LSDParser::_decisionToDFA;
atn::PredictionContextCache LSDParser::_sharedContextCache;

// We own the ATN which in turn owns the ATN states.
atn::ATN LSDParser::_atn;
std::vector<uint16_t> LSDParser::_serializedATN;

std::vector<std::string> LSDParser::_ruleNames = {
  "file", "line", "bgeo", "setenv", "version", "defaults", "declare", "start", 
  "end", "detail", "property", "image", "geomerty", "time", "raytrace", 
  "quit"
};

std::vector<std::string> LSDParser::_literalNames = {
  "", "'cmd_end'", "'setenv'", "'='", "'cmd_version'", "'cmd_defaults'", 
  "'cmd_declare'", "';'", "'cmd_start'", "'cmd_detail'", "'-T'", "'-v'", 
  "'-V'", "'stdin'", "'cmd_property'", "'cmd_image'", "'cmd_geometry'", 
  "'cmd_time'", "'cmd_raytrace'", "'cmd_quit'"
};

std::vector<std::string> LSDParser::_symbolicNames = {
  "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
  "", "", "COMMENT", "OBJNAME", "TYPE", "OBJECT", "VEX_VERSION", "VAR_NAME", 
  "VALUE", "INTEGER", "NUMBER", "STRING", "NO_QUOTED", "QUOTED", "CHARS", 
  "BGEO_START", "WS"
};

dfa::Vocabulary LSDParser::_vocabulary(_literalNames, _symbolicNames);

std::vector<std::string> LSDParser::_tokenNames;

LSDParser::Initializer::Initializer() {
	for (size_t i = 0; i < _symbolicNames.size(); ++i) {
		std::string name = _vocabulary.getLiteralName(i);
		if (name.empty()) {
			name = _vocabulary.getSymbolicName(i);
		}

		if (name.empty()) {
			_tokenNames.push_back("<INVALID>");
		} else {
      _tokenNames.push_back(name);
    }
	}

  _serializedATN = {
    0x3, 0x608b, 0xa72a, 0x8133, 0xb9ed, 0x417c, 0x3be7, 0x7786, 0x5964, 
    0x3, 0x24, 0x83, 0x4, 0x2, 0x9, 0x2, 0x4, 0x3, 0x9, 0x3, 0x4, 0x4, 0x9, 
    0x4, 0x4, 0x5, 0x9, 0x5, 0x4, 0x6, 0x9, 0x6, 0x4, 0x7, 0x9, 0x7, 0x4, 
    0x8, 0x9, 0x8, 0x4, 0x9, 0x9, 0x9, 0x4, 0xa, 0x9, 0xa, 0x4, 0xb, 0x9, 
    0xb, 0x4, 0xc, 0x9, 0xc, 0x4, 0xd, 0x9, 0xd, 0x4, 0xe, 0x9, 0xe, 0x4, 
    0xf, 0x9, 0xf, 0x4, 0x10, 0x9, 0x10, 0x4, 0x11, 0x9, 0x11, 0x3, 0x2, 
    0x7, 0x2, 0x24, 0xa, 0x2, 0xc, 0x2, 0xe, 0x2, 0x27, 0xb, 0x2, 0x3, 0x3, 
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 
    0x5, 0x3, 0x38, 0xa, 0x3, 0x3, 0x4, 0x3, 0x4, 0x7, 0x4, 0x3c, 0xa, 0x4, 
    0xc, 0x4, 0xe, 0x4, 0x3f, 0xb, 0x4, 0x3, 0x4, 0x3, 0x4, 0x3, 0x5, 0x3, 
    0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 
    0x7, 0x3, 0x7, 0x3, 0x7, 0x3, 0x8, 0x3, 0x8, 0x3, 0x8, 0x3, 0x8, 0x3, 
    0x8, 0x3, 0x8, 0x3, 0x8, 0x3, 0x8, 0x7, 0x8, 0x56, 0xa, 0x8, 0xc, 0x8, 
    0xe, 0x8, 0x59, 0xb, 0x8, 0x3, 0x9, 0x3, 0x9, 0x3, 0x9, 0x3, 0xa, 0x3, 
    0xa, 0x3, 0xb, 0x3, 0xb, 0x3, 0xb, 0x3, 0xb, 0x3, 0xb, 0x3, 0xb, 0x3, 
    0xb, 0x5, 0xb, 0x67, 0xa, 0xb, 0x5, 0xb, 0x69, 0xa, 0xb, 0x3, 0xb, 0x3, 
    0xb, 0x3, 0xb, 0x3, 0xc, 0x3, 0xc, 0x3, 0xc, 0x3, 0xc, 0x5, 0xc, 0x72, 
    0xa, 0xc, 0x3, 0xd, 0x3, 0xd, 0x3, 0xd, 0x5, 0xd, 0x77, 0xa, 0xd, 0x3, 
    0xe, 0x3, 0xe, 0x3, 0xe, 0x3, 0xf, 0x3, 0xf, 0x3, 0xf, 0x3, 0x10, 0x3, 
    0x10, 0x3, 0x11, 0x3, 0x11, 0x3, 0x11, 0x3, 0x3d, 0x2, 0x12, 0x2, 0x4, 
    0x6, 0x8, 0xa, 0xc, 0xe, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e, 
    0x20, 0x2, 0x4, 0x3, 0x2, 0x3, 0x3, 0x4, 0x2, 0xf, 0xf, 0x1f, 0x1f, 
    0x2, 0x88, 0x2, 0x25, 0x3, 0x2, 0x2, 0x2, 0x4, 0x37, 0x3, 0x2, 0x2, 
    0x2, 0x6, 0x39, 0x3, 0x2, 0x2, 0x2, 0x8, 0x42, 0x3, 0x2, 0x2, 0x2, 0xa, 
    0x47, 0x3, 0x2, 0x2, 0x2, 0xc, 0x4a, 0x3, 0x2, 0x2, 0x2, 0xe, 0x4d, 
    0x3, 0x2, 0x2, 0x2, 0x10, 0x5a, 0x3, 0x2, 0x2, 0x2, 0x12, 0x5d, 0x3, 
    0x2, 0x2, 0x2, 0x14, 0x5f, 0x3, 0x2, 0x2, 0x2, 0x16, 0x6d, 0x3, 0x2, 
    0x2, 0x2, 0x18, 0x73, 0x3, 0x2, 0x2, 0x2, 0x1a, 0x78, 0x3, 0x2, 0x2, 
    0x2, 0x1c, 0x7b, 0x3, 0x2, 0x2, 0x2, 0x1e, 0x7e, 0x3, 0x2, 0x2, 0x2, 
    0x20, 0x80, 0x3, 0x2, 0x2, 0x2, 0x22, 0x24, 0x5, 0x4, 0x3, 0x2, 0x23, 
    0x22, 0x3, 0x2, 0x2, 0x2, 0x24, 0x27, 0x3, 0x2, 0x2, 0x2, 0x25, 0x23, 
    0x3, 0x2, 0x2, 0x2, 0x25, 0x26, 0x3, 0x2, 0x2, 0x2, 0x26, 0x3, 0x3, 
    0x2, 0x2, 0x2, 0x27, 0x25, 0x3, 0x2, 0x2, 0x2, 0x28, 0x38, 0x5, 0xa, 
    0x6, 0x2, 0x29, 0x38, 0x5, 0xe, 0x8, 0x2, 0x2a, 0x38, 0x5, 0x8, 0x5, 
    0x2, 0x2b, 0x38, 0x5, 0x10, 0x9, 0x2, 0x2c, 0x38, 0x5, 0x12, 0xa, 0x2, 
    0x2d, 0x38, 0x5, 0x16, 0xc, 0x2, 0x2e, 0x38, 0x5, 0x14, 0xb, 0x2, 0x2f, 
    0x38, 0x5, 0x18, 0xd, 0x2, 0x30, 0x38, 0x5, 0x1a, 0xe, 0x2, 0x31, 0x38, 
    0x5, 0x1c, 0xf, 0x2, 0x32, 0x38, 0x5, 0x6, 0x4, 0x2, 0x33, 0x38, 0x5, 
    0x1e, 0x10, 0x2, 0x34, 0x38, 0x5, 0x20, 0x11, 0x2, 0x35, 0x38, 0x5, 
    0xc, 0x7, 0x2, 0x36, 0x38, 0x7, 0x16, 0x2, 0x2, 0x37, 0x28, 0x3, 0x2, 
    0x2, 0x2, 0x37, 0x29, 0x3, 0x2, 0x2, 0x2, 0x37, 0x2a, 0x3, 0x2, 0x2, 
    0x2, 0x37, 0x2b, 0x3, 0x2, 0x2, 0x2, 0x37, 0x2c, 0x3, 0x2, 0x2, 0x2, 
    0x37, 0x2d, 0x3, 0x2, 0x2, 0x2, 0x37, 0x2e, 0x3, 0x2, 0x2, 0x2, 0x37, 
    0x2f, 0x3, 0x2, 0x2, 0x2, 0x37, 0x30, 0x3, 0x2, 0x2, 0x2, 0x37, 0x31, 
    0x3, 0x2, 0x2, 0x2, 0x37, 0x32, 0x3, 0x2, 0x2, 0x2, 0x37, 0x33, 0x3, 
    0x2, 0x2, 0x2, 0x37, 0x34, 0x3, 0x2, 0x2, 0x2, 0x37, 0x35, 0x3, 0x2, 
    0x2, 0x2, 0x37, 0x36, 0x3, 0x2, 0x2, 0x2, 0x38, 0x5, 0x3, 0x2, 0x2, 
    0x2, 0x39, 0x3d, 0x7, 0x23, 0x2, 0x2, 0x3a, 0x3c, 0xb, 0x2, 0x2, 0x2, 
    0x3b, 0x3a, 0x3, 0x2, 0x2, 0x2, 0x3c, 0x3f, 0x3, 0x2, 0x2, 0x2, 0x3d, 
    0x3e, 0x3, 0x2, 0x2, 0x2, 0x3d, 0x3b, 0x3, 0x2, 0x2, 0x2, 0x3e, 0x40, 
    0x3, 0x2, 0x2, 0x2, 0x3f, 0x3d, 0x3, 0x2, 0x2, 0x2, 0x40, 0x41, 0xa, 
    0x2, 0x2, 0x2, 0x41, 0x7, 0x3, 0x2, 0x2, 0x2, 0x42, 0x43, 0x7, 0x4, 
    0x2, 0x2, 0x43, 0x44, 0x7, 0x1b, 0x2, 0x2, 0x44, 0x45, 0x7, 0x5, 0x2, 
    0x2, 0x45, 0x46, 0x7, 0x1c, 0x2, 0x2, 0x46, 0x9, 0x3, 0x2, 0x2, 0x2, 
    0x47, 0x48, 0x7, 0x6, 0x2, 0x2, 0x48, 0x49, 0x7, 0x1a, 0x2, 0x2, 0x49, 
    0xb, 0x3, 0x2, 0x2, 0x2, 0x4a, 0x4b, 0x7, 0x7, 0x2, 0x2, 0x4b, 0x4c, 
    0x7, 0x1f, 0x2, 0x2, 0x4c, 0xd, 0x3, 0x2, 0x2, 0x2, 0x4d, 0x4e, 0x7, 
    0x8, 0x2, 0x2, 0x4e, 0x4f, 0x7, 0x19, 0x2, 0x2, 0x4f, 0x50, 0x7, 0x18, 
    0x2, 0x2, 0x50, 0x51, 0x7, 0x1b, 0x2, 0x2, 0x51, 0x57, 0x7, 0x1c, 0x2, 
    0x2, 0x52, 0x53, 0x7, 0x9, 0x2, 0x2, 0x53, 0x54, 0x7, 0x1b, 0x2, 0x2, 
    0x54, 0x56, 0x7, 0x1c, 0x2, 0x2, 0x55, 0x52, 0x3, 0x2, 0x2, 0x2, 0x56, 
    0x59, 0x3, 0x2, 0x2, 0x2, 0x57, 0x55, 0x3, 0x2, 0x2, 0x2, 0x57, 0x58, 
    0x3, 0x2, 0x2, 0x2, 0x58, 0xf, 0x3, 0x2, 0x2, 0x2, 0x59, 0x57, 0x3, 
    0x2, 0x2, 0x2, 0x5a, 0x5b, 0x7, 0xa, 0x2, 0x2, 0x5b, 0x5c, 0x7, 0x19, 
    0x2, 0x2, 0x5c, 0x11, 0x3, 0x2, 0x2, 0x2, 0x5d, 0x5e, 0x7, 0x3, 0x2, 
    0x2, 0x5e, 0x13, 0x3, 0x2, 0x2, 0x2, 0x5f, 0x68, 0x7, 0xb, 0x2, 0x2, 
    0x60, 0x69, 0x7, 0xc, 0x2, 0x2, 0x61, 0x62, 0x7, 0xd, 0x2, 0x2, 0x62, 
    0x67, 0x7, 0x1c, 0x2, 0x2, 0x63, 0x64, 0x7, 0xe, 0x2, 0x2, 0x64, 0x65, 
    0x7, 0x1c, 0x2, 0x2, 0x65, 0x67, 0x7, 0x1c, 0x2, 0x2, 0x66, 0x61, 0x3, 
    0x2, 0x2, 0x2, 0x66, 0x63, 0x3, 0x2, 0x2, 0x2, 0x67, 0x69, 0x3, 0x2, 
    0x2, 0x2, 0x68, 0x60, 0x3, 0x2, 0x2, 0x2, 0x68, 0x66, 0x3, 0x2, 0x2, 
    0x2, 0x68, 0x69, 0x3, 0x2, 0x2, 0x2, 0x69, 0x6a, 0x3, 0x2, 0x2, 0x2, 
    0x6a, 0x6b, 0x7, 0x17, 0x2, 0x2, 0x6b, 0x6c, 0x9, 0x3, 0x2, 0x2, 0x6c, 
    0x15, 0x3, 0x2, 0x2, 0x2, 0x6d, 0x6e, 0x7, 0x10, 0x2, 0x2, 0x6e, 0x6f, 
    0x7, 0x19, 0x2, 0x2, 0x6f, 0x71, 0x7, 0x1b, 0x2, 0x2, 0x70, 0x72, 0x7, 
    0x1c, 0x2, 0x2, 0x71, 0x70, 0x3, 0x2, 0x2, 0x2, 0x71, 0x72, 0x3, 0x2, 
    0x2, 0x2, 0x72, 0x17, 0x3, 0x2, 0x2, 0x2, 0x73, 0x74, 0x7, 0x11, 0x2, 
    0x2, 0x74, 0x76, 0x7, 0x1c, 0x2, 0x2, 0x75, 0x77, 0x7, 0x1c, 0x2, 0x2, 
    0x76, 0x75, 0x3, 0x2, 0x2, 0x2, 0x76, 0x77, 0x3, 0x2, 0x2, 0x2, 0x77, 
    0x19, 0x3, 0x2, 0x2, 0x2, 0x78, 0x79, 0x7, 0x12, 0x2, 0x2, 0x79, 0x7a, 
    0x7, 0x1c, 0x2, 0x2, 0x7a, 0x1b, 0x3, 0x2, 0x2, 0x2, 0x7b, 0x7c, 0x7, 
    0x13, 0x2, 0x2, 0x7c, 0x7d, 0x7, 0x1c, 0x2, 0x2, 0x7d, 0x1d, 0x3, 0x2, 
    0x2, 0x2, 0x7e, 0x7f, 0x7, 0x14, 0x2, 0x2, 0x7f, 0x1f, 0x3, 0x2, 0x2, 
    0x2, 0x80, 0x81, 0x7, 0x15, 0x2, 0x2, 0x81, 0x21, 0x3, 0x2, 0x2, 0x2, 
    0xa, 0x25, 0x37, 0x3d, 0x57, 0x66, 0x68, 0x71, 0x76, 
  };

  atn::ATNDeserializer deserializer;
  _atn = deserializer.deserialize(_serializedATN);

  size_t count = _atn.getNumberOfDecisions();
  _decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    _decisionToDFA.emplace_back(_atn.getDecisionState(i), i);
  }
}

LSDParser::Initializer LSDParser::_init;
