#ifndef SRC_LAVA_LIB_GRAMMAR_LSD_EXPR_H_
#define SRC_LAVA_LIB_GRAMMAR_LSD_EXPR_H_

#include <array>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include <variant>
#include <map>

#include <boost/array.hpp>
#include <boost/filesystem.hpp>
#include <boost/range.hpp>
//#include <boost/range/join.hpp>
//#include <boost/algorithm/string/join.hpp>

#ifdef DEBUG
   // #define BOOST_SPIRIT_X3_DEBUG
#endif

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/support/traits/container_traits.hpp>
#include <boost/spirit/include/support_istream_iterator.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/sequence/io.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/adapted/array.hpp>
#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/fusion/adapted.hpp>

#include <boost/container/static_vector.hpp>


namespace x3 = boost::spirit::x3;

namespace lava {

namespace lsd {

namespace expr {

namespace ast {


    // an expression node (either an EQ or an NEQ )
    struct Expr;

    struct ExprValue: x3::variant<std::string, int, double> {
        using base_type::base_type;
        using base_type::operator=;
    };

    // child of an expression -- either another expression, or a terminal
    struct Node : x3::variant<ExprValue, x3::forward_ast<Expr>> {
        using base_type::base_type;
        using base_type::operator=;

        bool operator==(Node const & other) const;
        bool operator!=(Node const & other) const;
    };

    // tags for expression operator type
    enum OP {
        EQ  = 1,
        NEQ = 2,
        OR  = 3,
        AND = 4
    };

    struct Expr {
        OP op;
        Node left, right;

        explicit operator bool() const;
    };

}  // namespace ast

struct Visitor: public boost::static_visitor<> {
    Visitor(){}

    virtual void operator()(ast::Node const& node) const {};
    virtual void operator()(ast::Expr const& expr) const {};
    virtual void operator()(ast::ExprValue const& o) const {};
    virtual void operator()(std::string const& val) const {};
    virtual void operator()(double val) const {};
    virtual void operator()(int val) const {};
};

struct EchoVisitor: public Visitor {
    std::ostream& _os;

    EchoVisitor(): Visitor(), _os(std::cout){}
    EchoVisitor(std::ostream& ss): Visitor(), _os(ss){}

    void operator()(ast::Node const& node) const override;
    void operator()(ast::Expr const& expr) const override;
    void operator()(ast::ExprValue const& o) const override;
    void operator()(std::string const& val) const override;
    void operator()(double val) const override;
    void operator()(int val) const override;
};

}  // namespace expr

static inline std::ostream& operator<<(std::ostream& os, const expr::ast::OP& op) {
    switch(op) {
        case expr::ast::OP::EQ:
            os << "==";
            break;
        case expr::ast::OP::NEQ:
            os << "!=";
            break;
        case expr::ast::OP::AND:
            os << "&&";
            break;
        case expr::ast::OP::OR:
            os << "||";
            break;
        default:
            break;
    }
    return os;
};

static inline std::ostream& operator<<(std::ostream& os, const expr::ast::Expr& expr) {
    os << "Expression[ ";
    boost::apply_visitor(expr::EchoVisitor(os) , expr.left); 
    os << " " << expr.op << " ";
    boost::apply_visitor(expr::EchoVisitor(os) , expr.right); 
    return os << " ]";
};

}  // namespace lsd

}  // namespace lava


BOOST_FUSION_ADAPT_STRUCT(lava::lsd::expr::ast::Expr, left, op, right)

namespace lava::lsd::expr::parser {
    namespace ascii = boost::spirit::x3::ascii;
    using namespace x3;

    struct OpTypesTable : x3::symbols<ast::OP> {
        OpTypesTable() {
            add ("=="   , ast::OP::EQ)
                ("!="   , ast::OP::NEQ)
                ("||"   , ast::OP::OR)
                ("&&"   , ast::OP::AND)
                ;
        }
    } const op_type;

    static auto const skipper = lexeme[ 
        "/*" >> *(char_ - "*/") >> "*/"
      | "//" >> *~char_("\r\n")
      | '#' >> *~char_("\r\n")
    ] | blank;

    auto const esc_char 
        = x3::rule<struct esc_char_, char> {"esc_char"}
        = '\\' >> char_("\"");

    auto const string 
        = x3::rule<struct string_, std::string> {"string"}
        = lexeme[+graph];

    auto const string_char
        = esc_char | alnum | graph | char_("$/_.:-+@!~");

    x3::rule<class unquoted_string_, std::string> const unquoted_string = "unquoted_string";
    auto const unquoted_string_def = //lexeme[+(~char_(" \"\'"))];
        lexeme[+string_char];
    BOOST_SPIRIT_DEFINE(unquoted_string)

    x3::rule<class empty_string_> const empty_string = "empty_string";
    auto const empty_string_def = (char_('"') >> char_('"')) | (char_('\'') >> char_('\''));
    BOOST_SPIRIT_DEFINE(empty_string)

    x3::rule<class quoted_string_, std::string> const quoted_string = "quoted_string";
    auto const quoted_string_def = 
        x3::lexeme['"' > *(esc_char | ~x3::char_('"')) > '"'] | 
        x3::lexeme['\'' > *(esc_char | ~x3::char_('"')) > '\''] |lexeme[empty_string];

    BOOST_SPIRIT_DEFINE(quoted_string)

    x3::rule<class any_string_, std::string> const any_string = "any_string";
    auto const any_string_def = unquoted_string | quoted_string;
    BOOST_SPIRIT_DEFINE(any_string)


    //auto make_expression = [](auto& ctx) { 
    //    _val(ctx).type = _attr(ctx);
    //};

    x3::rule<class node_, ast::Node> const node = "node";
    x3::rule<class expression_, ast::Expr> const expression = "expression";

    x3::rule<class expr_value_, ast::ExprValue> const expr_value = "expr_value";
    auto const expr_value_def = double_ | int_ | any_string;
    BOOST_SPIRIT_DEFINE(expr_value)

    auto const expression_def = (node >> op_type >> node);//[make_expression];
    BOOST_SPIRIT_DEFINE(expression)

    auto const node_def = expr_value | expression;
    BOOST_SPIRIT_DEFINE(node)


    using boost::fusion::at_c;

    template <ast::OP Op>
    struct make_node {
        template <typename Context >
        void operator()(Context const& ctx) const {
            if (boost::fusion::size(_attr(ctx)) == 1)
                _val(ctx) = std::move(at_c<0>(_attr(ctx)));
            else
                _val(ctx) = ast::Expr{ Op, std::move(_attr(ctx)) };
        }
    };

    //auto const op_equal
    //    = x3::rule<class op_equal_, ast::Expr>{"op_equal"}
    //    = (operand >> *("==" >> operand))[make_node<ast::OP::EQ>{}];

    //auto const op_noequal
    //    = x3::rule<class op_noequal_, ast::Expr>{"op_noequal"}
    //    = (operand >> *("!=" >> operand))[make_node<ast::OP::NEQ>{}];

    //auto const oper
    //    = op_equal | op_noequal;

    auto const oper
        = x3::rule<class op_oper_, ast::Expr>{"oper"}
        = expression;

    auto const input  = skip(skipper) [oper];

}


#endif  // SRC_LAVA_LIB_GRAMMAR_LSD_EXPR_H_
