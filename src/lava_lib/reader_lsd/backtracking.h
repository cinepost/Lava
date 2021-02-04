#ifndef SRC_LAVA_LIB_BACKTRACKING_H_
#define SRC_LAVA_LIB_BACKTRACKING_H_

#include <iostream>
#include <boost/spirit/home/x3.hpp>


namespace x3 = boost::spirit::x3;

namespace lava {

/*
template <class Left, class Right>
struct backtracking : x3::binary_parser<Left, Right, backtracking<Left, Right>> {
    using base = x3::binary_parser<Left, Right, backtracking<Left, Right>>;
    backtracking(Left const& left, Right const& right):  base(left, right) {}

    template<typename It, typename Ctx, typename Other>
    bool parse(It& f, It l, Ctx const& ctx, Other const& other, x3::unused_type) const {
        std::cout << "---\n" << std::string(f, l) << "\n---\n";

        auto end_it = l;
        while (end_it != f) {
            auto save = f;
            if (this->left.parse(f, end_it, ctx, other, x3::unused)) {
                if (this->right.parse(f, l, ctx, other, x3::unused))
                    return true;
            }
            f = save;
            ++end_it;
        }
        return false;
    }
};
*/

template <class Left, class Right>
struct backtracking : x3::binary_parser<Left, Right, backtracking<Left, Right>> {
    using base = x3::binary_parser<Left, Right, backtracking<Left, Right>>;
    backtracking(Left const& left, Right const& right):  base(left, right) {}

    template<typename It, typename Ctx, typename Other>
    bool parse(It& f, It l, Ctx const& ctx, Other const& other, x3::unused_type) const {
        uint oc = 0; // open brackets count
        uint cc = 0; // closing brackets count
        while (f != l) {
            if (*f == '[') ++oc;
            else if(*f == ']') ++cc;
            ++f;

            if((oc > 0) && (oc == cc)) return true;
        }

        return false;
    }
};


/* usage example
https://stackoverflow.com/questions/61058763/boost-spirit-x3-how-to-prevent-token-from-being-parsed-by-previous-rule

const auto grammar_def = x3::lit("start")
    > x3::lit("{")
    > backtracking(
    *(char_("a-zA-Z0-9\".{}=_~")), lit("}") >> lit(";"));

*/

}  // namespace lava


#endif  // SRC_LAVA_LIB_BACKTRACKING_H_