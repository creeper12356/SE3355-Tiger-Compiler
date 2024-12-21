#include "tiger/frame/x64frame.h"
#include "tiger/env/env.h"

#include <iostream>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

extern frame::RegManager *reg_manager;
extern llvm::IRBuilder<> *ir_builder;
extern llvm::Module *ir_module;

namespace frame {

X64RegManager::X64RegManager() : RegManager() {
  for (int i = 0; i < REG_COUNT; i++)
    regs_.push_back(temp::TempFactory::NewTemp());

  // Note: no frame pointer in tiger compiler
  std::array<std::string_view, REG_COUNT> reg_name{
      "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi", "%rbp", "%rsp",
      "%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"};
  int reg = RAX;
  for (auto &name : reg_name) {
    temp_map_->Enter(regs_[reg], new std::string(name));
    reg++;
  }
}

temp::TempList *X64RegManager::Registers() {
  const std::array reg_array{
      RAX, RBX, RCX, RDX, RSI, RDI, RBP, R8, R9, R10, R11, R12, R13, R14, R15,
  };
  auto *temp_list = new temp::TempList();
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::ArgRegs() {
  const std::array reg_array{RDI, RSI, RDX, RCX, R8, R9};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::CallerSaves() {
  std::array reg_array{RAX, RDI, RSI, RDX, RCX, R8, R9, R10, R11};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::CalleeSaves() {
  std::array reg_array{RBP, RBX, R12, R13, R14, R15};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::ReturnSink() {
  temp::TempList *temp_list = CalleeSaves();
  temp_list->Append(regs_[SP]);
  temp_list->Append(regs_[RV]);
  return temp_list;
}

int X64RegManager::WordSize() { return 8; }

temp::Temp *X64RegManager::FramePointer() { return regs_[FP]; }

class InFrameAccess : public Access {
public:
  int offset;
  frame::Frame *parent_frame;

  explicit InFrameAccess(int offset, frame::Frame *parent)
      : offset(offset), parent_frame(parent) {}

  llvm::Value *ToLLVMVal(llvm::Value *sp) override {
    auto local_framesize_val = ir_builder->CreateLoad(
      ir_builder->getInt64Ty(),
      parent_frame->framesize_global
    );
    auto add_val_1 = ir_builder->CreateAdd(
      local_framesize_val,
      llvm::ConstantInt::get(ir_builder->getInt64Ty(), offset)
    );
    auto add_val_2 = ir_builder->CreateAdd(
      sp,
      add_val_1
    );
    return add_val_2;
  }
};

class X64Frame : public Frame {
public:
  X64Frame(temp::Label *name, std::list<frame::Access *> *formals)
      : Frame(8, 0, name, formals) {}

  [[nodiscard]] std::string GetLabel() const override { return name_->Name(); }
  [[nodiscard]] temp::Label *Name() const override { return name_; }
  [[nodiscard]] std::list<frame::Access *> *Formals() const override {
    return formals_;
  }
  frame::Access *AllocLocal(bool escape) override {
    frame::Access *access;

    offset_ -= reg_manager->WordSize();
    access = new InFrameAccess(offset_, this);

    return access;
  }
  void AllocOutgoSpace(int size) override {
    if (size > outgo_size_)
      outgo_size_ = size;
  }
};

frame::Frame *NewFrame(temp::Label *name, std::list<bool> formals) {
  auto formal_access_list = new std::list<frame::Access *>();
  auto new_frame = new X64Frame(name, formal_access_list);
  auto formal_cnt = formals.size();
  for(size_t i = 0;i < formal_cnt; ++i) {
    // NOTE: 忽略逃逸分析结果，全部假定通过栈传参
    formal_access_list->push_back(new InFrameAccess((i + 1) * 8, new_frame));
  }
  return new_frame;
}

/**
 * Moving incoming formal parameters, the saving and restoring of callee-save
 * Registers
 * @param frame curruent frame
 * @param stm statements
 * @return statements with saving, restoring and view shift
 */
assem::InstrList *ProcEntryExit1(std::string_view function_name,
                                 assem::InstrList *body) {
  // TODO: your lab5 code here
  auto callee_saved_regs = reg_manager->CalleeSaves()->GetList();
  // 用来保存calle saved register的临时变量列表
  auto temps = std::vector<temp::Temp *>();
  for(auto& callee_saved_reg: callee_saved_regs) {
    temps.push_back(temp::TempFactory::NewTemp());
  }

  // 5.Store instructions to save any callee-saved registers- including the return address register – used within the function
  auto temps_iter = temps.begin();
  for(auto &reg: callee_saved_regs) {
    body->Insert(
      body->GetList().begin(),
      new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList((*temps_iter)),
        new temp::TempList(reg)
      )
    );
    
    ++ temps_iter;
  }


  // 创建一个返回的标签，所有return语句跳转到它
  body->Append(new assem::LabelInstr(std::string(function_name) + "_ret"));

  // 8.Load instructions to restore the callee-save registers
  temps_iter = temps.begin();
  for(auto &reg: callee_saved_regs) {
    body->Append(
      new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(reg),
        new temp::TempList((*temps_iter))
      )
    );

    ++ temps_iter;
  }

  return body;
}

/**
 * Appends a “sink” instruction to the function body to tell the register
 * allocator that certain registers are live at procedure exit
 * @param body function body
 * @return instructions with sink instruction
 */
assem::InstrList *ProcEntryExit2(assem::InstrList *body) {
  body->Append(new assem::OperInstr("", new temp::TempList(),
                                    reg_manager->ReturnSink(), nullptr));
  return body;
}

/**
 * The procedure entry/exit sequences
 * @param frame the frame of current func
 * @param body current function body
 * @return whole instruction list with prolog_ end epilog_
 */
assem::Proc *ProcEntryExit3(std::string_view function_name,
                            assem::InstrList *body) {
  std::string prologue = "";
  std::string epilogue = "";

  auto function_name_str = std::string(function_name);

  // TODO: your lab5 code here
  // prologue
  // 1. Pseudo-instructions to announce the beginning of a function;

  
  // 2. A label definition of the function name
  auto function_label = function_name_str + ":\n";
  prologue += function_label;

  // 3. An instruction to adjust the stack pointer.
  auto load_framesize = "movq " + function_name_str + "_framesize_global(%rip),%rax\n";
  auto set_sp = "subq %rax,%rsp\n";
  prologue += load_framesize;
  prologue += set_sp;
  
  // epilogue
  // 9. An instruction to reset the stack pointer (to deallocate the frame)
  load_framesize = "movq " + function_name_str + "_framesize_global(%rip),%rdi\n";
  auto reset_sp = "addq %rdi,%rsp\n";
  epilogue += load_framesize;
  epilogue += reset_sp;

  // 10. A return instruction (Jump to the return address)
  epilogue += "retq\n";
  
  // 11. Pseduo-instructions, as needed, to announce the end of a function


  return new assem::Proc(prologue, body, epilogue);
}

void Frags::PushBack(Frag *frag) { frags_.emplace_back(frag); }

} // namespace frame