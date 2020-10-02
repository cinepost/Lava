#include "visitor.h"
#include "session_lsd.h"

namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;

namespace lava { 

namespace lsd {

Visitor::Visitor(std::unique_ptr<SessionLSD>& pSession): mpSession(std::move(pSession)) {
    push_flag(Visitor::Flag::NOFLAG);
} 

const std::vector<Visitor::Flag>& Visitor::flags() const {
    return flag_stack;
}

Visitor::Flag Visitor::current_flag() const {
    return flag_stack.back();
}

void Visitor::operator()(ast::ifthen const& c) {
    if (!c.expr) {
        // false expression evaluation, ignore commands until 'endif'
        push_flag(Visitor::Flag::IGNORE_CMDS);
    }
}

void Visitor::operator()(ast::endif const& c) {
    if (current_flag() == Visitor::Flag::IGNORE_CMDS)
        pop_flag();
}

void Visitor::operator()(ast::setenv const& c) const {
    std::cout << "Visitor setenv\n";
    mpSession->cmdSetEnv(c.key, c.value);
};

void Visitor::operator()(ast::cmd_image const& c) const { 
    std::cout << "LSDVisitor cmd_image\n";
    if (c.display_type != ast::DisplayType::NONE) {
        mpSession->loadDisplayByType(c.display_type);
    } else {
        mpSession->loadDisplayByFileName(c.filename);
    }
}

void Visitor::operator()(ast::cmd_end const& c) const { 
    std::cout << "LSDVisitor cmd_end\n";
}

void Visitor::operator()(ast::cmd_quit const& c) const { 
    std::cout << "LSDVisitor cmd_quit\n";
}

void Visitor::operator()(ast::cmd_start const& c) const {

}

void Visitor::operator()(ast::cmd_time const& c) const {

}

void Visitor::operator()(ast::cmd_detail const& c) {
    if(c.filename == "stdin") {
        // set flag so reader can process inline geo reading
        push_flag(Visitor::Flag::READ_INLINE_GEO);
        return;
    }
}

void Visitor::inlineGeoReadDone() {
    if (current_flag() == Visitor::Flag::READ_INLINE_GEO)
        pop_flag();
}

void Visitor::operator()(ast::cmd_version const& c) const {

}

void Visitor::operator()(ast::cmd_config const& c) const {
    mpSession->cmdConfig(c.filename);
}

void Visitor::operator()(ast::cmd_defaults const& c) const {

}

void Visitor::operator()(ast::cmd_transform const& c) const {

}

void Visitor::operator()(ast::cmd_geometry const& c) const {

}

void Visitor::operator()(ast::cmd_property const& c) const {
   //mpSession->cmdProperty(c);
}

void Visitor::operator()(ast::cmd_deviceoption const& c) const {

}

void Visitor::operator()(ast::cmd_declare const& c) const {
    //mpSession->cmdDeclare(c);
}

void Visitor::operator()(ast::cmd_raytrace const& c) const {
    mpSession->cmdRaytrace();
}

}  // namespace lsd

}  // namespace lava
