#include "grammar_lsd_expr.h"

namespace x3 = boost::spirit::x3;
namespace fs = boost::filesystem;

namespace lava::lsd::expr { 

namespace ast {

bool Node::operator==(Node const & other) const { 
    //return x == other.x;
    return true;
}

bool Node::operator!=(Node const & other) const { 
    //return x == other.x;
    return false;
}

Expr::operator bool() const { 
    switch(op) {
        case ast::OP::EQ:
            return left == right;
        case ast::OP::NEQ:
            return left != right;
        case ast::OP::AND:
        //    _os << "AND(";
        //    break;
        case ast::OP::OR:
        //    _os << "OR(";
        //    break;
        default:
            return false;
    }
}

}

void EchoVisitor::operator()(ast::Expr const& expr) const {
    _os << expr;
}

void EchoVisitor::operator()(ast::Node const& node) const {
    boost::apply_visitor(*this, node);
}

void EchoVisitor::operator()(ast::ExprValue const& val) const {
    boost::apply_visitor(*this, val);
}

void EchoVisitor::operator()(std::string const& val) const {
    _os << val;
}

void EchoVisitor::operator()(double val) const {
    _os << val;
}

void EchoVisitor::operator()(int val) const {
    _os << '"' << val << '"';
} 

}