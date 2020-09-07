#ifndef SRC_LAVA_LIB_GRAMMAR_LSD_H_
#define SRC_LAVA_LIB_GRAMMAR_LSD_H_

#include <memory>
#include <string>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/adapted/struct.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>

#include "renderer_iface_lsd.h"


namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

typedef std::string::const_iterator Iterator;

namespace lava {

template<typename IteratorT>
class SkipperParser : public qi::grammar<IteratorT> {
 public:
  SkipperParser():SkipperParser::base_type( rule ) {
    lineCommentRule1 = qi::lit( "//" ) >> 
                       *(qi::char_ -qi::eol) >> 
                       qi::eol;
    lineCommentRule2 = qi::lit( "#" ) >> 
                       *(qi::char_ -qi::eol) >> 
                       qi::eol;
    blockCommentRule = qi::lit( "/*" ) >> 
                       *(qi::char_ -qi::lit( "*/" ) ) >> 
                       qi::lit( "*/" );
    spaceRule        = qi::space;
    rule             = spaceRule | lineCommentRule1 | lineCommentRule2 | blockCommentRule;
  }

  qi::rule<IteratorT> lineCommentRule1;
  qi::rule<IteratorT> lineCommentRule2;
  qi::rule<IteratorT> blockCommentRule;
  qi::rule<IteratorT> spaceRule;
  qi::rule<IteratorT> rule;
};

namespace Ast {
    struct NoValue {
        bool operator==(NoValue const &) const { return true; }
        friend std::ostream& operator<<(std::ostream& os, NoValue) { return os; }
    };
    template <typename Tag> struct GenericCommand {};

    namespace tag {
        struct setenv {};
        struct cmd_version {}; 
        struct cmd_declare {};
        struct cmd_start {};
        struct cmd_end {};
        struct cmd_property {}; 
        struct cmd_detail {};
        struct cmd_image {};
        struct cmd_geomerty {};
        struct cmd_time {};
        struct cmd_bgeo {};
        struct cmd_raytrace {};
        struct cmd_quit {};
        struct cmd_defaults {};

        static std::ostream& operator<<(std::ostream& os, setenv) { return os << "setenv"; }
        static std::ostream& operator<<(std::ostream& os, cmd_version) { return os << "cmd_version"; }
        static std::ostream& operator<<(std::ostream& os, cmd_declare) { return os << "cmd_declare"; }
        static std::ostream& operator<<(std::ostream& os, cmd_start) { return os << "cmd_start"; }
        static std::ostream& operator<<(std::ostream& os, cmd_end) { return os << "cmd_end"; }
        static std::ostream& operator<<(std::ostream& os, cmd_property) { return os << "cmd_property"; }
        static std::ostream& operator<<(std::ostream& os, cmd_detail) { return os << "cmd_detail"; }
        static std::ostream& operator<<(std::ostream& os, cmd_image) { return os << "cmd_image"; }
        static std::ostream& operator<<(std::ostream& os, cmd_geomerty) { return os << "cmd_geomerty"; }
        static std::ostream& operator<<(std::ostream& os, cmd_time) { return os << "cmd_time"; }
        static std::ostream& operator<<(std::ostream& os, cmd_bgeo) { return os << "cmd_bgeo"; }
        static std::ostream& operator<<(std::ostream& os, cmd_raytrace) { return os << "cmd_raytrace"; }
        static std::ostream& operator<<(std::ostream& os, cmd_quit) { return os << "cmd_quit"; }
        static std::ostream& operator<<(std::ostream& os, cmd_defaults) { return os << "cmd_defaults"; }
    };

    //template<> struct GenericCommand<tag::load> { std::string name; };

    
    template<> struct GenericCommand<tag::setenv> {
        std::string key;
        std::string value;
    };

    template<> struct GenericCommand<tag::cmd_detail> {
        std::string key;
        std::string value;
    };

    template<> struct GenericCommand<tag::cmd_time> {
        boost::variant<float, int> value;
    };
    
    template<> struct GenericCommand<tag::cmd_version> {
        std::vector<uint> value;
    };
    

    //template <> struct GenericCommand<tag::set> {
    //    std::string property;
    //    boost::variant<NoValue, std::string, bool> value; // optional
    //};

    using Setenv        = GenericCommand<tag::setenv>;
    using CmdVersion    = GenericCommand<tag::cmd_version>;
    using CmdDeclare    = GenericCommand<tag::cmd_declare>;
    using CmdStart      = GenericCommand<tag::cmd_start>;
    using CmdEnd        = GenericCommand<tag::cmd_end>;
    using CmdProperty   = GenericCommand<tag::cmd_property>;
    using CmdDetail     = GenericCommand<tag::cmd_detail>;
    using CmdImage      = GenericCommand<tag::cmd_image>;
    using CmdGeometry   = GenericCommand<tag::cmd_geomerty>;
    using CmdTime       = GenericCommand<tag::cmd_time>;
    using CmdBgeo       = GenericCommand<tag::cmd_bgeo>;
    using CmdRaytrace   = GenericCommand<tag::cmd_raytrace>;
    using CmdQuit       = GenericCommand<tag::cmd_quit>;
    using CmdDefaults   = GenericCommand<tag::cmd_defaults>;

