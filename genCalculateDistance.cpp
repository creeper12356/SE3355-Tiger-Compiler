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
    llvm::BasicBlock *label3 = llvm::BasicBlock::Create(ir_module->getContext(), "", caculateDistance);
    llvm::BasicBlock *label6 = llvm::BasicBlock::Create(ir_module->getContext(), "", caculateDistance);
    llvm::BasicBlock *label16 = llvm::BasicBlock::Create(ir_module->getContext(), "", caculateDistance);
    llvm::BasicBlock *label18 = llvm::BasicBlock::Create(ir_module->getContext(), "", caculateDistance);
    llvm::BasicBlock *label20 = llvm::BasicBlock::Create(ir_module->getContext(), "", caculateDistance);
    llvm::BasicBlock *label22 = llvm::BasicBlock::Create(ir_module->getContext(), "", caculateDistance);
    llvm::BasicBlock *label25 = llvm::BasicBlock::Create(ir_module->getContext(), "", caculateDistance);

    auto gDist = global_values["dist"];
    auto gMinDistance = global_values["minDistance"];
    auto sEdge_s = struct_types["struct.Edge_s"];

    builder.SetInsertPoint(entry);
    auto alloca1 = builder.CreateAlloca(builder.getInt32Ty());
    alloca1->setAlignment(llvm::Align(4));
    auto alloca2 = builder.CreateAlloca(builder.getInt64Ty());
    alloca2->setAlignment(llvm::Align(8));
    builder.CreateStore(builder.getInt32(0), alloca1)
            ->setAlignment(llvm::Align(4));
    builder.CreateBr(label3);

    builder.SetInsertPoint(label3);
    auto load4 = builder.CreateLoad(builder.getInt32Ty(), alloca1);
    load4->setAlignment(llvm::Align(4));
    auto cmp5 = builder.CreateCmp(llvm::CmpInst::Predicate::ICMP_SLT, load4, builder.getInt32(3));
    builder.CreateCondBr(cmp5, label6, label25);

    builder.SetInsertPoint(label6);
    auto load7 = builder.CreateLoad(builder.getInt32Ty(), alloca1);
    auto sext8 = builder.CreateSExt(load7, builder.getInt64Ty());

    std::cout << "gDist dump: " << std::endl;
    gDist->getType()->dump();
    
    auto gep9 = builder.CreateGEP(
        gDist->getType(),
        gDist,
        {builder.getInt64(0), sext8}
    );

    auto load10 = builder.CreateLoad(llvm::PointerType::get(sEdge_s, 0), gep9);
    load10->setAlignment(llvm::Align(8));

    std::cout << "load10 dump: " << std::endl;
    load10->getType()->dump();
    
    auto gep11 = builder.CreateGEP(
        sEdge_s,
        load10,
        {builder.getInt32(0), builder.getInt32(2)}
    );
    auto load12 = builder.CreateLoad(builder.getInt64Ty(), gep11);
    load12->setAlignment(llvm::Align(4));
    builder.CreateStore(load12, gep11)
            ->setAlignment(llvm::Align(4));
    auto load13 = builder.CreateLoad(builder.getInt64Ty(), gep11);
    load13->setAlignment(llvm::Align(4));
    auto load14 = builder.CreateLoad(builder.getInt64Ty(), gMinDistance);
    load14->setAlignment(llvm::Align(4));
    auto cmp15 = builder.CreateCmp(llvm::CmpInst::Predicate::ICMP_SLT, load13, load14);
    builder.CreateCondBr(cmp15, label16, label18);

    builder.SetInsertPoint(label16);
    auto load17 = builder.CreateLoad(builder.getInt64Ty(), alloca2);
    load17->setAlignment(llvm::Align(4));
    builder.CreateBr(label20);

    builder.SetInsertPoint(label18);
    auto load19 = builder.CreateLoad(builder.getInt64Ty(), gMinDistance);
    load19->setAlignment(llvm::Align(4));
    builder.CreateBr(label20);

    builder.SetInsertPoint(label20);
    auto phi21 = builder.CreatePHI(builder.getInt64Ty(), 2);
    phi21->addIncoming(load17, label16);
    phi21->addIncoming(load19, label18);
    builder.CreateStore(phi21, gMinDistance)
            ->setAlignment(llvm::Align(4));
    builder.CreateBr(label22);

    builder.SetInsertPoint(label22);
    auto load23 = builder.CreateLoad(builder.getInt32Ty(), alloca1);
    load23->setAlignment(llvm::Align(4));
    auto add24 = builder.CreateAdd(load23, builder.getInt32(1), "", false, true);
    builder.CreateStore(add24, alloca1)
            ->setAlignment(llvm::Align(4));
    builder.CreateBr(label3);

    builder.SetInsertPoint(label25);
    builder.CreateRetVoid();
}

