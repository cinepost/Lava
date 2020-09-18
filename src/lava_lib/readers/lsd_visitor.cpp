#include "grammar_lsd.h"

namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;

namespace lava { 

namespace lsd {  

void LSDVisitor::operator()(ast::setenv const& c) const {
    std::cout << "LSDVisitor setenv\n";
    std::cout << "iface" << mIface.get();
    mIface->setEnvVariable(c.key, c.value);
};

void LSDVisitor::operator()(ast::cmd_image const& c) const { 
    std::cout << "LSDVisitor cmd_image\n";
}

void LSDVisitor::operator()(ast::cmd_end const& c) const { 
    std::cout << "LSDVisitor cmd_end\n";
}

void LSDVisitor::operator()(ast::cmd_quit const& c) const { 
    std::cout << "LSDVisitor cmd_quit\n";
}

void LSDVisitor::operator()(ast::cmd_start const& c) const {

}

void LSDVisitor::operator()(ast::cmd_time const& c) const {

}

void LSDVisitor::operator()(ast::cmd_detail const& c) const {

}

void LSDVisitor::operator()(ast::cmd_detail_inline const& c) const { 

}

void LSDVisitor::operator()(ast::cmd_version const& c) const {

}

void LSDVisitor::operator()(ast::cmd_defaults const& c) const {

}

void LSDVisitor::operator()(ast::cmd_transform const& c) const {

}

void LSDVisitor::operator()(ast::cmd_geometry const& c) const {

}

void LSDVisitor::operator()(ast::cmd_property const& c) const {
   
}

void LSDVisitor::operator()(ast::cmd_deviceoption const& c) const {

}

void LSDVisitor::operator()(ast::cmd_declare const& c) const {

}

void LSDVisitor::operator()(ast::cmd_raytrace const& c) const {

}

}  // namespace lsd

}  // namespace lava
