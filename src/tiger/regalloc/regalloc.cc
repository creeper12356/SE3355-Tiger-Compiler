#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"
#include <iostream>
#include <set>

extern frame::RegManager *reg_manager;
extern std::map<std::string, std::pair<int, int>> frame_info_map;

namespace ra {
void RegAllocator::RegAlloc() {
    RegAllocMain();
    auto coloring = temp::Map::Empty();
    for(auto color: color_map_) {
        coloring->Enter(color.first->NodeInfo(), reg_manager->temp_map_->Look(color.second));
    }
    
    result_ = std::make_unique<Result>(coloring, assem_instr_->GetInstrList());
}

void RegAllocator::RegAllocMain() {
    // 构造控制流图
    auto flowgraph = BuildCFG();

    // 根据控制流图，构造活跃相干图
    live::LiveGraphFactory live_graph_factory(flowgraph);
    live_graph_factory.Liveness();
    live_graph_ = live_graph_factory.GetLiveGraph();
    auto live_graph_nodes = live_graph_.interf_graph->Nodes()->GetList();
    
    // 初始化相干图中precolored_和initial_
    for(auto node: live_graph_nodes) {
        if(reg_manager->Registers()->Contain(node->NodeInfo())) {
            precolored_.Append(node);
        } else {
            initial_.Append(node);
        }    
    }

    worklist_moves_ = live_graph_.moves;
    coalesced_moves_ = new live::MoveList();
    constrained_moves_ = new live::MoveList();
    frozen_moves_ = new live::MoveList();
    active_moves_ = new live::MoveList();


    // 初始化adj_set_
    for(auto node: live_graph_nodes) {
        auto adj_nodes = node->Adj()->GetList();
        for(auto adj_node: adj_nodes) {
            // 保证双向都在
            adj_set_.insert({node, adj_node});
            adj_set_.insert({adj_node, node});
        }
    }

    // 初始化degree_map_和move_list_map_和alias_map_和color_map_
    for(auto node: live_graph_nodes) {
        degree_map_.insert({node, node->Degree()});
        move_list_map_.insert({node, new live::MoveList()});
        alias_map_.insert({node, node});
        if(precolored_.Contain(node)) {
            // precolored_中的节点已经着色
            color_map_.insert({node, node->NodeInfo()});
        } else {
            color_map_.insert({node, nullptr});
        }
    }
    auto worklist_moves_list = worklist_moves_->GetList();
    for(auto move: worklist_moves_list) {
        move_list_map_[move.first]->Append(move.first, move.second);
        move_list_map_[move.second]->Append(move.first, move.second);
    }

    MakeWorklist();
    do {
        if(simplify_worklist_.GetList().size() > 0) {
            Simplify();
        } else if(worklist_moves_->GetList().size() > 0) {
            Coalesce();
        } else if(freeze_worklist_.GetList().size() > 0) {
            Freeze();
        } else if(spill_worklist_.GetList().size() > 0) {
            SelectSpill();
        }
    } while(
        simplify_worklist_.GetList().size() > 0 ||
        worklist_moves_->GetList().size() > 0 ||
        freeze_worklist_.GetList().size() > 0 ||
        spill_worklist_.GetList().size() > 0
    );
    
    AssignColors();
    if(spilled_nodes_.GetList().size() > 0) {
        RewriteProgram();
        // 尾递归
        RegAlloc();
    }
}

std::unique_ptr<ra::Result> RegAllocator::TransferResult() {
    return std::move(result_);
}

void RegAllocator::AddEdge(live::INodePtr u, live::INodePtr v) {
    if(adj_set_.find({u, v}) == adj_set_.end() && u != v) {
        adj_set_.insert({u, v});
        adj_set_.insert({v, u});

        degree_map_[u] += 1;
        degree_map_[v] += 1;
        // TODO: 判断u, v的precolored ? 
        live_graph_.interf_graph->AddEdge(u, v);
    }
}

fg::FGraphPtr RegAllocator::BuildCFG() {
    // 构造控制流图
    fg::FGraphPtr flowgraph = new fg::FGraph();
    std::map<assem::Instr *, fg::FNodePtr> instr_node_map;
    std::map<temp::Label *, fg::FNodePtr> label_node_map;
    auto instr_list = assem_instr_->GetInstrList()->GetList();
    for(const auto &instr: instr_list) {
        auto node = flowgraph->NewNode(instr);
        instr_node_map.insert({instr, node});
        if(auto label_instr = dynamic_cast<assem::LabelInstr *>(instr)) {
            label_node_map.insert({label_instr->label_, node});
        }
    }

    // 正常执行流
    for(auto iter = instr_list.begin(); iter != instr_list.end(); ++iter) {
        auto instr = *iter;
        if(auto oper_instr = dynamic_cast<assem::OperInstr *>(instr)) {
            // oper_instr->assem_以jmp开头，跳过
            if(oper_instr->assem_.find("jmp") == 0) {
                continue;
            }
        }
        
        if(std::next(iter) == instr_list.end()) {
            break;
        }

        flowgraph->AddEdge(instr_node_map[instr], instr_node_map[*std::next(iter)]);
    }

    // 跳转执行流
    for(auto iter = instr_list.begin(); iter != instr_list.end(); ++iter) {
        auto instr = *iter;
        if(auto oper_instr = dynamic_cast<assem::OperInstr *>(instr)) {
            auto targets = oper_instr->jumps_;
            if(targets) {
                for(auto label: *targets->labels_) {
                    flowgraph->AddEdge(instr_node_map[instr], label_node_map[label]);
                }
            }
        }
    }

    return flowgraph;
}

void RegAllocator::MakeWorklist() {
    auto initial_nodes = initial_.GetList();
    // 算法中的K
    int phy_reg_cnt = reg_manager->Registers()->GetList().size();

    for(auto node: initial_nodes) {
        if(degree_map_[node] >= phy_reg_cnt) {
            spill_worklist_.Append(node);
        } else if(MoveRelated(node)) {
            freeze_worklist_.Append(node);
        } else {
            simplify_worklist_.Append(node);
        }
    }
    initial_nodes.clear();
}

void RegAllocator::DecrementDegree(live::INodePtr node) {
    auto degree = degree_map_[node];
    degree_map_[node] = degree - 1;
    if(degree == reg_manager->Registers()->GetList().size()) {
        auto neighbor_nodes = Adjacent(node);
        neighbor_nodes->Append(node);
        EnableMoves(neighbor_nodes);
        spill_worklist_.DeleteNode(node);
        if(MoveRelated(node)) {
            freeze_worklist_.Append(node);
        } else {
            simplify_worklist_.Append(node);
        }
    }
}

void RegAllocator::EnableMoves(live::INodeListPtr nodes) {
    auto nodes_list = nodes->GetList();
    for(auto node: nodes_list) {
        auto node_moves = NodeMoves(node);
        auto node_moves_list = node_moves->GetList();
        for(auto move: node_moves_list) {
            if(active_moves_->Contain(move.first, move.second)) {
                active_moves_->Delete(move.first, move.second);
                worklist_moves_->Append(move.first, move.second);
            }
        }
    }
}

void RegAllocator::Simplify() {
    auto node = simplify_worklist_.GetList().back();
    simplify_worklist_.DeleteNode(node);
    select_stack_.Append(node);

    auto neighbors = Adjacent(node)->GetList();
    for(auto neighbor: neighbors) {
        DecrementDegree(neighbor);
    }
}

void RegAllocator::Coalesce() {
    auto move = worklist_moves_->GetList().back();
    auto u = GetAlias(move.first);
    auto v = GetAlias(move.second);
    if(precolored_.Contain(v)) {
        u = v;
        v = alias_map_[move.first];
    }    
    worklist_moves_->Delete(move.first, move.second);
    if(u == v) {
        coalesced_moves_->Append(move.first, move.second);
        AddWorkList(u);
    } else if(precolored_.Contain(v) || adj_set_.find({u, v}) != adj_set_.end()) {
        constrained_moves_->Append(move.first, move.second);
        AddWorkList(u);
        AddWorkList(v);
    } else if(precolored_.Contain(u)) {
        bool all_ok = true;
        auto v_neighbors = Adjacent(v)->GetList();
        for(auto neighbor: v_neighbors) {
            if(!OK(neighbor, u)) {
                all_ok = false;
                break;
            }
        }

        if(all_ok) {
            coalesced_moves_->Append(move.first, move.second);
            Combine(u, v);
            AddWorkList(u);
        }
    } else if(!precolored_.Contain(v) && Conservative(Adjacent(u)->Union(Adjacent(v)))) {
        // Same
        coalesced_moves_->Append(move.first, move.second);
        Combine(u, v);
        AddWorkList(u);
    } else {
        active_moves_->Append(move.first, move.second);
    }
}

bool RegAllocator::OK(live::INodePtr t, live::INodePtr r) {
    int phy_reg_cnt = reg_manager->Registers()->GetList().size();
    return degree_map_[t] < phy_reg_cnt ||
        precolored_.Contain(t) || 
        adj_set_.find({t, r}) != adj_set_.end();
}

bool RegAllocator::Conservative(live::INodeListPtr nodes) {
    int cnt = 0;
    int phy_reg_cnt = reg_manager->Registers()->GetList().size();
    auto nodes_list = nodes->GetList();
    for(auto node: nodes_list) {
        if(degree_map_[node] >= phy_reg_cnt) {
            ++ cnt;
        }
    }

    return cnt < phy_reg_cnt;
}

void RegAllocator::AddWorkList(live::INodePtr node) {
    int phy_reg_cnt = reg_manager->Registers()->GetList().size();
    if(!precolored_.Contain(node)
        && !MoveRelated(node)
        && degree_map_[node] < phy_reg_cnt) {
        freeze_worklist_.DeleteNode(node);
        simplify_worklist_.Append(node);
    }
}

live::INodePtr RegAllocator::GetAlias(live::INodePtr node) {
    if(coalesced_nodes_.Contain(node)) {
        return GetAlias(alias_map_[node]);
    } else {
        return node;
    }
}

void RegAllocator::Combine(live::INodePtr u, live::INodePtr v) {
    if(freeze_worklist_.Contain(v)) {
        freeze_worklist_.DeleteNode(v);
    } else {
        spill_worklist_.DeleteNode(v);
    }
    coalesced_nodes_.Append(v);
    alias_map_[v] = u;
    move_list_map_[u] = move_list_map_[u]->Union(move_list_map_[v]);
    auto v_inode_list = live::INodeList();
    v_inode_list.Append(v);
    EnableMoves(&v_inode_list);

    auto v_neighbors = Adjacent(v)->GetList();
    for(auto neighbor: v_neighbors) {
        AddEdge(neighbor, u);
        DecrementDegree(neighbor);
    }
    
    int phy_reg_cnt = reg_manager->Registers()->GetList().size();
    if(degree_map_[u] >= phy_reg_cnt && freeze_worklist_.Contain(u)) {
        freeze_worklist_.DeleteNode(u);
        spill_worklist_.Append(u);
    }
}

void RegAllocator::Freeze() {
    auto node = freeze_worklist_.GetList().back();
    freeze_worklist_.DeleteNode(node);
    simplify_worklist_.Append(node);
    FreezeMoves(node);
}

void RegAllocator::FreezeMoves(live::INodePtr node) {
    auto node_moves_list = NodeMoves(node)->GetList();
    for(auto move: node_moves_list) {
        // move: (x, y)
        live::INodePtr v;
        if(GetAlias(move.second) == GetAlias(node)) {
            v = GetAlias(move.first);
        } else {
            v = GetAlias(move.second);
        }

        active_moves_->Delete(move.first, move.second);
        frozen_moves_->Append(move.first, move.second);
        int phy_reg_cnt = reg_manager->Registers()->GetList().size();
        if(NodeMoves(v)->GetList().size() == 0 && degree_map_[v] < phy_reg_cnt) {
            freeze_worklist_.DeleteNode(v);
            simplify_worklist_.Append(v);
        }
    }
}

void RegAllocator::SelectSpill() {
    // TODO: 启发式选择一个溢出节点
    auto node = spill_worklist_.GetList().back();
    spill_worklist_.DeleteNode(node);
    simplify_worklist_.Append(node);
    FreezeMoves(node);
}

void RegAllocator::AssignColors() {
    auto select_stack = this->select_stack_.GetList();
    while(!select_stack.empty()) {
        auto node = select_stack.back();
        select_stack.pop_back();
        auto ok_colors = reg_manager->Registers();

        auto adj_nodes = node->Adj()->GetList();
        for(auto adj_node: adj_nodes) {
            if(colored_nodes_.Union(&precolored_)->Contain(GetAlias(adj_node))) {
                ok_colors->Delete(color_map_[GetAlias(adj_node)]);
            }
        }

        if(ok_colors->GetList().empty()) {
            spilled_nodes_.Append(node);
        } else {
            colored_nodes_.Append(node);
            auto color = ok_colors->GetList().front();
            color_map_[node] = color;
        }
    }

    auto coalesced_nodes = coalesced_nodes_.GetList();
    for(auto node: coalesced_nodes) {
        color_map_[node] = color_map_[GetAlias(node)];
    }

    select_stack_.Clear();
}

void RegAllocator::RewriteProgram() {
    auto instr_list = assem_instr_->GetInstrList();
    auto spilled_nodes = spilled_nodes_.GetList();
    for(auto spilled_node: spilled_nodes) {
        for(auto iter = instr_list->GetList().begin(); iter != instr_list->GetList().end(); ++iter) {
            auto instr = *iter;
            auto use = instr->Use();
            auto def = instr->Def();
            if(use->Contain(spilled_node->NodeInfo())) {
                auto new_temp = temp::TempFactory::NewTemp();
                instr_list->Insert(iter, new assem::OperInstr(
                    // TODO: 修改为正确的偏移
                    "movq `100(`s0),`d0",
                    new temp::TempList(new_temp),
                    new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RSP)),
                    nullptr
                ));
                if(auto oper_instr = dynamic_cast<assem::OperInstr *>(instr)) {
                    oper_instr->src_->Replace(spilled_node->NodeInfo(), new_temp);
                } else if(auto move_instr = dynamic_cast<assem::MoveInstr *>(instr)) {
                    move_instr->src_->Replace(spilled_node->NodeInfo(), new_temp);
                } else {
                    assert(false);
                }
            }

            if(def->Contain(spilled_node->NodeInfo())) {
                auto new_temp = temp::TempFactory::NewTemp();
                instr_list->Insert(std::next(iter), new assem::OperInstr(
                    "movq `s0,`100(`s1)",
                    nullptr,
                    new temp::TempList({new_temp, reg_manager->GetRegister(frame::X64RegManager::Reg::RSP)}),
                    nullptr
                ));
                if(auto oper_instr = dynamic_cast<assem::OperInstr *>(instr)) {
                    oper_instr->dst_->Replace(spilled_node->NodeInfo(), new_temp);
                } else if(auto move_instr = dynamic_cast<assem::MoveInstr *>(instr)) {
                    move_instr->dst_->Replace(spilled_node->NodeInfo(), new_temp);
                } else {
                    assert(false);
                }
            }
        }
    }

    spilled_nodes_.Clear();
    // initial_ = colored_nodes_.Union(&coalesced_nodes_)->Union(new_temps);
    colored_nodes_.Clear();
    coalesced_nodes_.Clear();
    
}

live::INodeListPtr RegAllocator::Adjacent(live::INodePtr node) {
    return node->Adj()->Diff(select_stack_.Union(&coalesced_nodes_));
}

live::MoveList *RegAllocator::NodeMoves(live::INodePtr node) {
    return move_list_map_[node]->Intersect(active_moves_->Union(worklist_moves_));
}

bool RegAllocator::MoveRelated(live::INodePtr node) {
    return NodeMoves(node)->GetList().size() > 0;
}
} // namespace ra