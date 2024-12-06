#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <stack>

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;
extern llvm::IRBuilder<> *ir_builder;
extern llvm::Module *ir_module;
std::stack<llvm::Function *> func_stack;
std::stack<llvm::BasicBlock *> loop_stack; // for break and continue
llvm::Function *alloc_record;
llvm::Function *init_array;
llvm::Function *string_equal;
std::vector<std::pair<std::string, frame::Frame *>> frame_info;

bool CheckBBTerminatorIsBranch(llvm::BasicBlock *bb) {
  auto inst = bb->getTerminator();
  if (inst) {
    llvm::BranchInst *branchInst = llvm::dyn_cast<llvm::BranchInst>(inst);
    if (branchInst && !branchInst->isConditional()) {
      return true;
    }
  }
  return false;
}

int getActualFramesize(tr::Level *level) {
  return level->frame_->calculateActualFramesize();
}

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  return new Access(level, level->frame_->AllocLocal(escape));
}

class ValAndTy {
public:
  type::Ty *ty_;
  llvm::Value *val_;
  llvm::BasicBlock *last_bb_;

  ValAndTy(llvm::Value *val, type::Ty *ty) : val_(val), ty_(ty) {}
};

void ProgTr::OutputIR(std::string_view filename) {
  std::string llvmfile = std::string(filename) + ".ll";
  std::error_code ec;
  llvm::raw_fd_ostream out(llvmfile, ec, llvm::sys::fs::OpenFlags::OF_Text);
  ir_module->print(out, nullptr);
}

void ProgTr::Translate() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level_.get(),
                         errormsg_.get());
}

} // namespace tr

namespace absyn {

tr::ValAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  return root_->Translate(venv, tenv, level, errormsg);
}

void TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                        err::ErrorMsg *errormsg) const {
  auto& original_type_list = this->types_->GetList();
  std::vector<type::NameTy *> processed_type_list;
  processed_type_list.reserve(original_type_list.size());

  // 将类型名加入环境tenv
  for (const auto &type : original_type_list) {
    auto *name_ty_instance = new type::NameTy(type->name_, nullptr);
    tenv->Enter(type->name_, name_ty_instance);
    processed_type_list.push_back(name_ty_instance);
  }

  // 解析并回填类型
  auto original_iter = original_type_list.begin();
  auto processed_iter = processed_type_list.begin();
  while (original_iter != original_type_list.end()) {
    auto resolved_type = (*original_iter)->ty_->Translate(tenv, errormsg);
    (*processed_iter)->ty_ = resolved_type;

    ++original_iter;
    ++processed_iter;
  }
}

void FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, err::ErrorMsg *errormsg) const {
  auto& func_list = this->functions_->GetList();
  for(auto &func_dec: func_list) {
    // 创建新的level和frame
    auto new_frame = frame::NewFrame(
      func_dec->name_,
      std::list<bool>(1 + func_dec->params_->GetList().size(), false)
    );
    auto new_level = new tr::Level(
      new_frame,
      level
    );

    // 创建framesize_global变量，初始化为0
    // 并初始化stack pointer
    new_frame->framesize_global = new llvm::GlobalVariable(
      *ir_module, 
      llvm::Type::getInt64Ty(ir_builder->getContext()),
      true,
      llvm::GlobalValue::ExternalLinkage,
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_builder->getContext()), 0),
      func_dec->name_->Name() + "_framesize_global"
    );
    
    // 创建LLVM IR函数定义
    auto result_ty = tenv->Look(func_dec->result_);
    auto params = std::vector<llvm::Type *>();
    auto func_dec_params = func_dec->params_->GetList();

    // 在生成LLVM IR的函数定义，参数列表前插入stack pointer, static link
    params.reserve(2 + func_dec_params.size());
    params.push_back(ir_builder->getInt64Ty());
    params.push_back(ir_builder->getInt64Ty());
    for(auto &func_dec_param: func_dec_params) {
      auto param_ty = tenv->Look(func_dec_param->typ_);
      params.push_back(param_ty->GetLLVMType());
    }

    auto func = llvm::Function::Create(
      llvm::FunctionType::get(
        result_ty->GetLLVMType(),
        params,
        false
      ),
      llvm::Function::ExternalLinkage
    );

    // 将函数加入环境venv,
    // 此处的形参并不包含stack pointer和static link
    auto formal_ty_list = func_dec->params_->MakeFormalTyList(tenv, errormsg);
    venv->Enter(
      func_dec->name_,
      new env::FunEntry(
        new_level,
        formal_ty_list,
        result_ty,
        func_dec->name_->Name()
      )
    );

    // 创建基本块
    auto new_bb = llvm::BasicBlock::Create(
      ir_builder->getContext(),
      func_dec->name_->Name() + "_bb",
      func
    );
    func_stack.push(func);
    ir_builder->SetInsertPoint(new_bb);

    new_level->set_sp(ir_builder->CreateSub(level->get_sp(), new_frame->framesize_global));

    
    // 回填形式参数
    auto arg_iter = func->arg_begin();
    // 跳过stack pointer
    ++ arg_iter;
    auto formal_iter = new_frame->Formals()->begin();

    while(arg_iter != func->arg_end()) {
      auto arg_val = &*arg_iter;
      ir_builder->CreateStore(arg_val, (*formal_iter)->ToLLVMVal(new_level->get_sp()));
      ++ arg_iter;
      ++ formal_iter;
    }

    // 生成函数体，
    // 在分析函数体时，进行outgo的分配
    auto body_val_ty = func_dec->body_->Translate(venv, tenv, new_level, errormsg);

    // 回填framesize_global
    new_frame->framesize_global->setInitializer(
      llvm::ConstantInt::get(
        ir_builder->getInt64Ty(),
        getActualFramesize(new_level)
      )
    );

    // 如果函数体不是void类型，需要返回值
    if(!result_ty->IsSameType(type::VoidTy::Instance())) {
      ir_builder->CreateRet(body_val_ty->val_);
    }

    func_stack.pop();
  }

  ir_builder->SetInsertPoint(&func_stack.top()->getBasicBlockList().back());

}

void VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                       err::ErrorMsg *errormsg) const {
  auto ty_entry = tenv->Look(this->typ_);
  auto init_val_ty = init_->Translate(venv, tenv, level, errormsg);

  // 将变量加入环境venv
  auto access = new tr::Access(level, level->frame_->AllocLocal(escape_));
  venv->Enter(var_, new env::VarEntry(access, ty_entry));

  // 初始化变量
  llvm::Value *val = init_val_ty->val_;
  ir_builder->CreateStore(val, access->access_->ToLLVMVal(level->get_sp()));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  auto entry = tenv->Look(name_);
  if (!entry) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().data());
    return type::VoidTy::Instance();
  }

  return entry;
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  auto field_list = record_->GetList();
  auto field_ty_list = new type::FieldList();
  for (auto field = field_list.begin(); field != field_list.end(); field++) {
    auto entry = tenv->Look((*field)->typ_);
    if (!entry) {
      errormsg->Error(pos_, "undefined type %s", (*field)->typ_->Name().data());
      return type::VoidTy::Instance();
    }
    field_ty_list->Append(new type::Field((*field)->name_, entry));
  }
  return new type::RecordTy(field_ty_list);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  auto entry = tenv->Look(array_);
  if (!entry) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().data());
    return type::VoidTy::Instance();
  }

  return new type::ArrayTy(entry);
}

tr::ValAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  auto var_entry = static_cast<env::VarEntry *> (venv->Look(sym_));
  assert(var_entry);
  if(var_entry->access_->level_ == level) {
    // 访问当前层的局部变量
    llvm::Value *val = ir_builder->CreateIntToPtr(
      var_entry->access_->access_->ToLLVMVal(level->get_sp()),
      llvm::PointerType::get(var_entry->ty_->GetLLVMType(), 0),
      sym_->Name()
    );
    return new tr::ValAndTy(val, var_entry->ty_->ActualTy());
  } else {
    // 访问逃逸变量
    llvm::Value *val = level->get_sp();
    while(level != var_entry->access_->level_) {
      // 当前层的第一个formal即为static link
      auto sl_formal = level->frame_->Formals()->begin();
      llvm::Value *static_link_addr = (*sl_formal)->ToLLVMVal(val);
      llvm::Value *static_link_ptr = ir_builder->CreateIntToPtr(
        static_link_addr,
        llvm::PointerType::get(ir_builder->getInt64Ty(), 0)
      );
      val = ir_builder->CreateLoad(ir_builder->getInt64Ty(), static_link_ptr);
      level = level->parent_;
    }

    val = var_entry->access_->access_->ToLLVMVal(val);
    return new tr::ValAndTy(val, var_entry->ty_->ActualTy());
  }
}