void buildMain(std::shared_ptr<llvm::Module> ir_module)
{
    llvm::IRBuilder<> builder(ir_module->getContext());

    llvm::Function *main = addFunction(ir_module, "main", llvm::FunctionType::get(llvm::Type::getInt32Ty(ir_module->getContext()), false));
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ir_module->getContext(), "", main);
    builder.SetInsertPoint(entry);

    auto sEdge_s = struct_types["struct.Edge_s"];

    auto alloca1 = builder.CreateAlloca(builder.getInt32Ty());
    alloca1->setAlignment(llvm::Align(4));
    auto alloca2 = builder.CreateAlloca(sEdge_s);
    alloca2->setAlignment(llvm::Align(8));
    builder.CreateStore(builder.getInt32(0), alloca1)
            ->setAlignment(llvm::Align(4));
    auto gep3 = builder.CreateGEP(
        sEdge_s,
        alloca2,
        {builder.getInt32(0), builder.getInt32(0)}
    );
    //   %4 = call i32 (i8*, ...) @__isoc99_scanf(i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str, i64 0, i64 0), i64* %3)
    auto call4 = builder.CreateCall(
        functions["__isoc99_scanf"],
        {
            builder.CreateGEP(
                global_values[".str"]->getType(),
                global_values[".str"],
                {builder.getInt64(0), builder.getInt64(0)}
            ),
            alloca1
        }
    );
    auto gep5 = builder.CreateGEP(
        sEdge_s,
        alloca2,
        {builder.getInt32(0), builder.getInt32(2)}
    );
    auto load6 = builder.CreateLoad(builder.getInt64Ty(), gep5);
    load6->setAlignment(llvm::Align(4));
    auto trunc7 = builder.CreateTrunc(load6, builder.getInt32Ty());
    //   store i32 %7, i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @allDist, i64 0, i64 0, i64 0), align 4
    builder.CreateStore(trunc7, builder.CreateGEP(
        global_values["allDist"]->getType(),
        global_values["allDist"],
        {builder.getInt64(0), builder.getInt64(0), builder.getInt64(0)}
    )) ->setAlignment(llvm::Align(4));
    builder.CreateCall(
        functions["caculateDistance"],
        {}
    );
    auto load8 = builder.CreateLoad(builder.getInt64Ty(), global_values["minDistance"]);
    load8->setAlignment(llvm::Align(4));
    auto gep9 = builder.CreateGEP(
        sEdge_s,
        alloca2,
        {builder.getInt32(0), builder.getInt32(2)}
    );
    auto load10 = builder.CreateLoad(builder.getInt64Ty(), gep9);
    load10->setAlignment(llvm::Align(4));
    auto add11 = builder.CreateAdd(load10, builder.getInt64(5), "", false, true);
    auto add12 = builder.CreateAdd(add11, builder.getInt64(10), "", false, true);
    auto load13 = builder.CreateLoad(
        builder.getInt32Ty(),
        builder.CreateGEP(
            global_values["allDist"]->getType(),
            global_values["allDist"],
            {builder.getInt64(0), builder.getInt64(0), builder.getInt64(0)}
        )
    );
    load13->setAlignment(llvm::Align(4));
    auto call14 = builder.CreateCall(
        functions["printf"],
        {
            builder.CreateGEP(
                global_values[".str1"]->getType(),
                global_values[".str1"],
                {builder.getInt64(0), builder.getInt64(0)}
            ),
            load8,
            add12,
            load13
        }
    );
    builder.CreateRet(builder.getInt32(0));
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
