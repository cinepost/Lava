#include "visitor.h"
#include "session.h"

namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;


namespace lava { 

namespace lsd {

EchoVisitor::EchoVisitor(std::unique_ptr<Session>& pSession): Visitor(pSession), _os(std::cout){ 

}

EchoVisitor::EchoVisitor(std::unique_ptr<Session>& pSession, std::ostream& os): Visitor(pSession), _os(os){ 

}

void EchoVisitor::operator()(int v) const {
    _os << v;
}

void EchoVisitor::operator()(double v) const {
    _os << v;
}

void EchoVisitor::operator()(std::string const& v) const {
    _os << '"' << v << '"';
}

void EchoVisitor::operator()(Int2 const& v) const {
    _os << v;
}

void EchoVisitor::operator()(Int3 const& v) const {
    _os << v;
}

void EchoVisitor::operator()(Int4 const& v) const {
    _os << v;
}

void EchoVisitor::operator()(Vector2 const& v) const {
    _os << v;
}

void EchoVisitor::operator()(Vector3 const& v) const {
    _os << v;
}

void EchoVisitor::operator()(Vector4 const& v) const {
    _os << v;
}

void EchoVisitor::operator()(PropValue const& v) const { 
    boost::apply_visitor(*this, v);
}

void EchoVisitor::operator()(std::vector<PropValue> const& v) const {
    if (!v.empty()) {
        _os << "[ ";
        for(std::vector<PropValue>::const_iterator it = v.begin(); it != (v.end() - 1); it++) {
            boost::apply_visitor(*this, *it);
            _os << " ";
        }
        boost::apply_visitor(*this, v.back());
        _os << " ]";
    } else {
        _os << "!!! EMPTY !!!";
    }
}

void EchoVisitor::operator()(ast::ifthen const& c) {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> ifthen: " << c.expr << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::endif const& c) {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> endif: \x1b[0m\n";
}


void EchoVisitor::operator()(ast::setenv const& c) const { 
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> setenv: " << c.key << " = " << c.value << "\x1b[0m\n"; 
};

void EchoVisitor::operator()(ast::cmd_image const& c) const { 
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_image: ";
    if(c.display_type != ast::DisplayType::NONE) {
        _os << "type: ";
        switch(c.display_type) {
            case ast::DisplayType::IP:
                _os << "md";
                break;
            case ast::DisplayType::MD:
                _os << "ip";
                break;
            case ast::DisplayType::OPENEXR:
                _os << "openexr";
                break;
            case ast::DisplayType::JPEG:
                _os << "jpeg";
                break;
            case ast::DisplayType::TIFF:
                _os << "tiff";
                break;
            case ast::DisplayType::PNG:
                _os << "png";
                break;
            default:
                _os << "unknown";
                break;
        }
        _os << " ";
    }
    if(!c.filename.empty()) {
        _os << "filename: " << c.filename;
    }
    _os << "\x1b[0m\n"; 
}

void EchoVisitor::operator()(ast::cmd_end const& c) const { 
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_end: " << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_edge const& c) const { 
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_edge: " << c.src_node_uuid << " " << c.src_node_output_socket << " " 
        << c.dst_node_uuid << " " << c.dst_node_input_socket << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_quit const& c) const { 
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_quit: " << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_socket const& c) const { 
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_socket: " << c.direction << " " << c.data_type << " " << c.name << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_start const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_start: " << c.object_type << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_time const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_time: " << c.time << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_iprmode const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_iprmode: " << c.mode << " stash:" << c.stash <<"\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_detail const& c) {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_detail: name: " << c.name << " filename: " << c.filename;
    if(c.filename != "stdin"){
        _os << " expanded filename: " << mpSession->getExpandedString(c.filename);
    }
    _os << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_version const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_version: " << c.version << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_defaults const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_defaults: filename: " << c.filename << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_config const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_config: type: " << to_string(c.prop_type) << " name:" << c.prop_name << " value: ";
    EchoVisitor::operator()(c.prop_value);
    _os << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_transform const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_transform: " << c.m << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_mtransform const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_mtransform: " << c.m << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_geometry const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_geometry: geometry_object: " << c.geometry_name << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_property const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_property: style: " << c.style;
    if(c.values.size() > 1) {
        _os << " [ ";
        for( const auto& value: c.values ) {
            _os << " token: " << value.first << " value: ";
            EchoVisitor::operator()(value.second);
            _os << ", ";
        }
        _os << " ]";
    } else {
        _os << " token: " << c.values[0].first << " value: ";
        EchoVisitor::operator()(c.values[0].second);
    }
    _os << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_deviceoption const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_deviceoption: type: " << c.type << " name: " << c.name << " values: ";
    EchoVisitor::operator()(c.values);
    _os << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_declare const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_declare: style: " << c.style << " token: " << c.token << " type: " << c.type << " values: ";
    EchoVisitor::operator()(c.values);
    _os << "\x1b[0m\n";
}

void EchoVisitor::operator()(ast::cmd_raytrace const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_raytrace: " << "\x1b[0m\n";
} 

void EchoVisitor::operator()(ast::cmd_reset const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> cmd_reset: ";
    if (c.lights) _os << "lights ";
    if (c.objects) _os << "objects";
    if (c.fogs) _os << "fogs";
    _os << "\x1b[0m\n";
} 

void EchoVisitor::operator()(ast::ray_embeddedfile const& c) const {
    Visitor::operator()(c);
    _os << "\x1b[32m" << "> ray_embeddedfile: " << c.type << " " << c.name << " " << c.size <<"\x1b[0m\n";
} 

}  // namespace lsd

}  // namespace lava
