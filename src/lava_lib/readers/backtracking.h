#ifndef SRC_LAVA_LIB_BACKTRACKING_H_
#define SRC_LAVA_LIB_BACKTRACKING_H_

#include <boost/spirit/home/x3.hpp>


namespace x3 = boost::spirit::x3;

namespace lava {

template <class Left, class Right>
struct backtracking : x3::binary_parser<Left, Right, backtracking<Left, Right>> {
    using base = x3::binary_parser<Left, Right, backtracking<Left, Right>>;
    backtracking(Left const& left, Right const& right):  base(left, right) {}

    template<typename It, typename Ctx, typename Other>
    bool parse(It& f, It l, Ctx const& ctx, Other const& other, x3::unused_type) const {
        auto end_it = l;
        while (end_it != f) {
            auto save = f;
            if (this->left.parse(f, end_it, ctx, other, x3::unused)) {
                if (this->right.parse(f, l, ctx, other, x3::unused))
                    return true;
            }
            f = save;
            --end_it;
        }
        return false;
    }
};

}  // namespace lava


#endif  // SRC_LAVA_LIB_BACKTRACKING_H_