    using Command = boost::variant<
        Setenv, CmdVersion, CmdQuit, CmdTime, CmdRaytrace
        //CmdDeclare, CmdStart, CmdEnd, CmdProperty, CmdDetail, 
        //CmdImage, CmdGeometry, CmdBgeo, CmdDefaults
    >;
    
    using Commands = std::list<Command>;

    template <typename Tag>
    static inline std::ostream& operator<<(std::ostream& os, Ast::GenericCommand<Tag> const& command) { 
        return os << Tag{} << " " << boost::fusion::as_vector(command);
    }
}

}  // namespace lava

BOOST_FUSION_ADAPT_TPL_STRUCT((Tag), (lava::Ast::GenericCommand) (Tag), )
BOOST_FUSION_ADAPT_STRUCT(lava::Ast::Setenv, key, value)
BOOST_FUSION_ADAPT_STRUCT(lava::Ast::CmdTime, value)
BOOST_FUSION_ADAPT_STRUCT(lava::Ast::CmdVersion, value)
BOOST_FUSION_ADAPT_STRUCT(lava::Ast::CmdQuit)
BOOST_FUSION_ADAPT_STRUCT(lava::Ast::CmdRaytrace)

namespace lava {

template <typename Iterator>
struct command_grammar : qi::grammar<Iterator, Ast::Commands()> {
    command_grammar() : command_grammar::base_type(start) {
        //using namespace qi;

        start = qi::skip(qi::blank) [command % qi::eol];

        // nabialek trick
        command = qi::no_case [ commands ];

        on_off.add("on", true)("off", false);

        commands.add
            ("cmd_version", &cmd_version) ("cmd_quit", &cmd_quit) 
            ("cmd_time", &cmd_time) ("cmd_raytrace", &cmd_raytrace)
            ("setenv", &setenv);

        quoted_string = '"' >> +~qi::char_('"') >> '"';

        version = qi::int_ >> '.' >> qi::int_ >> '.' >> qi::int_; 

        env_variable = quoted_string >> "=" >> quoted_string;

        // nullary commands
        cmd_quit_ = qi::eps;
        cmd_raytrace_ = qi::eps;
        cmd_time = qi::eps;
        cmd_version = qi::eps;

        // non-nullary commands
        setenv_ = quoted_string >> "=" >> quoted_string;
        //cmd_version_ = qi::raw[qi::lexeme["VEX" >> +version]];
        //cmd_version_ = "VEX" >> +version;
        //cmd_time_ = qi::int_ | qi::float_;
        
        //load_command_ = quoted_string;
        //drive_        = char_("A-Z") >> ':';
        //set_command_  = no_case[lit("drive")|"driv"|"dri"|"dr"] >> attr("DRIVE") >> drive_
        //       | no_case[ (lit("debug")|"debu"|"deb"|"de")     >> attr("DEBUG") >> on_off ]
        //        | no_case[ (lit("trace")|"trac"|"tra"|"tr"|"t") >> attr("TRACE") >> on_off ]
        //        ;

        BOOST_SPIRIT_DEBUG_NODES(
            (start)(command)
            (cmd_version) (cmd_quit_) (cmd_raytrace) (cmd_time)
            (cmd_version_)(cmd_quit_)(cmd_raytrace_)(cmd_time_)
            (setenv)
            (setenv_)
            (quoted_string)(version)
        )

        //on_error<fail>(start, error_handler_(_4, _3, _2));
        //on_error<fail>(command, error_handler_(_4, _3, _2));

        setenv = setenv_;
        cmd_version = cmd_version_;
        cmd_quit = cmd_quit_;
        cmd_raytrace = cmd_raytrace_;
        cmd_time = cmd_time_;

    }

  private:
    struct error_handler_t {
        template <typename...> struct result { typedef void type; };

        void operator()(qi::info const &What, Iterator Err_pos, Iterator Last) const {
            std::cout << "Error! Expecting " << What << " here: \"" << std::string(Err_pos, Last) << "\"" << std::endl;
        }
    };

    //phoenix::function<error_handler_t> const error_handler_ = error_handler_t {};

    qi::rule<Iterator, Ast::Commands()> start;

    using Skipper = qi::blank_type;
    using CommandRule  = qi::rule<Iterator, Ast::Command(), Skipper>;

    qi::symbols<char, bool> on_off;
    qi::symbols<char, CommandRule const*> commands;

    qi::rule<Iterator, std::string()> quoted_string;
    qi::rule<Iterator, std::tuple<int, int, int>()> version;
    qi::rule<Iterator, std::string(), std::string()> env_variable;

    qi::rule<Iterator, Ast::Command(), Skipper, qi::locals<CommandRule const*> > command;
    CommandRule cmd_version, cmd_quit, cmd_time, cmd_raytrace, setenv;

    qi::rule<Iterator, Ast::CmdVersion(), Skipper> cmd_version_;
    qi::rule<Iterator, Ast::CmdQuit(), Skipper> cmd_quit_;
    qi::rule<Iterator, Ast::CmdTime(), Skipper> cmd_time_;
    qi::rule<Iterator, Ast::CmdRaytrace(), Skipper> cmd_raytrace_;
    qi::rule<Iterator, Ast::Setenv(), Skipper> setenv_;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_GRAMMAR_LSD_H_