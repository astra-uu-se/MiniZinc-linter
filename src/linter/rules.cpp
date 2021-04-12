#include "rules.hpp"
#include <linter/file_utils.hpp>
#include <linter/overload.hpp>
#include <linter/utils.hpp>

namespace {
template <typename T, typename F>
const T &lazy_value(std::optional<T> &opt, F f) {
  if (!opt) {
    opt = f();
  }
  return *opt;
}
} // namespace

namespace LZN {
std::ostream &operator<<(std::ostream &os, const LintResult &value) {
  os << "(" << value.rule->name << ")";
  std::visit(overload{[&](const std::monostate &) { os << "None"; },
                      [&](const LintResult::OneLineMarked &olm) {
                        os << "OLM{" << olm.line << "," << olm.startcol << "," << olm.endcol << "}";
                      },
                      [&](const LintResult::MultiLine &ml) {
                        os << "ML{" << ml.startline << "," << ml.endline << "}";
                      }},
             value.region);
  return os;
}

void LintEnv::add_result(LintResult lr) {
  _results.push_back(std::move(lr));
}

const LintEnv::ECMap &LintEnv::equal_constrained() {
  return lazy_value(_equal_constrained, [this, model = _model]() {
    const auto s = get_builder()
                       .global_filter(filter_out_annotations)
                       // TODO: look for constraints in let
                       // .in_vardecl()
                       // .in_assign_rhs()
                       // .in_function_body()
                       .in_constraint()
                       .direct(MiniZinc::BinOpType::BOT_EQ)
                       .capture()
                       .direct(MiniZinc::Expression::E_ID)
                       .build();
    auto ms = s.search(model);

    LintEnv::ECMap ids;
    while (ms.next()) {
      auto eq = ms.capture(0)->cast<MiniZinc::BinOp>();
      MiniZinc::Id *left;
      if ((left = eq->lhs()->dynamicCast<MiniZinc::Id>()) != nullptr) {
        assert(left->decl() != nullptr);
        ids.emplace(left->decl(), eq->rhs());
      }
      MiniZinc::Id *right;
      if ((right = eq->rhs()->dynamicCast<MiniZinc::Id>()) != nullptr) {
        assert(right->decl() != nullptr);
        ids.emplace(right->decl(), eq->lhs());
      }
    }
    return ids;
  });
}

const LintEnv::VDVec &LintEnv::user_defined_variable_declarations() {
  using ExpressionId = MiniZinc::Expression::ExpressionId;
  return lazy_value(_vardecls, [this, model = _model]() {
    const auto s = get_builder()
                       .in_vardecl()
                       .in_assign_rhs()
                       .in_constraint()
                       .in_function_body()
                       .under(ExpressionId::E_VARDECL)
                       .capture()
                       .build();
    auto ms = s.search(model);

    MiniZinc::VarDecl *solve_var = nullptr;
    if (auto si = solve_item(); si != nullptr && si->e() != nullptr) {
      auto id = si->e()->dynamicCast<MiniZinc::Id>();
      solve_var = id->decl();
    }

    LintEnv::VDVec vec;
    while (ms.next()) {
      // ignore enum functions from stdlib
      if (auto fi = ms.cur_item()->dynamicCast<MiniZinc::FunctionI>();
          fi != nullptr && (fi->fromStdLib() || fi->loc().isIntroduced())) {
        ms.skip_item();
        continue;
      }

      auto vd = ms.capture(0)->cast<MiniZinc::VarDecl>();

      // ignore enum definitions from stdlib, but keep _objective
      if ((solve_var == nullptr || solve_var != vd) &&
          !is_user_defined(_includePath, vd->loc().filename())) {
        continue;
      }

      vec.push_back(vd);
    }
    return vec;
  });
}

const LintEnv::AECMap &LintEnv::array_equal_constrained() {
  return lazy_value(_array_equal_constrained, [this, model = _model]() {
    LintEnv::AECMap map;

    {
      const auto s = get_builder()
                         .in_constraint()
                         .direct(MiniZinc::Expression::E_CALL)
                         .capture()
                         .direct(MiniZinc::Expression::E_COMP)
                         .capture()
                         .filter(filter_comprehension_expr)
                         .direct(MiniZinc::BinOpType::BOT_EQ)
                         .capture()
                         .direct(MiniZinc::Expression::E_ARRAYACCESS)
                         .capture()
                         .filter(filter_arrayaccess_name)
                         .direct(MiniZinc::Expression::E_ID)
                         .capture()
                         .build();
      auto forallsearcher = s.search(model);

      while (forallsearcher.next()) {
        const auto forall = forallsearcher.capture_cast<MiniZinc::Call>(0);
        if (forall->id() != MiniZinc::constants().ids.forall)
          continue;

        const auto comp = forallsearcher.capture_cast<MiniZinc::Comprehension>(1);
        const auto eq = forallsearcher.capture_cast<MiniZinc::BinOp>(2);
        const auto access = forallsearcher.capture_cast<MiniZinc::ArrayAccess>(3);
        const auto decl = forallsearcher.capture_cast<MiniZinc::Id>(4)->decl();
        assert(decl != nullptr);
        const MiniZinc::Expression *rhs = eq->rhs() == access ? eq->lhs() : eq->rhs();

        map.emplace(decl, AECValue{access, rhs, comp});
      }
    }

    {
      const auto s = get_builder()
                         .in_constraint()
                         .direct(MiniZinc::BinOpType::BOT_EQ)
                         .capture()
                         .direct(MiniZinc::Expression::E_ARRAYACCESS)
                         .capture()
                         .filter(filter_arrayaccess_name)
                         .direct(MiniZinc::Expression::E_ID)
                         .capture()
                         .build();
      auto individualsearcher = s.search(model);

      while (individualsearcher.next()) {
        const auto eq = individualsearcher.capture_cast<MiniZinc::BinOp>(0);
        const auto access = individualsearcher.capture_cast<MiniZinc::ArrayAccess>(1);
        const auto decl = individualsearcher.capture_cast<MiniZinc::Id>(2)->decl();
        const MiniZinc::Expression *rhs = eq->rhs() == access ? eq->lhs() : eq->rhs();
        assert(decl != nullptr);
        map.emplace(decl, AECValue{access, rhs, nullptr});
      }
    }

    return map;
  });
}

const LintEnv::UDFVec &LintEnv::user_defined_functions() {
  return lazy_value(_user_defined_funcs, [this, model = _model]() {
    const auto s = get_builder().in_function().build();
    auto ms = s.search(model);
    LintEnv::UDFVec vec;
    while (ms.next()) {
      auto fi = ms.cur_item()->cast<MiniZinc::FunctionI>();
      if (!fi->fromStdLib() && !fi->loc().isIntroduced())
        vec.push_back(fi);
    }
    return vec;
  });
}

const MiniZinc::SolveI *LintEnv::solve_item() {
  return lazy_value(_solve_item, [this, model = _model]() -> const MiniZinc::SolveI * {
    const auto s = get_builder().in_solve().build();
    auto ms = s.search(model);
    while (ms.next()) {
      return ms.cur_item()->cast<MiniZinc::SolveI>();
    }
    return nullptr;
  });
}

const MiniZinc::Expression *LintEnv::get_equal_constrained_rhs(const MiniZinc::VarDecl *vd) {
  const auto &map = equal_constrained();
  auto it = map.find(vd);
  if (it != map.end()) {
    assert(it->second != nullptr);
    return it->second;
  }
  return nullptr;
}

bool LintEnv::is_every_index_touched(const MiniZinc::VarDecl *arraydecl) {
  auto [first, last] = array_equal_constrained().equal_range(arraydecl);
  if (first == last)
    return false;

  std::vector<const MiniZinc::Expression *> ranges;
  for (auto x : arraydecl->ti()->ranges()) {
    ranges.push_back(x->domain());
  }

  std::vector<const MiniZinc::Expression *> comp_ranges;
  for (; first != last; ++first) {
    auto [arrayaccess, rhs, comp] = first->second;
    if (comp == nullptr)
      continue;

    comp_ranges.clear();
    for (unsigned int gen = 0; gen < comp->numberOfGenerators(); ++gen) {
      if (comp->where(gen) != nullptr)
        continue;

      auto in = comp->in(gen);
      for (unsigned int i = 0; i < comp->numberOfDecls(gen); ++i) {
        comp_ranges.push_back(in);
      }
    }

    bool same_ranges =
        unsorted_equal_cmp(ranges.begin(), ranges.end(), comp_ranges.begin(), comp_ranges.end(),
                           [](const MiniZinc::Expression *r, const MiniZinc::Expression *cr) {
                             return MiniZinc::Expression::equal(r, cr);
                           });
    if (same_ranges)
      return true;
  }
  return false;
}

SearchBuilder LintEnv::get_builder() const {
  return SearchBuilder().only_user_defined(_includePath).recursive();
}

} // namespace LZN
