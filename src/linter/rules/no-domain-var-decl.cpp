#include <linter/registry.hpp>
#include <linter/rules.hpp>

namespace {
using namespace LZN;

bool isNoDomainVar(const MiniZinc::VarDecl &vd) {
  auto &t = vd.type();
  auto domain = vd.ti()->domain();
  return t.isvar() && t.st() == MiniZinc::Type::SetType::ST_PLAIN &&
         (t.bt() == MiniZinc::Type::BaseType::BT_INT ||
          t.bt() == MiniZinc::Type::BaseType::BT_FLOAT) &&
         t.dim() >= 0 && t.isPresent() && domain == nullptr;
}

bool isGeneratorVar(const MiniZinc::VarDecl *vd, LintEnv &env) {
  auto comps = env.comprehensions();
  for (const MiniZinc::Comprehension *c : comps) {
    for (unsigned int gen = 0; gen < c->numberOfGenerators(); gen++) {
      for (unsigned int decl = 0; decl < c->numberOfDecls(gen); decl++) {
        if (vd == c->decl(gen, decl))
          return true;
      }
    }
  }
  return false;
}

class NoDomainVarDecl : public LintRule {
public:
  constexpr NoDomainVarDecl() : LintRule(13, "unbounded-variable", Category::PERFORMANCE) {}

private:
  virtual void do_run(LintEnv &env) const override {
    for (const MiniZinc::VarDecl *vd : env.user_defined_variable_declarations()) {
      if (isNoDomainVar(*vd) && vd->e() == nullptr &&
          env.get_equal_constrained_rhs(vd) == nullptr && !isGeneratorVar(vd, env)) {
        auto &loc = vd->loc();
        env.emplace_result(FileContents::Type::OneLineMarked, loc, this,
                           "no explicit domain on variable declaration");
      }
    }
  }
};

} // namespace

REGISTER_RULE(NoDomainVarDecl)
