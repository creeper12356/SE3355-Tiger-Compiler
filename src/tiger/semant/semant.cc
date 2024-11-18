#include "tiger/semant/semant.h"
#include "tiger/absyn/absyn.h"

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
      return (static_cast<env::VarEntry *>(entry))->ty_->ActualTy();
  } else {
      errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
  }
  return type::IntTy::Instance();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  auto var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if(!var_ty || typeid(*var_ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }
  auto record_field_list = (static_cast<type::RecordTy *>(var_ty))->fields_->GetList();
  for(auto field = record_field_list.begin(); field != record_field_list.end(); field++) {
    if((*field)->name_->Name() == sym_->Name()) {
      return (*field)->ty_->ActualTy();
    }
  }

  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  return type::IntTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  auto var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if(!var_ty || typeid(*var_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return type::IntTy::Instance();
  }

  auto subscript_ty = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if(!subscript_ty || typeid(*subscript_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "subscript must be an integer");
    return type::IntTy::Instance();
  }

  return (static_cast<type::ArrayTy *>(var_ty))->ty_->ActualTy();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg);                
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  auto func_entry = venv->Look(func_);
  if(!func_entry || typeid(*func_entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return type::IntTy::Instance();
  }

  // Check formals
  auto arg_list = args_->GetList();
  auto formal_list = (static_cast<env::FunEntry *>(func_entry))->formals_->GetList();
  auto arg_it = arg_list.begin();
  auto formal_it = formal_list.begin();

  while(arg_it != arg_list.end() && formal_it != formal_list.end()) {
    auto arg_ty = (*arg_it)->SemAnalyze(venv, tenv, labelcount, errormsg);
    auto formal_ty = (*formal_it)->ActualTy();
    if(!arg_ty->IsSameType(formal_ty)) {
      errormsg->Error(pos_, "paramter type mismatch");
      return type::IntTy::Instance();
    }
    arg_it++;
    formal_it++;
    if(arg_it != arg_list.end() && formal_it == formal_list.end()) {
      errormsg->Error(pos_, "too many parameters in function %s", func_->Name().data());
      return type::IntTy::Instance();
    }
    if(arg_it == arg_list.end() && formal_it != formal_list.end()) {
      errormsg->Error(pos_, "too few parameters in function %s", func_->Name().data());
      return type::IntTy::Instance();
    }
  }

  return (static_cast<env::FunEntry *>(func_entry))->result_->ActualTy();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *left_ty = left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *right_ty=right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (oper_ == absyn::PLUS_OP || oper_ == absyn::MINUS_OP || 
      oper_ == absyn::TIMES_OP || oper_ == absyn::DIVIDE_OP) {
    if (typeid(*left_ty) != typeid(type::IntTy)) {
      errormsg->Error(left_->pos_,"integer required");
    }
    if (typeid(*right_ty) != typeid(type::IntTy)) {
      errormsg->Error(right_->pos_,"integer required");
    }
    return type::IntTy::Instance();  
  } else {
    if (!left_ty->IsSameType(right_ty)) {
      errormsg->Error(pos_, "same type required");
      return type::IntTy::Instance();
    }
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  auto entry = tenv->Look(typ_);
  if(!entry) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }

  auto record_ty = entry->ActualTy();
  if(!record_ty || typeid(*record_ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }

  return record_ty;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  auto exp_list = seq_->GetList();
  type::Ty *result = type::VoidTy::Instance();
  for(auto exp = exp_list.begin(); exp != exp_list.end(); exp++) {
    result = (*exp)->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  return result;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  if (typeid(*var_) != typeid(SimpleVar)) {
    errormsg->Error(pos_, "lvalue required");
    return type::IntTy::Instance();
  }

  auto var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto exp_ty = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if(!var_ty->IsSameType(exp_ty)) {
    errormsg->Error(pos_, "unmatched assign exp");
  }

  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  auto test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "integer required");
  }

  auto then_ty = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if(elsee_ == nullptr) {
    if (typeid(*then_ty) != typeid(type::VoidTy)) {
      errormsg->Error(then_->pos_, "if-then exp's body must produce no value");
    }
    return type::VoidTy::Instance();
  } 

  auto elsee_ty = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!then_ty->IsSameType(elsee_ty)) {
    errormsg->Error(pos_, "then exp and else exp type mismatch");
    return type::IntTy::Instance();
  }

  return then_ty;
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  auto test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "integer required");
  }

  auto body_ty = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*body_ty) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "while body must produce no value");
    return type::IntTy::Instance();
  }

  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  auto lo_ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto hi_ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*lo_ty) != typeid(type::IntTy)) {
    errormsg->Error(lo_->pos_, "for exp's range type is not integer");
  }
  if (typeid(*hi_ty) != typeid(type::IntTy)) {
    errormsg->Error(hi_->pos_, "for exp's range type is not integer");
  }

  venv->BeginScope();
  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance()));
  auto body_ty = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  venv->EndScope();

  if (typeid(*body_ty) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "for body must produce no value");
    return type::IntTy::Instance();
  }

  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  if (labelcount == 0) {
    errormsg->Error(pos_, "break statement not within loop");
  }

  return type::VoidTy::Instance();                 
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();
  for (Dec *dec : decs_->GetList())
      dec->SemAnalyze(venv, tenv, labelcount, errormsg);

  type::Ty *result;
  if (!body_) 
      result = type::VoidTy::Instance();
  else 
      result = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  
  tenv->EndScope();
  venv->EndScope();
  return result;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  auto entry = tenv->Look(typ_);
  if (!entry) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }

  if(typeid(*entry->ActualTy()) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "not an array type");
    return type::IntTy::Instance();
  }

  auto array_ty = static_cast<type::ArrayTy *>(entry->ActualTy());
  auto array_size_ty = size_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (typeid(*array_size_ty) != typeid(type::IntTy)) {
    errormsg->Error(size_->pos_, "array size must be an integer");
    return type::VoidTy::Instance();
  }
  auto array_init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!array_init_ty->IsSameType(array_ty->ty_)) {
    errormsg->Error(init_->pos_, "type mismatch");
    return type::VoidTy::Instance();
  }

  return array_ty;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  auto entry = tenv->Look(name_);
  if (!entry) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().data());
    return type::VoidTy::Instance();
  }

  return entry;
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  auto field_list = record_->GetList();
  auto field_ty_list = new type::FieldList();
  for(auto field = field_list.begin(); field != field_list.end(); field++) {
    auto entry = tenv->Look((*field)->typ_);
    if (!entry) {
      errormsg->Error(pos_, "undefined type %s", (*field)->typ_->Name().data());
      return type::VoidTy::Instance();
    }
    field_ty_list->Append(new type::Field((*field)->name_, entry));
  }
  return new type::RecordTy(field_ty_list);
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  auto entry = tenv->Look(array_);
  if (!entry) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().data());
    return type::VoidTy::Instance(); 
  }

  return new type::ArrayTy(entry);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}
} // namespace sem