tr::ValAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  auto var_val_ty = var_->Translate(venv, tenv, level, errormsg);
  auto record_ty = static_cast<type::RecordTy *>(var_val_ty->ty_->ActualTy());

  int index = 0;
  auto &field_list = record_ty->fields_->GetList();
  type::Ty *field_ty = nullptr;
  for(auto &field : field_list) {
    if(field->name_ == sym_) {
      field_ty = field->ty_;
      break;
    }
    ++ index;
  }

  assert(field_ty);

  llvm::Value *val = ir_builder->CreateGEP(
    field_ty->GetLLVMType(),
    ir_builder->CreateLoad(var_val_ty->ty_->GetLLVMType(), var_val_ty->val_),
    ir_builder->getInt32(index)
  );

  return new tr::ValAndTy(val, field_ty);
}

tr::ValAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level,
                                      err::ErrorMsg *errormsg) const {
  auto var_val_ty = var_->Translate(venv, tenv, level, errormsg);
  auto array_ty = static_cast<type::ArrayTy *>(var_val_ty->ty_->ActualTy());

  auto subscript_val_ty = subscript_->Translate(venv, tenv, level, errormsg);
  assert(subscript_val_ty->ty_->ActualTy() == type::IntTy::Instance());

  llvm::Value *val = ir_builder->CreateGEP(
    array_ty->ty_->GetLLVMType(),
    ir_builder->CreateLoad(var_val_ty->ty_->GetLLVMType(), var_val_ty->val_),
    subscript_val_ty->val_
  );

  return new tr::ValAndTy(val, array_ty->ty_);
}

tr::ValAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  return var_->Translate(venv, tenv, level, errormsg);
}

tr::ValAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  return new tr::ValAndTy(ir_builder->getInt64(0), type::NilTy::Instance());
}

tr::ValAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  return new tr::ValAndTy(ir_builder->getInt64(val_), type::IntTy::Instance());
}

tr::ValAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  // TODO: 翻译tiger string
  return new tr::ValAndTy(ir_builder->CreateGlobalStringPtr(str_),
                          type::StringTy::Instance());
}

tr::ValAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level,
                                 err::ErrorMsg *errormsg) const {
  // NOTE: 在目前的实现中，将所有域中定义的函数视为全局函数
  // 不考虑重名的情况
  auto func = static_cast<env::FunEntry *>(venv->Look(func_));
  
  auto llvm_func = ir_module->getFunction(func_->Name());
  auto& logical_args = args_->GetList();
  std::vector<llvm::Value *> actual_args;
  if(llvm_func->arg_size() == logical_args.size()) {
    // 全局函数
    actual_args.reserve(logical_args.size());
  } else {
    // 用户自定义函数，需要在参数中传入stack pointer和static link
    actual_args.reserve(logical_args.size() + 2);
    actual_args.push_back(level->get_sp());
    actual_args.push_back(level->get_sp());
  }
  for(auto &logical_arg: logical_args) {
    auto arg_val_ty = logical_arg->Translate(venv, tenv, level, errormsg);
    actual_args.push_back(arg_val_ty->val_);
  }

  // 分配outgo
  // 逻辑函数加上static link
  level->frame_->AllocOutgoSpace((logical_args.size() + 1) * 8);

  auto call_val = ir_builder->CreateCall(llvm_func, actual_args);

  if(llvm_func->getReturnType()->isVoidTy()) {
    return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
  } else {
    return new tr::ValAndTy(call_val, func->result_);
  }
}

