#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

namespace ra {

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result() {}
};

class RegAllocator {
  /* TODO: Put your lab6 code here */
public:
    // ra::RegAllocator reg_allocator(body_->getName().str(),
                                  //  std::move(assem_instr));
  RegAllocator(std::string code, std::unique_ptr<cg::AssemInstr> assem_instr)
      : code_(code), assem_instr_(std::move(assem_instr)) {}
  void RegAlloc();
  std::unique_ptr<ra::Result> TransferResult();
  
private:
  void AddEdge(live::INodePtr u, live::INodePtr v);
  fg::FGraphPtr BuildCFG();
  void MakeWorklist();
  void DecrementDegree(live::INodePtr node);
  void EnableMoves(live::INodeListPtr nodes);
  void Simplify();
  void Coalesce();
  void AddWorkList(live::INodePtr node);
  bool OK(live::INodePtr t, live::INodePtr r);
  bool Conservative(live::INodeListPtr nodes);
  live::INodePtr GetAlias(live::INodePtr node);
  void Combine(live::INodePtr u, live::INodePtr v);
  void Freeze();
  void FreezeMoves(live::INodePtr node);
  void SelectSpill();

  void AssignColors();

  live::INodeListPtr Adjacent(live::INodePtr node);
  live::MoveList *NodeMoves(live::INodePtr node) ;
  bool MoveRelated(live::INodePtr node) ;

private:
  std::string code_;
  std::unique_ptr<cg::AssemInstr> assem_instr_;
  std::unique_ptr<ra::Result> result_;

  live::INodeList precolored_;
  live::INodeList initial_;
  live::INodeList simplify_worklist_;
  live::INodeList freeze_worklist_;
  live::INodeList spill_worklist_;
  live::INodeList spilled_nodes_;
  live::INodeList coalesced_nodes_;
  live::INodeList colored_nodes_;
  live::INodeList select_stack_;


  live::MoveList *coalesced_moves_;
  live::MoveList *constrained_moves_;
  live::MoveList *frozen_moves_;
  live::MoveList *worklist_moves_;
  live::MoveList *active_moves_;

  // live graph
  live::LiveGraph live_graph_ = {nullptr, nullptr};

  std::set<std::pair<live::INodePtr, live::INodePtr>> adj_set_;
  std::map<live::INodePtr, int> degree_map_;
  std::map<live::INodePtr, live::MoveList *> move_list_map_;
  std::map<live::INodePtr, live::INodePtr> alias_map_;

};

} // namespace ra

#endif