#include "tiger/liveness/liveness.h"

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  auto instr_nodes = flowgraph_->Nodes()->GetList();
  for(const auto &instr_node: instr_nodes) {
    in_->Enter(instr_node, new temp::TempList());
    out_->Enter(instr_node, new temp::TempList());
  }

  bool is_stable;
  do {
    is_stable = true;

    for(const auto &instr_node: instr_nodes) {
      auto in = in_->Look(instr_node);
      auto out = out_->Look(instr_node);
      auto succs = instr_node->Succ();
      auto def = instr_node->NodeInfo()->Def();
      auto use = instr_node->NodeInfo()->Use();

      auto new_in = out->Diff(def)->Union(use);
      auto new_out = new temp::TempList();
      for(const auto &succ: succs->GetList()) {
        new_out = new_out->Union(in_->Look(succ));
      }

      is_stable = is_stable && in->Equal(new_in) && out->Equal(new_out);
      in_->Enter(instr_node, new_in);
      out_->Enter(instr_node, new_out);
    }
  } while(!is_stable);
  
  in_->Dump([](fg::FNodePtr node, temp::TempList *tl) {
    auto map = temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
    node->NodeInfo()->Print(stdout, map);
    printf("in: ");
    for(auto temp: tl->GetList()) {
      printf("%s ", map->Look(temp)->c_str());
    }
    printf("\n\n");
  });

  out_->Dump([](fg::FNodePtr node, temp::TempList *tl) {
    auto map = temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
    node->NodeInfo()->Print(stdout, map);
    printf("out: ");
    for(auto temp: tl->GetList()) {
      printf("%s ", map->Look(temp)->c_str());
    }
    printf("\n\n");
  });
}

void LiveGraphFactory::InterfGraph() {
  /* TODO: Put your lab6 code here */
  auto instr_nodes = flowgraph_->Nodes()->GetList();
  // 为每个临时变量创建一个节点，
  // 并建立临时变量到节点的映射
  for(const auto &instr_node: instr_nodes) {
    auto use = instr_node->NodeInfo()->Use() ? instr_node->NodeInfo()->Use()->GetList() : std::list<temp::Temp *>();
    auto def = instr_node->NodeInfo()->Def() ? instr_node->NodeInfo()->Def()->GetList() : std::list<temp::Temp *>();

    for(auto use_temp: use) {
      if(temp_node_map_->Look(use_temp) == nullptr) {
        auto new_node = live_graph_.interf_graph->NewNode(use_temp);
        temp_node_map_->Enter(use_temp, new_node);
      }
    }
    for(auto def_temp: def) {
      if(temp_node_map_->Look(def_temp) == nullptr) {
        auto new_node = live_graph_.interf_graph->NewNode(def_temp);
        temp_node_map_->Enter(def_temp, new_node);
      }
    }
  }

  // 添加相干图边
  for(const auto &instr_node: instr_nodes) {
    auto out = out_->Look(instr_node)->GetList();
    // out集合的互相干涉
    for(auto iter = out.begin(); iter != out.end(); ++ iter) {
      for(auto iter2 = std::next(iter); iter2 != out.end(); ++ iter2) {
        // 排除move指令的干涉情况
        if(auto move_instr = dynamic_cast<assem::MoveInstr *>(instr_node->NodeInfo())) {
          if(move_instr->dst_->Contain(*iter) && move_instr->src_->Contain(*iter2)) {
            continue;
          }
          if(move_instr->dst_->Contain(*iter2) && move_instr->src_->Contain(*iter)) {
            continue;
          }
        }
        
        auto u = temp_node_map_->Look(*iter);
        auto v = temp_node_map_->Look(*iter2);
        if(u->GoesTo(v) || v->GoesTo(u)) {
          continue;
        }

        live_graph_.interf_graph->AddEdge(u, v);
      }
    }

    // def和out集合的干涉
    // NOTE: 对于call指令是必要的，因为call指令会修改所有callee-save寄存器
    if(auto oper_instr = dynamic_cast<assem::OperInstr *>(instr_node->NodeInfo())) {
      auto def = instr_node->NodeInfo()->Def() ? instr_node->NodeInfo()->Def()->GetList() : std::list<temp::Temp *>();
      for(auto &def_temp: def) {
        for(auto &out_temp: out) {
          auto u = temp_node_map_->Look(def_temp);
          auto v = temp_node_map_->Look(out_temp);
          if(u->GoesTo(v) || v->GoesTo(u)) {
            continue;
          }

          live_graph_.interf_graph->AddEdge(u, v);
        }
      }
    }
  }

  // 添加移动指令列表（虚线）
  for(const auto &instr_node: instr_nodes) {
    if(auto move_instr = dynamic_cast<assem::MoveInstr *>(instr_node->NodeInfo())) {
      auto dst_node = temp_node_map_->Look(move_instr->dst_->GetList().front());
      auto src_node = temp_node_map_->Look(move_instr->src_->GetList().front());
      auto new_moves = new MoveList();
      new_moves->Append(src_node, dst_node);
      live_graph_.moves = live_graph_.moves->Union(new_moves);
    }
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live