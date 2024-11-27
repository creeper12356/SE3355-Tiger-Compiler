#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  return std::max(stm1->MaxArgs(), stm2->MaxArgs());
}

Table *A::CompoundStm::Interp(Table *t) const {
  t = stm1->Interp(t);
  t = stm2->Interp(t);
  return t;
}

int A::AssignStm::MaxArgs() const { return exp->MaxArgs(); }

Table *A::AssignStm::Interp(Table *t) const {
  IntAndTable *res = exp->Interp(t);
  int val = res->i;
  t = res->t;
  delete res;

  if(!t) {
    return new Table(id, val, nullptr);
  } else {
    return t->Update(id, val);
  }
}

int A::PrintStm::MaxArgs() const {
  return std::max(exps->NumExps(), exps->MaxArgs());
}

Table *A::PrintStm::Interp(Table *t) const {
  IntAndTable *res = exps->Interp(t);
  t = res->t;
  delete res;

  return t;
}

int A::IdExp::MaxArgs() const { return 0; }

IntAndTable *A::IdExp::Interp(Table *t) const {
  int val = t->Lookup(id);
  return new IntAndTable(val, t);
}

int A::NumExp::MaxArgs() const { return 0; }

IntAndTable *A::NumExp::Interp(Table *t) const {
  return new IntAndTable(num, t);
}

int A::OpExp::MaxArgs() const {
  return std::max(left->MaxArgs(), right->MaxArgs());
}

IntAndTable *A::OpExp::Interp(Table *t) const {
  IntAndTable *leftRes = left->Interp(t);
  int leftVal = leftRes->i;
  t = leftRes->t;
  delete leftRes;

  IntAndTable *rightRes = right->Interp(t);
  int rightVal = rightRes->i;
  t = rightRes->t;
  delete rightRes;

  return new IntAndTable(Calculate(leftVal, this->oper, rightVal), t);
}

int A::OpExp::Calculate(int leftVal, BinOp oper, int rightVal) {
  switch (oper) {
  case PLUS:
    return leftVal + rightVal;
  case MINUS:
    return leftVal - rightVal;
  case TIMES:
    return leftVal * rightVal;
  case DIV:
  default:
    return leftVal / rightVal;
  }
}
int A::EseqExp::MaxArgs() const {
  return std::max(exp->MaxArgs(), stm->MaxArgs());
}

IntAndTable *A::EseqExp::Interp(Table *t) const {
  t = stm->Interp(t);
  return exp->Interp(t);
}

int A::PairExpList::MaxArgs() const {
  return std::max(exp->MaxArgs(), tail->MaxArgs());
}

int A::PairExpList::NumExps() const { return tail->NumExps() + 1; }

IntAndTable *A::PairExpList::Interp(Table *t) const {
  IntAndTable *res = exp->Interp(t);
  int val = res->i;
  t = res->t;
  delete res;

  std::cout << val << ' ';
  return tail->Interp(t);
}

int A::LastExpList::MaxArgs() const { return exp->MaxArgs(); }

int A::LastExpList::NumExps() const { return 1; }

IntAndTable *A::LastExpList::Interp(Table *t) const {
  IntAndTable *res = exp->Interp(t);
  std::cout << res->i << std::endl;
  return res;
}

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
} // namespace A
