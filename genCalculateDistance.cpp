#include <iostream>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <map>

std::map<std::string, llvm::StructType *> struct_types;
std::map<std::string, llvm::GlobalVariable *> global_values;
std::map<std::string, llvm::Function *> functions;

llvm::StructType *addStructType(std::shared_ptr<llvm::Module> ir_module, std::string name, std::vector<llvm::Type *> fields)
{
    llvm::StructType *struct_type = llvm::StructType::create(ir_module->getContext(), name);
    struct_type->setBody(fields);
    struct_types.insert(std::make_pair(name, struct_type));
    return struct_type;
}

llvm::GlobalVariable *addGlobalValue(std::shared_ptr<llvm::Module> ir_module, std::string name, llvm::Type *type, llvm::Constant *initializer, int align)
{
    llvm::GlobalVariable *global = (llvm::GlobalVariable *)ir_module->getOrInsertGlobal(name, type);
    global->setInitializer(initializer);
    global->setDSOLocal(true);
    global->setAlignment(llvm::MaybeAlign(align));
    global_values.insert(std::make_pair(name, global));
    return global;
}

llvm::GlobalVariable *addGlobalString(std::shared_ptr<llvm::Module> ir_module, std::string name, std::string value)
{
    llvm::GlobalVariable *global = (llvm::GlobalVariable *)ir_module->getOrInsertGlobal(name, llvm::ArrayType::get(llvm::Type::getInt8Ty(ir_module->getContext()), value.size() + 1));
    global->setInitializer(llvm::ConstantDataArray::getString(ir_module->getContext(), value, true));
    global->setDSOLocal(true);
    global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    global->setLinkage(llvm::GlobalValue::LinkageTypes::PrivateLinkage);
    global->setConstant(true);
    global->setAlignment(llvm::MaybeAlign(1));
    global_values.insert(std::make_pair(name, global));
    return global;
}

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
    llvm::IRBuilder<> ir_builder(ir_module->getContext());

    // add struct type
    llvm::Type *Edge_s_fields[] = {llvm::Type::getInt32Ty(ir_module->getContext()), llvm::Type::getInt32Ty(ir_module->getContext()), llvm::Type::getInt64Ty(ir_module->getContext())};
    llvm::StructType *Edge_s = addStructType(ir_module, "struct.Edge_s", std::vector<llvm::Type *>(Edge_s_fields, Edge_s_fields + 3));

    // add global value
    llvm::PointerType::get(Edge_s, 0);
    llvm::GlobalVariable *edge1 = addGlobalValue(ir_module, "edge1", Edge_s, llvm::ConstantStruct::get(Edge_s, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_module->getContext()), 5)), 8);
    llvm::GlobalVariable *edge2 = addGlobalValue(ir_module, "edge2", Edge_s, llvm::ConstantStruct::get(Edge_s, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_module->getContext()), 10)), 8);
    addGlobalValue(ir_module, "allDist", llvm::ArrayType::get(llvm::ArrayType::get(llvm::Type::getInt32Ty(ir_module->getContext()), 3), 3), llvm::ConstantAggregateZero::get(llvm::ArrayType::get(llvm::ArrayType::get(llvm::Type::getInt32Ty(ir_module->getContext()), 3), 3)), 16);
    addGlobalValue(ir_module, "dist", llvm::ArrayType::get(llvm::PointerType::get(Edge_s, 0), 3), llvm::ConstantArray::get(llvm::ArrayType::get(llvm::PointerType::get(Edge_s, 0), 3), {llvm::ConstantExpr::getBitCast(edge1, llvm::PointerType::get(Edge_s, 0)), llvm::ConstantExpr::getBitCast(edge2, llvm::PointerType::get(Edge_s, 0)), llvm::ConstantPointerNull::get(llvm::PointerType::get(Edge_s, 0))}), 16);
    addGlobalValue(ir_module, "minDistance", llvm::Type::getInt64Ty(ir_module->getContext()), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_module->getContext()), 5), 8);

    // add global string
    llvm::GlobalVariable *str = addGlobalString(ir_module, ".str", "%lld\00");
    llvm::GlobalVariable *str1 = addGlobalString(ir_module, ".str1", "%lld %lld %d\n\00");

    // add external function
    addFunction(ir_module, "__isoc99_scanf", llvm::FunctionType::get(llvm::Type::getInt32Ty(ir_module->getContext()), llvm::PointerType::get(llvm::Type::getInt8Ty(ir_module->getContext()), 0), true));
    addFunction(ir_module, "printf", llvm::FunctionType::get(llvm::Type::getInt32Ty(ir_module->getContext()), llvm::PointerType::get(llvm::Type::getInt8Ty(ir_module->getContext()), 0), true));
}

void buildCaculateDistance(std::shared_ptr<llvm::Module> ir_module)
{
    llvm::IRBuilder<> builder(ir_module->getContext());

    llvm::Function *caculateDistance = addFunction(ir_module, "caculateDistance", llvm::FunctionType::get(llvm::Type::getVoidTy(ir_module->getContext()), false));
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ir_module->getContext(), "", caculateDistance);
    builder.SetInsertPoint(entry);

    // TODO
}

void buildMain(std::shared_ptr<llvm::Module> ir_module)
{
    llvm::IRBuilder<> builder(ir_module->getContext());

    llvm::Function *main = addFunction(ir_module, "main", llvm::FunctionType::get(llvm::Type::getInt32Ty(ir_module->getContext()), false));
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ir_module->getContext(), "", main);
    builder.SetInsertPoint(entry);
    
    // TODO
}

void buildFunction(std::shared_ptr<llvm::Module> ir_module)
{
    buildCaculateDistance(ir_module);
    buildMain(ir_module);
}

int main(int, char **)
{
    llvm::LLVMContext context;
    std::shared_ptr<llvm::Module> ir_module = std::make_shared<llvm::Module>("calculateDistance", context);
    ir_module->setTargetTriple("x86_64-pc-linux-gnu");

    buildGlobal(ir_module);
    buildFunction(ir_module);

    ir_module->print(llvm::outs(), nullptr);

    return 0;
}
