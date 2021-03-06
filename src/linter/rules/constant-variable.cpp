#include <algorithm>
#include <linter/registry.hpp>
#include <linter/rules.hpp>

namespace {
using namespace LZN;

class ConstantVariable : public LintRule {
public:
  constexpr ConstantVariable() : LintRule(4, "constant-variable", Category::REDUNDANT) {}

private:
  using ExpressionId = MiniZinc::Expression::ExpressionId;

  virtual void do_run(LintEnv &env) const override {
    for (const MiniZinc::VarDecl *vd : env.user_defined_variable_declarations()) {
      const MiniZinc::Expression *rhs = nullptr;
      if ((rhs = vd->e()) == nullptr)
        rhs = env.get_equal_constrained_rhs(vd);

      if (rhs != nullptr && vd->type().isvar() && rhs->type().isPar()) {
        auto &loc = vd->loc();
        auto &res = env.emplace_result(FileContents::Type::OneLineMarked, loc, this,
                                       "is only assigned to par values, shouldn't be var");
        const auto &rhs_loc = rhs->loc();
        if (!rhs_loc.isIntroduced())
          res.emplace_subresult("assigned here", FileContents::Type::OneLineMarked, rhs_loc);

      } else if (rhs == nullptr && vd->ti()->isarray() && env.is_every_index_touched(vd)) {
        std::vector<const MiniZinc::Location *> sub_locations;
        auto [first, last] = env.array_equal_constrained().equal_range(vd);
        bool all_par = std::all_of(first, last, [&sub_locations](auto &pair) {
          auto [arrayaccess, rhs, comp] = pair.second;
          sub_locations.push_back(&arrayaccess->loc());
          return rhs->type().isPar();
        });

        if (all_par) {
          auto &loc = vd->loc();
          LintResult lr(FileContents::Type::OneLineMarked, loc, this,
                        "is only constrained to par values, shouldn't be var");

          for (auto sloc : sub_locations) {
            lr.emplace_subresult("constrained here", FileContents::Type::OneLineMarked, *sloc);
          }

          env.add_result(std::move(lr));
        }
      }
    }
  }
};

} // namespace

REGISTER_RULE(ConstantVariable)
