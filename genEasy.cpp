#include <iostream>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <map>

std::map<std::string, llvm::Function *> functions;

llvm::Function *addFunction(std::shared_ptr<llvm::Module> ir_module, std::string name, llvm::FunctionType *type)
{
    llvm::Function *function = llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, ir_module.get());
    for (auto &arg : function->args())
        arg.addAttr(llvm::Attribute::NoUndef);
    functions.insert(std::make_pair(name, function));
    return function;
}

void buildGlobal(std::shared_ptr<llvm::Module> ir_module)
{
}

void buildMain(std::shared_ptr<llvm::Module> ir_module)
{
    llvm::IRBuilder<> builder(ir_module->getContext());

    llvm::Function *main = addFunction(ir_module, "main", llvm::FunctionType::get(llvm::Type::getInt32Ty(ir_module->getContext()), false));
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ir_module->getContext(), "", main);
    llvm::BasicBlock *trueDest = llvm::BasicBlock::Create(ir_module->getContext(), "true_dest", main);
    llvm::BasicBlock *falseDest = llvm::BasicBlock::Create(ir_module->getContext(), "false_dest", main);

    builder.SetInsertPoint(entry);

    llvm::AllocaInst *alloca1 = builder.CreateAlloca(builder.getInt32Ty());
    alloca1->setAlignment(llvm::Align(4));
    llvm::AllocaInst *alloca2 = builder.CreateAlloca(builder.getInt32Ty());
    alloca2->setAlignment(llvm::Align(4));
    llvm::AllocaInst *alloca3 = builder.CreateAlloca(builder.getInt32Ty());
    alloca3->setAlignment(llvm::Align(4));

    builder.CreateStore(builder.getInt32(0), alloca1)->setAlignment(llvm::Align(4));
    builder.CreateStore(builder.getInt32(1), alloca2)->setAlignment(llvm::Align(4));
    builder.CreateStore(builder.getInt32(2), alloca3)->setAlignment(llvm::Align(4));

    llvm::LoadInst *load4 = builder.CreateLoad(builder.getInt32Ty(), alloca1);
    load4->setAlignment(llvm::Align(4));
    llvm::LoadInst *load5 = builder.CreateLoad(builder.getInt32Ty(), alloca2);
    load5->setAlignment(llvm::Align(4));
    llvm::LoadInst *load6 = builder.CreateLoad(builder.getInt32Ty(), alloca3);
    load6->setAlignment(llvm::Align(4));

    llvm::Value *cmp = builder.CreateCmp(llvm::CmpInst::Predicate::ICMP_SLT, load5, load6);
    
    builder.CreateCondBr(cmp, trueDest, falseDest);
    

    builder.SetInsertPoint(trueDest);

    builder.CreateStore(builder.getInt32(3), alloca3)->setAlignment(llvm::Align(4));

    builder.CreateBr(falseDest);


    builder.SetInsertPoint(falseDest);

    llvm::LoadInst *load9 = builder.CreateLoad(builder.getInt32Ty(), alloca2);
    load9->setAlignment(llvm::Align(4));
    llvm::LoadInst *load10 = builder.CreateLoad(builder.getInt32Ty(), alloca3);
    load10->setAlignment(llvm::Align(4));

    llvm::Value *add11 = builder.CreateAdd(load9, load10, "", false, true);

    builder.CreateRet(add11);
}

void buildFunction(std::shared_ptr<llvm::Module> ir_module)
{
    buildMain(ir_module);
}

int main()
{
    llvm::LLVMContext context;
    std::shared_ptr<llvm::Module> ir_module = std::make_shared<llvm::Module>("easy", context);
    ir_module->setTargetTriple("x86_64-pc-linux-gnu");

    buildGlobal(ir_module);
    buildFunction(ir_module);

    ir_module->print(llvm::outs(), nullptr);

    return 0;
}