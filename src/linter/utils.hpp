#pragma once

#include <minizinc/eval_par.hh>
#include <vector>

namespace LZN {
template <typename Cmp, typename It, typename ItModify>
bool unsorted_equal_cmp(ItModify modbeg, ItModify modend, It beg, It end, Cmp cmp) {
  ItModify first_unsure = modbeg;
  for (; beg != end; ++beg) {
    ItModify old_fu = first_unsure;

    for (ItModify i = first_unsure; i != modend; ++i) {
      if (cmp(*i, *beg)) {
        if (i != first_unsure)
          std::swap(*i, *first_unsure);
        ++first_unsure;
        break;
      }
    }

    if (old_fu == first_unsure)
      return false;
  }

  return first_unsure == modend;
}

inline const MiniZinc::Expression *follow_id(const MiniZinc::Expression *e) {
  return MiniZinc::follow_id(const_cast<MiniZinc::Expression *>(e));
}
inline const MiniZinc::Expression *follow_id_to_decl(const MiniZinc::Expression *e) {
  return MiniZinc::follow_id_to_decl(const_cast<MiniZinc::Expression *>(e));
}

inline bool is_int_expr(const MiniZinc::Expression *e, long long int i) {
  assert(e != nullptr);
  if (auto intlit = e->dynamicCast<MiniZinc::IntLit>(); intlit != nullptr) {
    return intlit->v() == MiniZinc::IntVal(i);
  }
  return false;
}

inline bool is_float_expr(const MiniZinc::Expression *e, double f) {
  assert(e != nullptr);
  if (auto floatlit = e->dynamicCast<MiniZinc::FloatLit>(); floatlit != nullptr) {
    return floatlit->v() == MiniZinc::FloatVal(f);
  }
  return false;
}
} // namespace LZN