tr::ValAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level,
                               err::ErrorMsg *errormsg) const {
  // TODO: 考虑短路求值
  if(oper_ == AND_OP) {
  } else if(oper_ == OR_OP) {
  } else {
    // 无需考虑短路求值
    auto left_val_ty = left_->Translate(venv, tenv, level, errormsg);
    auto right_val_ty = right_->Translate(venv, tenv, level, errormsg);

    // 保证操作数类型相同，且都为简单类型（int or string）    
    assert(left_val_ty->ty_->IsSameType(right_val_ty->ty_));
    assert(left_val_ty->ty_->IsSameType(type::IntTy::Instance()) || 
            left_val_ty->ty_->IsSameType(type::StringTy::Instance()));
    
    auto left_val_loaded = ir_builder->CreateLoad(left_val_ty->ty_->GetLLVMType(), left_val_ty->val_);
    auto right_val_loaded = ir_builder->CreateLoad(right_val_ty->ty_->GetLLVMType(), right_val_ty->val_);

    llvm::Value *val = nullptr;

    if(left_val_ty->ty_->IsSameType(type::IntTy::Instance())) {
      // 整数操作
      switch (oper_)
      {
        case PLUS_OP:
          val = ir_builder->CreateAdd(left_val_loaded, right_val_loaded);
          break;
        case MINUS_OP:
          val = ir_builder->CreateSub(left_val_loaded, right_val_loaded);
          break;
        case TIMES_OP:
          val = ir_builder->CreateMul(left_val_loaded, right_val_loaded);
          break;
        case DIVIDE_OP:
          val = ir_builder->CreateSDiv(left_val_loaded, right_val_loaded);
          break;
        case EQ_OP:
          val = ir_builder->CreateICmpEQ(left_val_loaded, right_val_loaded);
          break;
        case NEQ_OP:
          val = ir_builder->CreateICmpNE(left_val_loaded, right_val_loaded);
          break;
        case LT_OP:
          val = ir_builder->CreateICmpSLT(left_val_loaded, right_val_loaded);
          break;
        case LE_OP:
          val = ir_builder->CreateICmpSLE(left_val_loaded, right_val_loaded);
          break;
        case GT_OP:
          val = ir_builder->CreateICmpSGT(left_val_loaded, right_val_loaded);
          break;
        case GE_OP:
          val = ir_builder->CreateICmpSGE(left_val_loaded, right_val_loaded);
          break;
        default:
          assert(0);
          break;
      }; 
      return new tr::ValAndTy(val, left_val_ty->ty_);

    } else {
      // 字符串比较
      switch(oper_) 
      {
        case EQ_OP:
          val = ir_builder->CreateCall(string_equal, {left_val_ty->val_, right_val_ty->val_});
          break;
        case NEQ_OP:
          val = ir_builder->CreateNot(ir_builder->CreateCall(string_equal, {left_val_ty->val_, right_val_ty->val_}));
          break;
        default:
          assert(0);
          break;
      }
      return new tr::ValAndTy(val, type::IntTy::Instance());
    }
  }
  

}

tr::ValAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  auto &efield_list = fields_->GetList();
  auto record_ty = tenv->Look(typ_);
  assert(record_ty);
  auto record_ptr = ir_builder->CreateCall(alloc_record, {ir_builder->getInt64(efield_list.size() * 8)});

  return new tr::ValAndTy(record_ptr, record_ty);
}

tr::ValAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  auto &exp_list = seq_->GetList();
  tr::ValAndTy *val_ty = nullptr;
  for(auto &exp : exp_list) {
    val_ty = exp->Translate(venv, tenv, level, errormsg);
  }
  return val_ty;
}

tr::ValAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  auto var_ty_val = var_->Translate(venv, tenv, level, errormsg);
  auto exp_ty_val = exp_->Translate(venv, tenv, level, errormsg);

  ir_builder->CreateStore(exp_ty_val->val_, var_ty_val->val_);

  // 赋值表达式不产生值
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

