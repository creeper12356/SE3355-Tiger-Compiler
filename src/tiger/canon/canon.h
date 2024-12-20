#ifndef TIGER_CANON_CANON_H_
#define TIGER_CANON_CANON_H_

#include <cstdio>
#include <list>
#include <memory>
#include <stdexcept>
#include <vector>

#include "tiger/frame/temp.h"
#include <llvm/IR/Function.h>

// Forward Declarations
namespace tree {
class StmList;
class Exp;
class Stm;
} // namespace tree

namespace frame {
class ProcFrag;
}

namespace canon {

class BasicBlockList {
  friend class Canon;

public:
  BasicBlockList() = default;

  void Append(llvm::BasicBlock *basicblock) {
    basicblock_list_.push_back(basicblock);
  }
  [[nodiscard]] const std::list<llvm::BasicBlock *> &GetList() const {
    return basicblock_list_;
  }

private:
  std::list<llvm::BasicBlock *> basicblock_list_;
};

/**
 * @brief 执行轨迹，一系列连续执行的基本块
 * 
 */
class Traces {
public:
  Traces() = delete;
  explicit Traces(llvm::Function *body, BasicBlockList *bb,
                  std::string_view function_name)
      : body_(body), basicblock_list_(bb), function_name_(function_name) {
    if (bb == nullptr)
      throw std::invalid_argument("NULL pointer is not allowed in Traces");
  }
  Traces(const Traces &traces) = delete;
  Traces(Traces &&traces) = delete;
  Traces &operator=(const Traces &traces) = delete;
  Traces &operator=(Traces &&traces) = delete;
  ~Traces();

  [[nodiscard]] BasicBlockList *GetBasicBlockList() const {
    return basicblock_list_;
  }

  [[nodiscard]] std::string_view GetFunctionName() const {
    return function_name_;
  }

  [[nodiscard]] llvm::Function *GetBody() const { return body_; }

private:
  llvm::Function *body_;
  BasicBlockList *basicblock_list_;
  std::string function_name_;
};

class Canon {
  friend class frame::ProcFrag;

public:
  Canon() = delete;
  explicit Canon(llvm::Function *body) : body_(body) {}

  /**
   * Tree.stm_ list -> (Tree.stm_ list list * Tree.label_)
   * From a list of cleaned trees, produce a list of
   * basic blocks satisfying the following properties:
   * 1. and 2. as above;
   * 3.  Every block_ begins with a LABEL;
   * 4.  A LABEL appears only at the beginning of a block_;
   * 5.  Any JUMP or CJUMP is the last stm_ in a block_;
   * 6.  Every block_ ends with a JUMP or CJUMP;
   * Also produce the "label_" to which control will be passed
   * upon exit.
   * @return basic blocks
   */
  canon::BasicBlockList *BasicBlocks();

  /**
   * Transfer the ownership of traces_ to outer scope
   * @return unique pointer of traces_
   */
  std::unique_ptr<Traces> TransferTraces() { return std::move(traces_); }

private:
  // IR tree in a process fragment
  llvm::Function *body_;
  // Canonical tree generated
  BasicBlockList *bbs_;
  // Function names
  std::list<std::string> function_names_;
  std::unique_ptr<Traces> traces_;
};

} // namespace canon
#endif