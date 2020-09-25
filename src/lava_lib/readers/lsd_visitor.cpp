#include "grammar_lsd.h"
#include "renderer_iface_lsd.h"

namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;

namespace lava { 

namespace lsd { 

Visitor::Visitor(std::unique_ptr<RendererIfaceLSD>& iface): mIface(std::move(iface)) {

} 

void Visitor::operator()(ast::ifthen const& c) const {
    
}

void Visitor::operator()(ast::setenv const& c) const {
    std::cout << "Visitor setenv\n";
    std::cout << "iface" << mIface.get();
    mIface->setEnvVariable(c.key, c.value);
};

void Visitor::operator()(ast::cmd_image const& c) const { 
    std::cout << "LSDVisitor cmd_image\n";
    if (c.display_type != ast::DisplayType::NONE) {
        mIface->loadDisplayByType(c.display_type);
    } else {
        mIface->loadDisplayByFileName(c.filename);
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

void Visitor::operator()(ast::cmd_detail const& c) const {

}

void Visitor::operator()(ast::cmd_version const& c) const {

}

void Visitor::operator()(ast::cmd_config const& c) const {
    mIface->cmdConfig(c.filename);
}

void Visitor::operator()(ast::cmd_defaults const& c) const {

}

void Visitor::operator()(ast::cmd_transform const& c) const {

}

void Visitor::operator()(ast::cmd_geometry const& c) const {

}

void Visitor::operator()(ast::cmd_property const& c) const {
   
}

void Visitor::operator()(ast::cmd_deviceoption const& c) const {

}

void Visitor::operator()(ast::cmd_declare const& c) const {

}

void Visitor::operator()(ast::cmd_raytrace const& c) const {
    mIface->cmdRaytrace();
}

}  // namespace lsd

}  // namespace lava