tr::ValAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level,
                               err::ErrorMsg *errormsg) const {
  auto test_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "if_test", func_stack.top());
  auto then_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "if_then", func_stack.top());
  auto next_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "if_next", func_stack.top());

  // TODO: 检查origin bb是否以分支结尾
  ir_builder->CreateBr(test_bb);
  ir_builder->SetInsertPoint(test_bb);

  auto test_ty_val = test_->Translate(venv, tenv, level, errormsg);
  // TODO: 类型转换
  auto test_val = ir_builder->CreateICmpNE(
    test_ty_val->val_,
    ir_builder->getInt64(0)
  );

  tr::ValAndTy *then_ty_val = nullptr;
  tr::ValAndTy *elsee_ty_val = nullptr;
  if(!elsee_) {
    // if-then-else
    auto elsee_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "if_elsee", func_stack.top());

    ir_builder->CreateCondBr(test_val, then_bb, elsee_bb);

    ir_builder->SetInsertPoint(then_bb);
    then_ty_val = then_->Translate(venv, tenv, level, errormsg);
    ir_builder->CreateBr(next_bb);

    ir_builder->SetInsertPoint(elsee_bb);
    elsee_ty_val = elsee_->Translate(venv, tenv, level, errormsg);
    ir_builder->CreateBr(next_bb);

    ir_builder->SetInsertPoint(next_bb);
    auto phi_val = ir_builder->CreatePHI(then_ty_val->val_->getType(), 2);
    // TODO: last active bb
    // TODO: 考虑类型转换
    return new tr::ValAndTy(phi_val, then_ty_val->ty_);
  } else {
    // if-then
    ir_builder->CreateCondBr(test_val, then_bb, next_bb);

    ir_builder->SetInsertPoint(then_bb);
    then_ty_val = then_->Translate(venv, tenv, level, errormsg);
    ir_builder->CreateBr(next_bb);

    // if-then表达式是无值的
    return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
  }
}

tr::ValAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  auto test_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "while_test", func_stack.top());
  auto body_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "while_body", func_stack.top());
  auto done_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "while_done", func_stack.top());

  loop_stack.push(done_bb);

  ir_builder->CreateBr(test_bb);

  ir_builder->SetInsertPoint(test_bb);
  auto test_ty_val = test_->Translate(venv, tenv, level, errormsg);
  // TODO: 类型转换
  auto test_val = ir_builder->CreateICmpNE(
    test_ty_val->val_,
    ir_builder->getInt64(0)
  );
  ir_builder->CreateCondBr(test_val, body_bb, done_bb);

  ir_builder->SetInsertPoint(body_bb);
  body_->Translate(venv, tenv, level, errormsg);
  ir_builder->CreateBr(test_bb);

  ir_builder->SetInsertPoint(done_bb);
  loop_stack.pop();
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

tr::ValAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  // 重写AST子树
  /*
  let var i :=  lo
      var limit := hi
  in while i<= limit
        do (body; i :=  i + 1)
  end
   */
  auto dec_list = new DecList();
  auto limit_sym = sym::Symbol::UniqueSymbol("limit");
  auto int_sym = sym::Symbol::UniqueSymbol("int");
  dec_list->Prepend(new VarDec(pos_, limit_sym, int_sym, hi_));
  dec_list->Prepend(new VarDec(pos_, var_, int_sym, lo_));
  auto test_exp = new OpExp(pos_, LE_OP, new VarExp(pos_, new SimpleVar(pos_, var_)), new VarExp(pos_, new SimpleVar(pos_, limit_sym)));
  auto while_body_exp_list = new ExpList();
  while_body_exp_list->Prepend(new AssignExp(pos_, new SimpleVar(pos_, var_), new OpExp(pos_, PLUS_OP, new VarExp(pos_, new SimpleVar(pos_, var_)), new IntExp(pos_, 1))));
  while_body_exp_list->Prepend(body_);
  auto while_body_exp = new SeqExp(pos_, while_body_exp_list);
  auto let_body_exp = new WhileExp(pos_, test_exp, while_body_exp);
  auto let_exp = new LetExp(pos_, dec_list, let_body_exp);
  
  auto val_ty = let_exp->Translate(venv, tenv, level, errormsg);
  // TODO: 内存泄漏问题
  // delete let_exp;
  return val_ty;
}

tr::ValAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  ir_builder->CreateBr(loop_stack.top());
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

tr::ValAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  auto &dec_list = decs_->GetList();
  for(auto &dec: dec_list) {
    dec->Translate(venv, tenv, level, errormsg);
  }

  return body_->Translate(venv, tenv, level, errormsg);
}

tr::ValAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  auto array_ele_ty = tenv->Look(typ_);
  assert(array_ele_ty);

  auto size_val_ty = size_->Translate(venv, tenv, level, errormsg);
  auto init_val_ty = init_->Translate(venv, tenv, level, errormsg);

  auto array_ptr = ir_builder->CreateCall(init_array, {size_val_ty->val_, init_val_ty->val_});

  // TODO: 应该返回一个有效的数组类型指针
  return new tr::ValAndTy(array_ptr, type::ArrayTy(array_ele_ty).ActualTy());
}

tr::ValAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level,
                                 err::ErrorMsg *errormsg) const {
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

} // namespace absyn