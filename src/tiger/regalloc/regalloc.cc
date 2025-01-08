#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"
#include <iostream>

extern frame::RegManager *reg_manager;
extern std::map<std::string, std::pair<int, int>> frame_info_map;

namespace ra {
/* TODO: Put your lab6 code here */
void RegAllocator::RegAlloc() {
    // TODO
}

std::unique_ptr<ra::Result> RegAllocator::TransferResult() {
    return std::make_unique<ra::Result>();
}

} // namespace ra