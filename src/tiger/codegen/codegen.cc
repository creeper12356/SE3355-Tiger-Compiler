#include "tiger/codegen/codegen.h"

#include <cassert>
#include <iostream>
#include <sstream>

extern frame::RegManager *reg_manager;
extern frame::Frags *frags;

namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {

void CodeGen::Codegen() {
  temp_map_ = new std::unordered_map<llvm::Value *, temp::Temp *>();
  bb_map_ = new std::unordered_map<llvm::BasicBlock *, int>();
  auto *list = new assem::InstrList();

  // firstly get all global string's location
  for (auto &&frag : frags->GetList()) {
    if (auto *str_frag = dynamic_cast<frame::StringFrag *>(frag)) {
      auto tmp = temp::TempFactory::NewTemp();
      list->Append(new assem::OperInstr(
          "leaq " + std::string(str_frag->str_val_->getName()) + "(%rip),`d0",
          new temp::TempList(tmp), new temp::TempList(), nullptr));
      temp_map_->insert({str_frag->str_val_, tmp});
    }
  }

  // move arguments to temp
  auto arg_iter = traces_->GetBody()->arg_begin();
  auto regs = reg_manager->ArgRegs();
  auto tmp_iter = regs->GetList().begin();

  // first arguement is rsp, we need to skip it
  ++arg_iter;

  for (; arg_iter != traces_->GetBody()->arg_end() &&
         tmp_iter != regs->GetList().end();
       ++arg_iter, ++tmp_iter) {
    auto tmp = temp::TempFactory::NewTemp();
    // NOTE: 为什么此处要使用OperInstr，而不使用MoveInstr？
    list->Append(new assem::OperInstr("movq `s0,`d0", new temp::TempList(tmp),
                                      new temp::TempList(*tmp_iter), nullptr));
    temp_map_->insert({static_cast<llvm::Value *>(arg_iter), tmp});
  }

  // pass-by-stack parameters
  if (arg_iter != traces_->GetBody()->arg_end()) {
    auto last_sp = temp::TempFactory::NewTemp();
    list->Append(
        new assem::OperInstr("movq %rsp,`d0", new temp::TempList(last_sp),
                             new temp::TempList(reg_manager->GetRegister(
                                 frame::X64RegManager::Reg::RSP)),
                             nullptr));
    list->Append(new assem::OperInstr(
        "addq $" + std::string(traces_->GetFunctionName()) +
            "_framesize_local,`s0",
        new temp::TempList(last_sp),
        new temp::TempList({last_sp, reg_manager->GetRegister(
                                         frame::X64RegManager::Reg::RSP)}),
        nullptr));
    while (arg_iter != traces_->GetBody()->arg_end()) {
      auto tmp = temp::TempFactory::NewTemp();
      list->Append(new assem::OperInstr(
          "movq " +
              std::to_string(8 * (arg_iter - traces_->GetBody()->arg_begin())) +
              "(`s0),`d0",
          new temp::TempList(tmp), new temp::TempList(last_sp), nullptr));
      temp_map_->insert({static_cast<llvm::Value *>(arg_iter), tmp});
      ++arg_iter;
    }
  }

  // construct bb_map
  int bb_index = 0;
  for (auto &&bb : traces_->GetBasicBlockList()->GetList()) {
    bb_map_->insert({bb, bb_index++});
  }

  for (auto &&bb : traces_->GetBasicBlockList()->GetList()) {
    // record every return value from llvm instruction
    for (auto &&inst : bb->getInstList())
      temp_map_->insert({&inst, temp::TempFactory::NewTemp()});
  }

  for (auto &&bb : traces_->GetBasicBlockList()->GetList()) {
    // Generate label for basic block
    list->Append(new assem::LabelInstr(std::string(bb->getName())));

    // Generate instructions for basic block
    for (auto &&inst : bb->getInstList())
      InstrSel(list, inst, traces_->GetFunctionName(), bb);
  }

  assem_instr_ = std::make_unique<AssemInstr>(frame::ProcEntryExit2(
      frame::ProcEntryExit1(traces_->GetFunctionName(), list)));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}

void CodeGen::InstrSel(assem::InstrList *instr_list, llvm::Instruction &inst,
                       std::string_view function_name, llvm::BasicBlock *bb) {
  // TODO: your lab5 code here
  // TODO: 考虑参数为llvm全局变量的情况
  auto opcode = inst.getOpcode();
  switch (opcode)
  {
  case llvm::Instruction::Load:
    {
      auto type = inst.getType();
      if(!type->isIntegerTy()) {
        throw std::runtime_error("Not supported load instr type");
      }
      auto src_temp = temp_map_->at(inst.getOperand(0));
      auto dst_temp = temp::TempFactory::NewTemp();
      temp_map_->insert({&inst, dst_temp});
      instr_list->Append(new assem::MoveInstr(
        "movq (`s0),`d0",
        new temp::TempList(dst_temp),
        new temp::TempList(src_temp)
      ));
    }
    break;
  case llvm::Instruction::Add:
    {
      auto type = inst.getType();
      if(!type->isIntegerTy()) {
        throw std::runtime_error("Not supported add instr type");
      }
      // NOTE: 此处假定第一个参数为临时变量
      auto first_operand_temp = temp_map_->at(inst.getOperand(0));
      auto dst_temp = temp::TempFactory::NewTemp();
      if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))) {
        // 第二个参数为常数，翻译为leaq指令
        instr_list->Append(new assem::OperInstr(
          "leaq " + std::to_string(const_int->getSExtValue()) + "(`s0),`d0",
          new temp::TempList(dst_temp),
          new temp::TempList(first_operand_temp),
          nullptr
        ));
      } else {
        // 第二个参数为临时变量，翻译为addq指令
        auto second_operand_temp = temp_map_->at(inst.getOperand(1));
        auto dst_temp = temp::TempFactory::NewTemp();
        instr_list->Append(new assem::MoveInstr(
          "movq `s0,`d0",
          new temp::TempList(dst_temp),
          new temp::TempList(first_operand_temp)
        ));
        instr_list->Append(new assem::OperInstr(
          "addq `s1,`d0",
          new temp::TempList(dst_temp),
          new temp::TempList({dst_temp, second_operand_temp}),
          nullptr
        ));
      }

      temp_map_->insert({&inst, dst_temp});
    }
    break;
  case llvm::Instruction::Sub:
    {
      // 减法逻辑与加法类似
      auto type = inst.getType();
      if(!type->isIntegerTy()) {
        throw std::runtime_error("Not supported sub instr type");
      }
      auto first_operand_temp = temp_map_->at(inst.getOperand(0));
      auto dst_temp = temp::TempFactory::NewTemp();
      if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))) {
        // 第二个参数为常数，翻译为leaq指令
        // NOTE: 不考虑d0本身为负数的情况，a.k.a --d0
        instr_list->Append(new assem::OperInstr(
          "leaq -" + std::to_string(const_int->getSExtValue()) + "(`s0),`d0",
          new temp::TempList(dst_temp),
          new temp::TempList(first_operand_temp),
          nullptr
        ));
      } else {
        // 第二个参数为临时变量，翻译为subq指令
        auto second_operand_temp = temp_map_->at(inst.getOperand(1));
        auto dst_temp = temp::TempFactory::NewTemp();
        instr_list->Append(new assem::MoveInstr(
          "movq `s0,`d0",
          new temp::TempList(dst_temp),
          new temp::TempList(first_operand_temp)
        ));
        instr_list->Append(new assem::OperInstr(
          "subq `s1,`d0",
          new temp::TempList(dst_temp),
          new temp::TempList({dst_temp, second_operand_temp}),
          nullptr
        ));
      }
    }
    break;
  case llvm::Instruction::Mul:
    {
      auto type = inst.getType();
      if(!type->isIntegerTy()) {
        throw std::runtime_error("Not supported mul instr type");
      }
      auto first_operand_temp = temp_map_->at(inst.getOperand(0));
      auto dst_temp = temp::TempFactory::NewTemp();
      instr_list->Append(new assem::MoveInstr(
        "movq `s0,%rax",
        new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
        new temp::TempList(first_operand_temp)
      ));
      if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))) {
        // 第二个参数为常数，翻译为imulq
        instr_list->Append(new assem::OperInstr(
          "imulq $" + std::to_string(const_int->getSExtValue()),
          nullptr, // NOTE:暂时置空
          nullptr,
          nullptr
        ));
      } else {
        // 第二个参数为临时变量，翻译为imulq
        auto second_operand_temp = temp_map_->at(inst.getOperand(1));
        instr_list->Append(new assem::OperInstr(
          "imulq `s0",
          nullptr,
          new temp::TempList(second_operand_temp),
          nullptr
        ));
      }
      // NOTE: imulq S 将%rax中的值乘以S，结果存入%rdx:%rax，此处暂时不考虑%rdx的溢出部分
      instr_list->Append(new assem::MoveInstr(
        "movq %rax,`d0",
        new temp::TempList(dst_temp),
        new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX))
      ));

      temp_map_->insert({&inst, dst_temp});
    }
    break;
  case llvm::Instruction::SDiv:
    {
      auto type = inst.getType();
      if(!type->isIntegerTy()) {
        throw std::runtime_error("Not supported sdiv instr type");
      }
      auto first_operand_temp = temp_map_->at(inst.getOperand(0));
      auto dst_temp = temp::TempFactory::NewTemp();
      instr_list->Append(new assem::MoveInstr(
        "movq `s0,%rax",
        new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
        new temp::TempList(first_operand_temp)
      ));
      instr_list->Append(new assem::OperInstr(
        "cqto",
        nullptr,
        nullptr,
        nullptr
      ));
      if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))) {
        // 第二个参数为常数
        instr_list->Append(new assem::OperInstr(
          "idivq $" + std::to_string(const_int->getSExtValue()),
          nullptr, 
          nullptr,
          nullptr
        ));
      } else {
        // 第二个参数为临时变量
        auto second_operand_temp = temp_map_->at(inst.getOperand(1));
        instr_list->Append(new assem::OperInstr(
          "idivq `s0",
          nullptr,
          new temp::TempList(second_operand_temp),
          nullptr
        ));
      }
      instr_list->Append(new assem::MoveInstr(
        "movq %rax,`d0",
        new temp::TempList(dst_temp),
        new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX))
      ));

      temp_map_->insert({&inst, dst_temp});
    }
    break;

  // 在x86-64中，不区分指针和整数，因此只需要移动临时变量即可
  case llvm::Instruction::PtrToInt:
    {
      auto dst_temp = temp::TempFactory::NewTemp();
      instr_list->Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(dst_temp),
        new temp::TempList(temp_map_->at(inst.getOperand(0)))
      ));
      temp_map_->insert({&inst, dst_temp});
    }
    break;
  case llvm::Instruction::IntToPtr:
    {
      auto dst_temp = temp::TempFactory::NewTemp();
      instr_list->Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(dst_temp),
        new temp::TempList(temp_map_->at(inst.getOperand(0)))
      ));
      temp_map_->insert({&inst, dst_temp});
    }
    break;


  case llvm::Instruction::GetElementPtr:
    {
      auto base_ptr = temp_map_->at(inst.getOperand(0));
      auto dst_temp = temp::TempFactory::NewTemp();
    }
    break;
  case llvm::Instruction::Store:
    {
      auto type = inst.getType();
      if(!type->isIntegerTy()) {
        throw std::runtime_error("Not supported store instr type");
      }
      auto dst_temp = temp_map_->at(inst.getOperand(1));
      if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
        // store一个常数
        instr_list->Append(new assem::MoveInstr(
          "movq $" + std::to_string(const_int->getSExtValue()) + ",(`d0)",
          new temp::TempList(dst_temp),
          nullptr
        ));
      } else {
        // store一个临时变量
        auto src_temp = temp_map_->at(inst.getOperand(0));
        instr_list->Append(new assem::MoveInstr(
          "movq `s0,(`d0)",
          new temp::TempList(dst_temp),
          new temp::TempList(src_temp)
        ));
      }
    }
  break;
  // case llvm::Instruction::BitCast:
  // case llvm::Instruction::ZExt:
  case llvm::Instruction::Call:
    {
      auto call_inst = llvm::cast<llvm::CallInst>(&inst);
      auto regs = reg_manager->ArgRegs();
      auto reg_tmp_iter = regs->GetList().begin();
      auto arg_iter = call_inst->arg_begin();
      // 跳过sp
      ++ arg_iter;

      // 寄存器传参
      for(; arg_iter != call_inst->arg_end() &&
            reg_tmp_iter != regs->GetList().end();
          ++ arg_iter, ++ reg_tmp_iter) {
        llvm::Value *arg = arg_iter->get();
        if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(arg)) {
          // 常数参数
          instr_list->Append(new assem::MoveInstr(
            "movq $" + std::to_string(const_int->getSExtValue()) + ",`d0",
            new temp::TempList(*reg_tmp_iter),
            nullptr
          ));
        } else {
          // 临时变量参数
          auto temp = temp_map_->at(arg);
          instr_list->Append(new assem::MoveInstr(
            "movq `s0,`d0",
            new temp::TempList(*reg_tmp_iter),
            new temp::TempList(temp)
          ));
        }
      }

      // 栈传参
      // TODO: 栈传参的参数应该倒序压栈
      if (arg_iter != call_inst->arg_end()) {
        for(; arg_iter != call_inst->arg_end(); ++ arg_iter) {
          llvm::Value *arg = arg_iter->get();
          if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(arg)) {
            // 常数参数，先将常数移到寄存器
            auto dest_temp = temp::TempFactory::NewTemp();
            instr_list->Append(new assem::MoveInstr(
              "movq $" + std::to_string(const_int->getSExtValue()) + ",`d0",
              new temp::TempList(dest_temp),
              nullptr
            ));
            // NOTE: 此处不需要将dest_temp加入temp_map，因为后续不会用到这个临时变量
            instr_list->Append(new assem::OperInstr(
              "pushq `s0",
              nullptr,
              new temp::TempList(dest_temp),
              nullptr
            ));
          } else {
            // 临时变量参数
            auto arg_temp = temp_map_->at(arg);
            instr_list->Append(new assem::OperInstr(
              "pushq `s0",
              nullptr,
              new temp::TempList(arg_temp),
              nullptr
            ));
          }
        }
      }

      // NOTE: 汇编级别无需将返回地址压栈，因为call指令会自动压栈

      // 调用函数对应的label
      instr_list->Append(new assem::OperInstr(
        "call " + std::string(call_inst->getCalledFunction()->getName()),
        nullptr,
        nullptr,
        nullptr
      ));

      // 将%rax中的返回值存入临时变量
      auto res_temp = temp::TempFactory::NewTemp();
      instr_list->Append(new assem::MoveInstr(
        "movq %rax,`d0",
        new temp::TempList(res_temp),
        new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX))
      ));
      temp_map_->insert({&inst, res_temp});
    }
    break;
  case llvm::Instruction::Ret:
    {
      if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
        // 返回值为常数
        instr_list->Append(new assem::MoveInstr(
          "movq $" + std::to_string(const_int->getSExtValue()) + ",%rax",
          new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
          nullptr
        ));
      } else {
        auto ret_temp = temp_map_->at(inst.getOperand(0));
        instr_list->Append(new assem::MoveInstr(
          "movq `s0,%rax",
          new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
          new temp::TempList(ret_temp)
        ));
      }
      instr_list->Append(new assem::OperInstr(
        "ret",
        nullptr,
        nullptr,
        nullptr
      ));
    }
    break;
  case llvm::Instruction::Br:
    {

    }
    break;
  case llvm::Instruction::ICmp:
    {

    }
    break;
  case llvm::Instruction::PHI:
    {

    }
    break;
  default:
    break;
  }

  // throw std::runtime_error(std::string("Unknown instruction: ") +
  //                          inst.getOpcodeName());
}

} // namespace cg