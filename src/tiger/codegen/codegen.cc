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
                             nullptr,
                             nullptr));
    list->Append(new assem::OperInstr(
        "addq $" + std::string(traces_->GetFunctionName()) +
            "_framesize_local,`s0",
        new temp::TempList(last_sp),
        new temp::TempList(last_sp),
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
  int index = 0;
  map->DumpMap(out);
  for (auto instr : instr_list_->GetList()) {
    instr->Print(out, map);
    ++ index;
  }
  fprintf(out, "\n");
}

void CodeGen::InstrSel(assem::InstrList *instr_list, llvm::Instruction &inst,
                       std::string_view function_name, llvm::BasicBlock *bb) {
  // TODO: your lab5 code here
  auto opcode = inst.getOpcode();
  switch (opcode)
  {
  case llvm::Instruction::Load:
  // res = load op0
    {
      auto type = inst.getType();
      if(!type->isIntegerTy() && !type->isPointerTy()) {
        throw std::runtime_error("Not supported load instr type");
      }
      auto dst_temp = temp_map_->at(&inst);
      if(auto global_var = llvm::dyn_cast<llvm::GlobalVariable>(inst.getOperand(0))) {
        // op0: global var
        instr_list->Append(new assem::OperInstr(
          "movq " + global_var->getName().str() + "(%rip),`d0",
          new temp::TempList(dst_temp),
          nullptr,
          nullptr
        ));
      } else {
        // op0: temp
        auto src_temp = temp_map_->at(inst.getOperand(0));
        instr_list->Append(new assem::OperInstr(
          "movq (`s0),`d0",
          new temp::TempList(dst_temp),
          new temp::TempList(src_temp),
          nullptr
        ));
      }
    }
    break;
  case llvm::Instruction::Add:
  // res = add op0, op1
    {
      auto type = inst.getType();
      if(!type->isIntegerTy()) {
        throw std::runtime_error("Not supported add instr type");
      }

      // NOTE: 生成的llvm中，只有add指令可能用到xxx_sp，且总是第一个参数
      auto dst_temp = temp_map_->at(&inst);
      if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))) {
        // op0: temp, op1: immediate
        if(IsRsp(inst.getOperand(0), function_name)) {
          instr_list->Append(new assem::OperInstr(
            "leaq " + std::to_string(const_int->getSExtValue()) + "(%rsp),`d0",
            new temp::TempList(dst_temp),
            nullptr,
            nullptr
          ));
        } else {
          temp::Temp *first_operand_temp = temp_map_->at(inst.getOperand(0));
          instr_list->Append(new assem::OperInstr(
            "leaq " + std::to_string(const_int->getSExtValue()) + "(`s0),`d0",
            new temp::TempList(dst_temp),
            new temp::TempList(first_operand_temp),
            nullptr
          ));
        }
      } else {
        // op1: temp
        if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
          // op0: immediate, op1: temp
          auto second_operand_temp = temp_map_->at(inst.getOperand(1));
          instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(const_int->getSExtValue()) + ",`d0",
            new temp::TempList(dst_temp),
            nullptr,
            nullptr
          ));
          instr_list->Append(new assem::OperInstr(
            "addq `s1,`d0",
            new temp::TempList(dst_temp),
            new temp::TempList({dst_temp, second_operand_temp}),
            nullptr
          ));

        } else {
          // op0: temp, op1: temp
          // NOTE: 代码重复，考虑合并
          if(IsRsp(inst.getOperand(0), function_name)) {
            // NOTE: 不能合并到rsp
            instr_list->Append(new assem::OperInstr(
              "movq %rsp,`d0",
              new temp::TempList(dst_temp),
              nullptr,
              nullptr
            ));
          } else {
            temp::Temp *first_operand_temp = temp_map_->at(inst.getOperand(0));
            instr_list->Append(new assem::MoveInstr(
              "movq `s0,`d0",
              new temp::TempList(dst_temp),
              new temp::TempList(first_operand_temp)
            ));
          }
          auto second_operand_temp = temp_map_->at(inst.getOperand(1));
          instr_list->Append(new assem::OperInstr(
            "addq `s1,`d0",
            new temp::TempList(dst_temp),
            new temp::TempList({dst_temp, second_operand_temp}),
            nullptr
          ));
        }
      }
    }
    break;
  
  case llvm::Instruction::Sub:
  // res = sub op0, op1
    {
      // 减法逻辑与加法类似
      auto type = inst.getType();
      if(!type->isIntegerTy()) {
        throw std::runtime_error("Not supported sub instr type");
      }
      // 处理llvm中sp的生成
      // e.g. %dec2bin_sp = sub i64 %0, %dec2bin_local_framesize
      if(IsRsp(&inst, function_name)) {
        // 忽略，因为已经在Codegen中处理过了，后续所有xx_sp可以直接替换为%rsp
        break;
      }

      auto dst_temp = temp_map_->at(&inst);
      if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))) {
        // op0: temp, op1: immediate
        // NOTE: 不考虑d0本身为负数的情况，a.k.a --d0
        auto first_operand_temp = temp_map_->at(inst.getOperand(0));
        instr_list->Append(new assem::OperInstr(
          "leaq -" + std::to_string(const_int->getSExtValue()) + "(`s0),`d0",
          new temp::TempList(dst_temp),
          new temp::TempList(first_operand_temp),
          nullptr
        ));
      } else {
        // op1: temp
        if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
          // op0: immediate, op1: temp
          auto second_operand_temp = temp_map_->at(inst.getOperand(1));
          instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(const_int->getSExtValue()) + ",`d0",
            new temp::TempList(dst_temp),
            nullptr,
            nullptr
          ));
          instr_list->Append(new assem::OperInstr(
            "subq `s1,`d0",
            new temp::TempList(dst_temp),
            new temp::TempList({dst_temp, second_operand_temp}),
            nullptr
          ));

        } else {
          // op0: temp, op1: temp
          auto first_operand_temp = temp_map_->at(inst.getOperand(0));
          auto second_operand_temp = temp_map_->at(inst.getOperand(1));
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
    }
    break;

  case llvm::Instruction::Mul:
  // res = mul op0, op1
    {
      auto type = inst.getType();
      if(!type->isIntegerTy()) {
        throw std::runtime_error("Not supported mul instr type");
      }
      auto dst_temp = temp_map_->at(&inst);
      // NOTE: 此处的imulq指令是二目的
      if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))) {
        // op0: temp, op1: immediate
        auto first_operand_temp = temp_map_->at(inst.getOperand(0));
        instr_list->Append(new assem::MoveInstr(
          "movq `s0,`d0",
          new temp::TempList(dst_temp),
          new temp::TempList(first_operand_temp)
        ));
        instr_list->Append(new assem::OperInstr(
          "imulq $" + std::to_string(const_int->getSExtValue()) + ",`d0",
          new temp::TempList(dst_temp),
          new temp::TempList(dst_temp),
          nullptr
        ));
      } else {
        // op1: temp
        if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
          // op0: immediate, op1: temp
          auto second_operand_temp = temp_map_->at(inst.getOperand(1));
          instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(const_int->getSExtValue()) + ",`d0",
            new temp::TempList(dst_temp),
            nullptr,
            nullptr
          ));
          instr_list->Append(new assem::OperInstr(
            "imulq `s1,`d0",
            new temp::TempList(dst_temp),
            new temp::TempList({dst_temp, second_operand_temp}),
            nullptr
          ));

        } else {
          // op0: temp, op1: temp
          auto first_operand_temp = temp_map_->at(inst.getOperand(0));
          auto second_operand_temp = temp_map_->at(inst.getOperand(1));
          instr_list->Append(new assem::MoveInstr(
            "movq `s0,`d0",
            new temp::TempList(dst_temp),
            new temp::TempList(first_operand_temp)
          ));
          instr_list->Append(new assem::OperInstr(
            "imulq `s1,`d0",
            new temp::TempList(dst_temp),
            new temp::TempList({dst_temp, second_operand_temp}),
            nullptr
          ));
        }
      }
    }
    break;

  case llvm::Instruction::SDiv:
  // res = sdiv op0, op1
    {
      auto type = inst.getType();
      if(!type->isIntegerTy()) {
        throw std::runtime_error("Not supported sdiv instr type");
      }
      auto dst_temp = temp_map_->at(&inst);
      if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
        // op0: immediate
        instr_list->Append(new assem::OperInstr(
          "movq $" + std::to_string(const_int->getSExtValue()) + ",%rax",
          new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
          nullptr,
          nullptr
        ));
      } else {
        auto first_operand_temp = temp_map_->at(inst.getOperand(0));
        instr_list->Append(new assem::MoveInstr(
          "movq `s0,%rax",
          new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
          new temp::TempList(first_operand_temp)
        ));
      }

      // 将%rax中的值扩展到%rdx:%rax
      instr_list->Append(new assem::OperInstr(
        "cqto",
        new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)),
        new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
        nullptr
      ));
      if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))) {
        // op1: immediate
        auto const_temp = temp::TempFactory::NewTemp();
        instr_list->Append(new assem::OperInstr(
          "movq $" + std::to_string(const_int->getSExtValue()) + ",`d0",
          new temp::TempList(const_temp),
          nullptr,
          nullptr
        ));
        instr_list->Append(new assem::OperInstr(
          "idivq `s0",
          new temp::TempList({reg_manager->GetRegister(frame::X64RegManager::Reg::RAX), reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}),
          new temp::TempList({const_temp, reg_manager->GetRegister(frame::X64RegManager::Reg::RAX), reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}),
          nullptr
        ));
      } else {
        // op1: temp
        auto second_operand_temp = temp_map_->at(inst.getOperand(1));
        instr_list->Append(new assem::OperInstr(
          "idivq `s0",
          new temp::TempList({reg_manager->GetRegister(frame::X64RegManager::Reg::RAX), reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}),
          new temp::TempList({second_operand_temp, reg_manager->GetRegister(frame::X64RegManager::Reg::RAX), reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}),
          nullptr
        ));
      }
      instr_list->Append(new assem::MoveInstr(
        "movq %rax,`d0",
        new temp::TempList(dst_temp),
        new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX))
      ));
    }
    break;

  // 在x86-64中，不区分指针和整数，因此只需要移动临时变量即可
  case llvm::Instruction::PtrToInt:
  // res = ptrtoint op0
    {
      auto dst_temp = temp_map_->at(&inst);
      instr_list->Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(dst_temp),
        new temp::TempList(temp_map_->at(inst.getOperand(0)))
      ));
    }
    break;

  case llvm::Instruction::IntToPtr:
  // res = inttoptr op0
    {
      auto dst_temp = temp_map_->at(&inst);
      instr_list->Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(dst_temp),
        new temp::TempList(temp_map_->at(inst.getOperand(0)))
      ));
    }
    break;

  case llvm::Instruction::GetElementPtr:
  // res = getelementptr op0: struct, op1, op2
  // res = getelementptr op0: int*, op1

    {
      // auto base_ptr = temp_map_->at(inst.getOperand(0));
      auto first_operand = inst.getOperand(0);
      auto src_temp = temp_map_->at(first_operand);
      auto dst_temp = temp_map_->at(&inst);

      if(!first_operand->getType()->isPointerTy()) {
        throw std::runtime_error("Not supported getelementptr instr type");
      }

      if(first_operand->getType()->getPointerElementType()->isStructTy()) {
        // 对于结构进行gep操作
        // NOTE: 暂时认为second_operand, third_operand都为immediate
        auto struct_ty = llvm::cast<llvm::StructType>(first_operand->getType()->getPointerElementType());
        auto second_operand = inst.getOperand(1);
        auto third_operand = inst.getOperand(2);

        auto second_operand_const_int = llvm::cast<llvm::ConstantInt>(second_operand);
        auto third_operand_const_int = llvm::cast<llvm::ConstantInt>(third_operand);

        // 对于结构体来说，起始位置偏移量应该为0
        assert(second_operand_const_int->getSExtValue() == 0);

        auto field_index = third_operand_const_int->getSExtValue();
        
        const llvm::DataLayout &data_layout = inst.getModule()->getDataLayout();
        uint64_t align_size = 0; //* 结构体对齐字节数

        for(auto i = 0U; i < struct_ty->getNumElements(); ++i) {
          auto element_type = struct_ty->getElementType(i);
          auto element_size = data_layout.getTypeAllocSize(element_type);
          
          if(element_size > align_size) {
            align_size = element_size;
          }
        }

        instr_list->Append(new assem::OperInstr(
          "leaq " + std::to_string(align_size * field_index) + "(`s0),`d0",
          new temp::TempList(dst_temp),
          new temp::TempList(src_temp),
          nullptr
        ));
      } else if(first_operand->getType()->getPointerElementType()->isIntegerTy()) {
        // NOTE: 暂时只实现数组元素类型为i32的情况
        auto second_operand = inst.getOperand(1);
        if(auto second_operand_const_int = llvm::dyn_cast<llvm::ConstantInt>(second_operand)) {
          // op1: immediate
          auto index = second_operand_const_int->getSExtValue();
          instr_list->Append(new assem::OperInstr(
            "leaq " + std::to_string(index * 8) + "(`s0),`d0",
            new temp::TempList(dst_temp),
            new temp::TempList(src_temp),
            nullptr
          ));
        } else {
          // op1: temp
          auto second_operand_temp = temp_map_->at(second_operand);
          instr_list->Append(new assem::OperInstr(
            "leaq (`s0,`s1,8),`d0",
            new temp::TempList(dst_temp),
            new temp::TempList({src_temp, second_operand_temp}),
            nullptr
          ));
        }

      } else {
        throw std::runtime_error("Not supported getelementptr instr type");
      }
    }
    break;
  case llvm::Instruction::Store:
  // store op0, op1
    {
      auto type = inst.getOperand(1)->getType()->getPointerElementType();
      if(!type->isIntegerTy() && !type->isPointerTy()) {
        throw std::runtime_error("Not supported store instr type");
      }
      auto dst_temp = temp_map_->at(inst.getOperand(1));
      if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
        // op0: immediate
        instr_list->Append(new assem::OperInstr(
          "movq $" + std::to_string(const_int->getSExtValue()) + ",(`s0)",
          nullptr,
          new temp::TempList(dst_temp),
          nullptr
        ));
      } else if(auto null = llvm::dyn_cast<llvm::ConstantPointerNull>(inst.getOperand(0))) {
        // op0: null
        instr_list->Append(new assem::OperInstr(
          "movq $0,(`s0)",
          nullptr,
          new temp::TempList(dst_temp),
          nullptr
        ));
      } else {
        // op0: temp
        auto src_temp = temp_map_->at(inst.getOperand(0));
        instr_list->Append(new assem::OperInstr(
          "movq `s0,(`s1)",
          nullptr,
          new temp::TempList({src_temp, dst_temp}),
          nullptr
        ));
      }
    }
  break;
  // case llvm::Instruction::BitCast:
  case llvm::Instruction::ZExt:
  // res = zext op0
    {
      auto type = inst.getType();
      if(!type->isIntegerTy()) {
        throw std::runtime_error("Not supported zext instr type");
      }
      auto dst_temp = temp_map_->at(&inst);
      // NOTE: 暂时先只处理op0:temp的情况
      instr_list->Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(dst_temp),
        new temp::TempList(temp_map_->at(inst.getOperand(0)))
      ));
      instr_list->Append(new assem::OperInstr(
        "andq $0x1,`d0",
        new temp::TempList(dst_temp),
        new temp::TempList(dst_temp),
        nullptr
      ));
    }
    break;
  case llvm::Instruction::Call:
    {
      auto call_inst = llvm::cast<llvm::CallInst>(&inst);
      auto regs = reg_manager->ArgRegs();
      auto reg_tmp_iter = regs->GetList().begin();
      auto arg_iter = call_inst->arg_begin();

      auto call_src_lst = new temp::TempList();
      auto call_dst_lst = reg_manager->CallerSaves();

      // 如果llvm指令中调用函数的第一个传参是sp（调用自定义函数），则跳过sp
      // 否则（调用库函数）不跳过
      if(IsRsp(arg_iter->get(), function_name)) {
        ++ arg_iter;
      }

      // 寄存器传参
      for(; arg_iter != call_inst->arg_end() &&
            reg_tmp_iter != regs->GetList().end();
          ++ arg_iter, ++ reg_tmp_iter) {
        llvm::Value *arg = arg_iter->get();
        if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(arg)) {
          // 常数参数
          instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(const_int->getSExtValue()) + ",`d0",
            new temp::TempList(*reg_tmp_iter),
            nullptr,
            nullptr
          ));
        } else {
          // 临时变量参数
          if(IsRsp(arg, function_name)) {
            instr_list->Append(new assem::OperInstr(
              "movq %rsp,`d0",
              new temp::TempList(*reg_tmp_iter),
              nullptr,
              nullptr
            ));
          } else {
            instr_list->Append(new assem::MoveInstr(
              "movq `s0,`d0",
              new temp::TempList(*reg_tmp_iter),
              new temp::TempList(temp_map_->at(arg))
            ));
          }
        }
        call_src_lst->Append(*reg_tmp_iter);
      }

      // 栈传参
      // TODO: 栈传参的参数应该倒序压栈
      if (arg_iter != call_inst->arg_end()) {
        for(; arg_iter != call_inst->arg_end(); ++ arg_iter) {
          llvm::Value *arg = arg_iter->get();
          if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(arg)) {
            // 常数参数，先将常数移到寄存器
            auto dest_temp = temp::TempFactory::NewTemp();
            instr_list->Append(new assem::OperInstr(
              "movq $" + std::to_string(const_int->getSExtValue()) + ",`d0",
              new temp::TempList(dest_temp),
              nullptr,
              nullptr
            ));
            // NOTE: 此处不需要将dest_temp加入temp_map，因为后续不会用到这个临时变量
            // TODO: 模拟器没有实现pushq指令，需要修改
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
        call_dst_lst,
        call_src_lst,
        nullptr
      ));

      // 如果函数有返回值
      // 将%rax中的返回值存入临时变量
      if(!call_inst->getType()->isVoidTy()) {
        auto res_temp = temp_map_->at(&inst);
        instr_list->Append(new assem::MoveInstr(
          "movq %rax,`d0",
          new temp::TempList(res_temp),
          new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX))
        ));
      }

    }
    break;
  case llvm::Instruction::Ret:
  // ret void
  // ret op0
    {
      if(inst.getNumOperands() > 0) {
        // 有返回值
        if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
          // 返回值为常数
          instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(const_int->getSExtValue()) + ",%rax",
            new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
            nullptr,
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
      }
      
      std::string ret_label_name = std::string(function_name) + "_ret";
      instr_list->Append(new assem::OperInstr(
        "jmp " + ret_label_name,
        nullptr,
        nullptr,
        new assem::Targets(new std::vector<temp::Label *>({temp::LabelFactory::NamedLabel(ret_label_name)}))
      ));
    }
    break;
  case llvm::Instruction::Br:
  // br label
  // br cond, true_label, false_label
    {
      auto br_inst = llvm::cast<llvm::BranchInst>(&inst);
      if(br_inst->isConditional()) {
        // 条件分支
        auto condition = br_inst->getCondition();
        auto true_label = br_inst->getSuccessor(0);
        auto false_label = br_inst->getSuccessor(1);

        auto condition_temp = temp_map_->at(condition);
        instr_list->Append(new assem::OperInstr(
          "andq $1,`d0",
          new temp::TempList(condition_temp),
          new temp::TempList(condition_temp),
          nullptr
        ));
        instr_list->Append(new assem::OperInstr(
          "cmpq $1,`s0",
          nullptr,
          new temp::TempList(condition_temp),
          nullptr
        ));
        // 为了在phi中追踪跳转来源，在跳转前将bb index移动到%rax
        instr_list->Append(new assem::OperInstr(
          "movq $" + std::to_string(bb_map_->at(bb)) + ",%rax",
          new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
          nullptr,
          nullptr
        ));
        instr_list->Append(new assem::OperInstr(
          "je " + std::string(true_label->getName()),
          nullptr,
          nullptr,
          new assem::Targets(new std::vector<temp::Label *>{temp::LabelFactory::NamedLabel(true_label->getName())})
        ));
        // NOTE: 构造OperInstr传入target
        instr_list->Append(new assem::OperInstr(
          "jmp " + std::string(false_label->getName()),
          nullptr,
          nullptr,
          new assem::Targets(new std::vector<temp::Label *>{temp::LabelFactory::NamedLabel(false_label->getName())})
        ));
      } else {
        // 无条件分支
        // 为了在phi中追踪跳转来源，在跳转前将bb index移动到%rax
        instr_list->Append(new assem::OperInstr(
          "movq $" + std::to_string(bb_map_->at(bb)) + ",%rax",
          new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
          nullptr,
          nullptr
        ));

        auto target_label = br_inst->getSuccessor(0);
        instr_list->Append(new assem::OperInstr(
          "jmp " + std::string(target_label->getName()),
          nullptr,
          nullptr,
          new assem::Targets(new std::vector<temp::Label *>{temp::LabelFactory::NamedLabel(target_label->getName())})
        ));
      }
    }
    break;
  case llvm::Instruction::ICmp:
  // res = icmp COND op0,op1
    {
      // NOTE: 假设op0: temp, op1: temp/immediate
      auto icmp_inst = llvm::cast<llvm::ICmpInst>(&inst);
      auto first_temp = temp_map_->at(icmp_inst->getOperand(0));
      if(auto const_int_second_operand = llvm::dyn_cast<llvm::ConstantInt>(icmp_inst->getOperand(1))) {
        // op1: immediate
        instr_list->Append(new assem::OperInstr(
          // x86-64实际上根据`s0 - `s1的结果设置CC
          "cmpq $" + std::to_string(const_int_second_operand->getSExtValue()) + ",`s0",
          nullptr,
          new temp::TempList(first_temp),
          nullptr
        ));
      } else {
        // op1: temp
        auto second_temp = temp_map_->at(icmp_inst->getOperand(1));
        instr_list->Append(new assem::OperInstr(
          // x86-64实际上根据`s0 - `s1的结果设置CC
          "cmpq `s1,`s0",
          nullptr,
          new temp::TempList({first_temp, second_temp}),
          nullptr
        ));
      }
      auto res_temp = temp_map_->at(&inst);
      auto pred = icmp_inst->getPredicate();
      std::string set_instr;
      switch(pred) {
        case llvm::ICmpInst::ICMP_EQ:
          set_instr = "sete";
          break;
        case llvm::ICmpInst::ICMP_NE:
          set_instr = "setne";
          break;
        case llvm::ICmpInst::ICMP_SGT:
          set_instr = "setg";
          break;
        case llvm::ICmpInst::ICMP_SGE:
          set_instr = "setge";
          break;
        case llvm::ICmpInst::ICMP_SLT:
          set_instr = "setl";
          break;
        case llvm::ICmpInst::ICMP_SLE:
          set_instr = "setle";
          break;
        default:
          throw std::runtime_error("Unsupported icmp predicate");
      }

      instr_list->Append(new assem::OperInstr(
        set_instr + " `d0",
        new temp::TempList(res_temp),
        nullptr,
        nullptr
      ));
    }
    break;
  case llvm::Instruction::PHI:
    {
      auto phi_inst = llvm::cast<llvm::PHINode>(&inst);
      auto dst_temp = temp_map_->at(&inst);
      auto num_incoming_values = phi_inst->getNumIncomingValues();
      auto jmp_label_names = std::vector<std::string>();
      auto end_label_name = std::string(bb->getName().data()) + "_end";

      // 生成随机并唯一的跳转label列表
      for(int i = 0;i < num_incoming_values; ++i) {
        auto jmp_label_name = bb->getName().data() + std::to_string(rand());
        jmp_label_names.push_back(std::move(jmp_label_name));
      }

      // 生成比较和跳转指令
      for(int i = 0;i < num_incoming_values; ++i) {
        instr_list->Append(new assem::OperInstr(
          "cmpq $" + std::to_string(bb_map_->at(phi_inst->getIncomingBlock(i))) + ",%rax",
          nullptr,
          new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
          nullptr
        ));
        instr_list->Append(new assem::OperInstr(
          "je " + jmp_label_names[i],
          nullptr,
          nullptr,
          new assem::Targets(new std::vector<temp::Label *>{temp::LabelFactory::NamedLabel(jmp_label_names[i])})
        ));
      }

      // 生成每个jmp_label的代码
      for(int i = 0;i < num_incoming_values; ++i) {
        instr_list->Append(new assem::LabelInstr(
          jmp_label_names[i]
        ));
        auto incoming_value = phi_inst->getIncomingValue(i);
        if(auto const_int = llvm::dyn_cast<llvm::ConstantInt>(incoming_value)) {
          // incoming value: immediate
          instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(const_int->getSExtValue()) + ",`d0",
            new temp::TempList(dst_temp),
            nullptr,
            nullptr
          ));
        } else if(llvm::isa<llvm::ConstantPointerNull>(incoming_value)) {
          // incoming value: nil
          instr_list->Append(new assem::OperInstr(
            "movq $0,`d0",
            new temp::TempList(dst_temp),
            nullptr,
            nullptr
          ));
        } else {
          // incoming value: temp(not null)
          auto incoming_value_temp = temp_map_->at(incoming_value);
          instr_list->Append(new assem::MoveInstr(
            "movq `s0,`d0",
            new temp::TempList(dst_temp),
            new temp::TempList(incoming_value_temp)
          ));
        }

        instr_list->Append(new assem::OperInstr(
          "jmp `j0",
          nullptr,
          nullptr,
          new assem::Targets(new std::vector<temp::Label *>{temp::LabelFactory::NamedLabel(end_label_name)})
        ));
      }

      // 追加end_label标签
      instr_list->Append(new assem::LabelInstr(
        end_label_name
      ));

    }
    break;
  default:
    break;
  }

  // throw std::runtime_error(std::string("Unknown instruction: ") +
  //                          inst.getOpcodeName());
}

} // namespace cg