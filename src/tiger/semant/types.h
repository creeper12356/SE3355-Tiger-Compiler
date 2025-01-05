#ifndef TIGER_SEMANT_TYPES_H_
#define TIGER_SEMANT_TYPES_H_

#include "tiger/symbol/symbol.h"
#include <list>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <string>

namespace type {

class TyList;
class Field;
class FieldList;

class Ty {
public:
  virtual Ty *ActualTy();
  virtual bool IsSameType(Ty *);
  virtual bool IsStrictSameType(Ty *);
  virtual llvm::Type *GetLLVMType();

protected:
  Ty() = default;
};

class NilTy : public Ty {
public:
  static NilTy *Instance() { return &nilty_; }
  llvm::Type *GetLLVMType() override;

private:
  static NilTy nilty_;
};

class IntTy : public Ty {
public:
  static IntTy *Instance() { return &intty_; }
  llvm::Type *GetLLVMType() override;

private:
  static IntTy intty_;
};

  // LLVM string: {i32, i8*}
class StringTy : public Ty {
public:
  static StringTy *Instance() { return &stringty_; }

  /**
   * @brief 生成关于String的LLVM定义
   */
  static void InitStringLLVMType();
  llvm::Type *GetLLVMType() override;
  static llvm::Value *CreateGlobalStringStructPtr(std::string str);

private:
  static StringTy stringty_;
  static llvm::StructType *string_llvm_type_;
  static llvm::PointerType *string_ptr_llvm_type_;
};

class VoidTy : public Ty {
public:
  static VoidTy *Instance() { return &voidty_; }
  llvm::Type *GetLLVMType() override;

private:
  static VoidTy voidty_;
};

class RecordTy : public Ty {
public:
  FieldList *fields_;
  llvm::Type *GetLLVMType() override;
  explicit RecordTy(FieldList *fields) : fields_(fields) { llvm_type_ = NULL; }

private:
  llvm::Type *llvm_type_;
};

class ArrayTy : public Ty {
public:
  Ty *ty_;

  /**
   * @brief 指针类型
   * 
   * @return llvm::Type* 
   */
  llvm::Type *GetLLVMType() override;

  /**
   * @brief 数组类型
   * 
   * @param len 
   * @return llvm::Type* 
   */
  llvm::Type *GetLLVMTypeWithLen(int len);
  explicit ArrayTy(Ty *ty) : ty_(ty) {}
};

class NameTy : public Ty {
public:
  sym::Symbol *sym_;
  Ty *ty_;
  llvm::Type *GetLLVMType() override;
  NameTy(sym::Symbol *sym, Ty *ty) : sym_(sym), ty_(ty) {}

  Ty *ActualTy() override;
};

class TyList {
public:
  TyList() = default;
  explicit TyList(Ty *ty) : ty_list_({ty}) {}
  TyList(std::initializer_list<Ty *> list) : ty_list_(list) {}

  const std::list<Ty *> &GetList() { return ty_list_; }
  void Append(Ty *ty) { ty_list_.push_back(ty); }

private:
  std::list<Ty *> ty_list_;
};

class Field {
public:
  sym::Symbol *name_;
  Ty *ty_;

  Field(sym::Symbol *name, Ty *ty) : name_(name), ty_(ty) {}
};

class FieldList {
public:
  FieldList() = default;
  explicit FieldList(Field *field) : field_list_({field}) {}

  std::list<Field *> &GetList() { return field_list_; }
  void Append(Field *field) { field_list_.push_back(field); }

private:
  std::list<Field *> field_list_;
};

} // namespace type

#endif // TIGER_SEMANT_TYPES_H_
