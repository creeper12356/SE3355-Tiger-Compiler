#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) {
  root_->Traverse(env, 0);
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  auto esc_entry = env->Look(sym_);
  assert(esc_entry);
  if (esc_entry->depth_ < depth) {
    *esc_entry->escape_ = true;
  }
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
  subscript_->Traverse(env, depth);
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {
}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {
}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {
}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  auto &args_list = args_->GetList();
  for(auto &arg: args_list) {
    arg->Traverse(env, depth);
  }
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  left_->Traverse(env, depth);
  right_->Traverse(env, depth);
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  auto &efield_list = fields_->GetList();
  for(auto &efield: efield_list) {
    efield->exp_->Traverse(env, depth);
  }
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  auto &exp_list = seq_->GetList();
  for(auto &exp: exp_list) {
    exp->Traverse(env, depth);
  }
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
  exp_->Traverse(env, depth);
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  test_->Traverse(env, depth);
  then_->Traverse(env, depth);
  if(elsee_) {
    // NOTE: else可选
    elsee_->Traverse(env, depth);
  }
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  test_->Traverse(env, depth);
  body_->Traverse(env, depth);
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  // NOTE: for的循环变量需要加入环境
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));

  lo_->Traverse(env, depth);
  hi_->Traverse(env, depth);
  body_->Traverse(env, depth);
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {
}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  auto &dec_list = decs_->GetList();
  for(auto &dec: dec_list) {
    dec->Traverse(env, depth);
  }

  body_->Traverse(env, depth);
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  size_->Traverse(env, depth);
  init_->Traverse(env, depth);
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {
}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  auto& fun_dec_list = functions_->GetList();
  for(auto &fun_dec: fun_dec_list) {
    auto &field_list = fun_dec->params_->GetList();
    for(auto &field: field_list) {
      env->Enter(field->name_, new esc::EscapeEntry(depth + 1, &field->escape_));
    }
  }
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  init_->Traverse(env, depth);
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
}

} // namespace absyn
