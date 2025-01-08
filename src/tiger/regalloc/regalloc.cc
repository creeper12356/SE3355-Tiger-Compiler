#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"
#include <iostream>

extern frame::RegManager *reg_manager;
extern std::map<std::string, std::pair<int, int>> frame_info_map;

namespace ra {
/* TODO: Put your lab6 code here */
void RegAllocator::RegAlloc() {
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

    live::LiveGraphFactory live_graph_factory(flowgraph);
    live_graph_factory.Liveness();
    auto live_graph = live_graph_factory.GetLiveGraph();
    
}

std::unique_ptr<ra::Result> RegAllocator::TransferResult() {
    return std::move(result_);
}

} // namespace ra