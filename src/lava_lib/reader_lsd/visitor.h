#ifndef SRC_LAVA_LIB_READER_LSD_LSD_VISITOR_H_
#define SRC_LAVA_LIB_READER_LSD_LSD_VISITOR_H_

#include <array>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include <variant>

#include "grammar_lsd.h"
#include "../reader_bgeo/bgeo/Bgeo.h"

namespace lava { 

namespace lsd { 

class Session;

struct Visitor: public boost::static_visitor<> {
  public:
    Visitor(std::unique_ptr<Session>& pSession);

    virtual void operator()(ast::NoValue const& c) const {};
    virtual void operator()(ast::ifthen const& c);
    virtual void operator()(ast::endif const& c);
    virtual void operator()(ast::setenv const& c) const;
    virtual void operator()(ast::cmd_image const& c) const;
    virtual void operator()(ast::cmd_iprmode const& c) const;
    virtual void operator()(ast::cmd_end const& c) const;
    virtual void operator()(ast::cmd_edge const& c) const;
    virtual void operator()(ast::cmd_quit const& c) const;
    virtual void operator()(ast::cmd_start const& c) const;
    virtual void operator()(ast::cmd_time const& c) const;
    virtual void operator()(ast::cmd_detail const& c);
    virtual void operator()(ast::cmd_version const& c) const;
    virtual void operator()(ast::cmd_delete const& c) const;
    virtual void operator()(ast::cmd_config const& c) const;
    virtual void operator()(ast::cmd_defaults const& c) const;
    virtual void operator()(ast::cmd_transform const& c) const;
    virtual void operator()(ast::cmd_mtransform const& c) const;
    virtual void operator()(ast::cmd_geometry const& c) const;
    virtual void operator()(ast::cmd_property const& c) const;
    virtual void operator()(ast::cmd_procedural const& c) const;
    virtual void operator()(ast::cmd_deviceoption const& c) const;
    virtual void operator()(ast::cmd_declare const& c) const;
    virtual void operator()(ast::cmd_raytrace const& c) const;
    virtual void operator()(ast::cmd_reset const& c) const;
    virtual void operator()(ast::cmd_socket const& c) const;
    virtual void operator()(ast::ray_embeddedfile const& c) const;

    void setParserStream(std::istream& in);

    bool ignoreCommands() const { return mIgnoreCommands; };
    bool readyToQuit() const { return mQuit; };
    bool failed() const;

  protected:
    std::unique_ptr<Session> mpSession;

  private:
    std::istream*   mpParserStream; // used for inline bgeo reading
    bool mIgnoreCommands;
    bool mQuit;
};


struct EchoVisitor: public Visitor {
  public:
    EchoVisitor(std::unique_ptr<Session>& pSession);
    EchoVisitor(std::unique_ptr<Session>& pSession, std::ostream& os);

    void operator()(ast::NoValue const& c) const override {};
    void operator()(ast::ifthen const& c) override;
    void operator()(ast::endif const& c) override;
    void operator()(ast::setenv const& c) const override;
    void operator()(ast::cmd_image const& c) const override;
    void operator()(ast::cmd_iprmode const& c) const override;
    void operator()(ast::cmd_end const& c) const override;
    void operator()(ast::cmd_edge const& c) const override;
    void operator()(ast::cmd_quit const& c) const override;
    void operator()(ast::cmd_start const& c) const override;
    void operator()(ast::cmd_time const& c) const override;
    void operator()(ast::cmd_detail const& c) override;
    void operator()(ast::cmd_version const& c) const override;
    void operator()(ast::cmd_delete const& c) const override;
    void operator()(ast::cmd_config const& c) const override;
    void operator()(ast::cmd_defaults const& c) const override;
    void operator()(ast::cmd_transform const& c) const override;
    void operator()(ast::cmd_mtransform const& c) const override;
    void operator()(ast::cmd_geometry const& c) const override;
    void operator()(ast::cmd_property const& c) const override;
    void operator()(ast::cmd_procedural const& c) const override;
    void operator()(ast::cmd_deviceoption const& c) const override;
    void operator()(ast::cmd_declare const& c) const override;
    void operator()(ast::cmd_raytrace const& c) const override;
    void operator()(ast::cmd_reset const& c) const override;
    void operator()(ast::cmd_socket const& c) const override;
    void operator()(ast::ray_embeddedfile const& c) const override;

  //private:
    void operator()(std::vector<PropValue> const& v) const;
    void operator()(int v) const;
    void operator()(double v) const;
    void operator()(std::string const& v) const;
    void operator()(Int2 const& v) const;
    void operator()(Int3 const& v) const;
    void operator()(Int4 const& v) const;
    void operator()(Vector2 const& v) const;
    void operator()(Vector3 const& v) const;
    void operator()(Vector4 const& v) const;
    void operator()(PropValue const& v) const;

 private:
    std::ostream& _os;
};

}  // namespace lsd

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_LSD_VISITOR_H_