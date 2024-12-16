#include "tiger/semant/semant.h"
#include "tiger/absyn/absyn.h"

#include <queue>

namespace absyn {

/**
 * Directed graph for cycle detection.
 */
class DirectGraph {
public:
  explicit DirectGraph(std::size_t n) : adjacency_list_(n) {}
  ~DirectGraph() = default;

  void add_node(std::size_t from, std::size_t to) {
    adjacency_list_[from].push_back(to);
  }

  /**
   * Check if the graph has a cycle.
   */
  bool has_circuit() const {
    const std::size_t num_nodes = adjacency_list_.size();
    std::vector<int> in_degree(num_nodes, 0);

    // Compute in-degrees for each node.
    for (const auto &neighbors : adjacency_list_) {
      for (std::size_t neighbor : neighbors) {
        in_degree[neighbor]++;
      }
    }

    // Collect nodes with zero in-degree.
    std::queue<std::size_t> zero_in_degree_queue;
    for (std::size_t i = 0; i < num_nodes; ++i) {
      if (in_degree[i] == 0) {
        zero_in_degree_queue.push(i);
      }
    }

    std::size_t visited_count = 0;
    // Kahn's Algorithm for topological sorting.
    while (!zero_in_degree_queue.empty()) {
      std::size_t node = zero_in_degree_queue.front();
      zero_in_degree_queue.pop();
      ++visited_count;

      for (std::size_t neighbor : adjacency_list_[node]) {
        if (--in_degree[neighbor] == 0) {
          zero_in_degree_queue.push(neighbor);
        }
      }
    }

    // If we visited all nodes, there’s no cycle; otherwise, there is one.
    return visited_count != num_nodes;
  }

private:
  std::vector<std::vector<std::size_t>> adjacency_list_;
};

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    return (static_cast<env::VarEntry *>(entry))->ty_->ActualTy();
  } else {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
  }
  return type::IntTy::Instance();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  auto var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!var_ty || typeid(*var_ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }
  auto record_field_list =
      (static_cast<type::RecordTy *>(var_ty))->fields_->GetList();
  for (auto field = record_field_list.begin(); field != record_field_list.end();
       field++) {
    if ((*field)->name_->Name() == sym_->Name()) {
      return (*field)->ty_->ActualTy();
    }
  }

  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  return type::IntTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  auto var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!var_ty || typeid(*var_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return type::IntTy::Instance();
  }

  auto subscript_ty = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!subscript_ty || typeid(*subscript_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "subscript must be an integer");
    return type::IntTy::Instance();
  }

  return (static_cast<type::ArrayTy *>(var_ty))->ty_->ActualTy();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  auto func_entry = venv->Look(func_);
  if (!func_entry || typeid(*func_entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return type::IntTy::Instance();
  }

  // Check formals
  auto arg_list = args_->GetList();
  auto formal_list =
      (static_cast<env::FunEntry *>(func_entry))->formals_->GetList();
  auto arg_it = arg_list.begin();
  auto formal_it = formal_list.begin();

  while (arg_it != arg_list.end() && formal_it != formal_list.end()) {
    auto arg_ty = (*arg_it)->SemAnalyze(venv, tenv, labelcount, errormsg);
    auto formal_ty = (*formal_it)->ActualTy();
    if (!arg_ty->IsSameType(formal_ty)) {
      errormsg->Error(pos_, "para type mismatch");
      return type::IntTy::Instance();
    }
    arg_it++;
    formal_it++;
    if (arg_it != arg_list.end() && formal_it == formal_list.end()) {
      errormsg->Error(pos_, "too many params in function %s",
                      func_->Name().data());
      return type::IntTy::Instance();
    }
    if (arg_it == arg_list.end() && formal_it != formal_list.end()) {
      errormsg->Error(pos_, "too few params in function %s",
                      func_->Name().data());
      return type::IntTy::Instance();
    }
  }

  return (static_cast<env::FunEntry *>(func_entry))->result_->ActualTy();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *left_ty =
      left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *right_ty =
      right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (oper_ == absyn::PLUS_OP || oper_ == absyn::MINUS_OP ||
      oper_ == absyn::TIMES_OP || oper_ == absyn::DIVIDE_OP) {
    if (typeid(*left_ty) != typeid(type::IntTy)) {
      errormsg->Error(left_->pos_, "integer required");
    }
    if (typeid(*right_ty) != typeid(type::IntTy)) {
      errormsg->Error(right_->pos_, "integer required");
    }
    return type::IntTy::Instance();
  } else {
    if (!left_ty->IsSameType(right_ty)) {
      errormsg->Error(pos_, "same type required");
      return type::IntTy::Instance();
    }
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  auto entry = tenv->Look(typ_);
  if (!entry) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }

  auto record_ty = entry->ActualTy();
  if (!record_ty || typeid(*record_ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }

  return record_ty;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  auto exp_list = seq_->GetList();
  type::Ty *result = type::VoidTy::Instance();
  for (auto exp = exp_list.begin(); exp != exp_list.end(); exp++) {
    result = (*exp)->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  return result;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  if (typeid(*var_) == typeid(absyn::SimpleVar)) {
    auto entry = venv->Look(static_cast<absyn::SimpleVar *>(var_)->sym_);
    if (entry && entry->readonly_) {
      errormsg->Error(pos_, "loop variable can't be assigned");
    }
  }

  auto var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto exp_ty = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!var_ty->IsSameType(exp_ty)) {
    errormsg->Error(pos_, "unmatched assign exp");
  }

  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  auto test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "integer required");
  }

  auto then_ty = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (elsee_ == nullptr) {
    if (typeid(*then_ty) != typeid(type::VoidTy)) {
      errormsg->Error(then_->pos_, "if-then exp's body must produce no value");
    }
    return type::VoidTy::Instance();
  }

  auto elsee_ty = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!then_ty->IsSameType(elsee_ty)) {
    errormsg->Error(pos_, "then exp and else exp type mismatch");
    return type::IntTy::Instance();
  }

  return then_ty;
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  auto test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "integer required");
  }

  auto body_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
  if (typeid(*body_ty) != typeid(type::VoidTy)) {
    errormsg->Error(pos_, "while body must produce no value");
    return type::VoidTy::Instance();
  }

  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  auto lo_ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto hi_ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*lo_ty) != typeid(type::IntTy)) {
    errormsg->Error(lo_->pos_, "for exp's range type is not integer");
  }
  if (typeid(*hi_ty) != typeid(type::IntTy)) {
    errormsg->Error(hi_->pos_, "for exp's range type is not integer");
  }

  venv->BeginScope();
  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));
  auto body_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
  venv->EndScope();

  if (typeid(*body_ty) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "for body must produce no value");
    return type::IntTy::Instance();
  }

  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  if (labelcount == 0) {
    errormsg->Error(pos_, "break statement not within loop");
  }

  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();
  for (Dec *dec : decs_->GetList())
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);

  type::Ty *result;
  if (!body_)
    result = type::VoidTy::Instance();
  else
    result = body_->SemAnalyze(venv, tenv, labelcount, errormsg);

  tenv->EndScope();
  venv->EndScope();
  return result;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  auto entry = tenv->Look(typ_);
  if (!entry) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }

  if (typeid(*entry->ActualTy()) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "not an array type");
    return type::IntTy::Instance();
  }

  auto array_ty = static_cast<type::ArrayTy *>(entry->ActualTy());
  auto array_size_ty = size_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (typeid(*array_size_ty) != typeid(type::IntTy)) {
    errormsg->Error(size_->pos_, "array size must be an integer");
    return type::VoidTy::Instance();
  }
  auto array_init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!array_init_ty->IsSameType(array_ty->ty_)) {
    errormsg->Error(init_->pos_, "type mismatch");
    return type::VoidTy::Instance();
  }

  return array_ty;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  auto fun_list = this->functions_->GetList();
  std::unordered_map<std::string_view, env::FunEntry *> fun_map{};

  for (auto fun : fun_list) {
    if (fun_map.find(fun->name_->Name()) != fun_map.end()) {
      errormsg->Error(this->pos_, "two functions have the same name");
      return;
    }
    auto formals = fun->params_->MakeFormalTyList(tenv, errormsg);
    auto result =
        fun->result_ ? tenv->Look(fun->result_) : type::VoidTy::Instance();
    if (!result) {
      errormsg->Error(this->pos_, "undefined type %s",
                      fun->result_->Name().c_str());
    }
    auto fun_entry = new env::FunEntry{formals, result};
    venv->Enter(fun->name_, fun_entry);
    fun_map.insert({fun->name_->Name(), fun_entry});
  }

  for (auto &func : fun_list) {
    auto found = fun_map.find(func->name_->Name());
    assert(found != fun_map.end());
    auto func_entry = found->second;
    venv->BeginScope();

    auto params = func->params_->GetList();
    auto expected_params = func_entry->formals_->GetList();
    assert(params.size() == expected_params.size());

    auto param_iter = params.begin();
    auto formal_iter = expected_params.begin();

    while (param_iter != params.end()) {
      venv->Enter((*param_iter)->name_,
                  new env::VarEntry((*formal_iter)->ActualTy()));
      ++param_iter;
      ++formal_iter;
    }
    assert(formal_iter == expected_params.end());

    auto return_type =
        func->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!return_type->IsSameType(func_entry->result_)) {
      if (typeid(*func_entry->result_) == typeid(type::VoidTy)) {
        errormsg->Error(this->pos_, "procedure returns value");
      }
    }
    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  auto ty_entry = typ_ ? tenv->Look(this->typ_) : nullptr;
  if (typ_ && !ty_entry) {
    errormsg->Error(this->pos_, "undefined type %s", typ_->Name().data());
  }
  auto init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typ_ && !ty_entry->IsSameType(init_ty)) {
    errormsg->Error(pos_, "type mismatch");
  }
  if (!typ_ && typeid(*init_ty) == typeid(type::NilTy)) {
    errormsg->Error(pos_, "init should not be nil without type specified");
  }
  venv->Enter(var_, new env::VarEntry(init_ty));
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  auto original_type_list = this->types_->GetList();
  std::vector<type::NameTy *> processed_type_list;
  DirectGraph dependency_graph(original_type_list.size());
  processed_type_list.reserve(original_type_list.size());

  // First pass: Check for duplicate type names and initialize NameTy objects.
  for (const auto &type : original_type_list) {
    bool has_duplicate =
        std::any_of(processed_type_list.begin(), processed_type_list.end(),
                    [&type](type::NameTy *existing_type) {
                      return existing_type->sym_->Name() == type->name_->Name();
                    });
    if (has_duplicate) {
      errormsg->Error(this->pos_, "two types have the same name");
      return;
    }

    auto *name_ty_instance = new type::NameTy(type->name_, nullptr);
    tenv->Enter(type->name_, name_ty_instance);
    processed_type_list.push_back(name_ty_instance);
  }

  // Second pass: Resolve types and build dependency graph for cycle detection.
  auto original_iter = original_type_list.begin();
  auto processed_iter = processed_type_list.begin();
  while (original_iter != original_type_list.end()) {
    auto resolved_type = (*original_iter)->ty_->SemAnalyze(tenv, errormsg);
    (*processed_iter)->ty_ = resolved_type;

    // If the resolved type is another NameTy, create a dependency in the graph.
    if (typeid(*resolved_type) == typeid(type::NameTy)) {
      auto matching_iter = std::find_if(
          processed_type_list.begin(), processed_type_list.end(),
          [resolved_type](type::NameTy *existing_type) {
            return existing_type->sym_->Name() ==
                   static_cast<type::NameTy *>(resolved_type)->sym_->Name();
          });

      if (matching_iter != processed_type_list.end()) {
        int start_node = processed_iter - processed_type_list.begin();
        int end_node = matching_iter - processed_type_list.begin();
        dependency_graph.add_node(start_node, end_node);
      }
    }

    ++original_iter;
    ++processed_iter;
  }

  if (dependency_graph.has_circuit()) {
    errormsg->Error(this->pos_, "illegal type cycle");
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  auto entry = tenv->Look(name_);
  if (!entry) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().data());
    return type::VoidTy::Instance();
  }

  return entry;
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  auto field_list = record_->GetList();
  auto field_ty_list = new type::FieldList();
  for (auto field = field_list.begin(); field != field_list.end(); field++) {
    auto entry = tenv->Look((*field)->typ_);
    if (!entry) {
      errormsg->Error(pos_, "undefined type %s", (*field)->typ_->Name().data());
      return type::VoidTy::Instance();
    }
    field_ty_list->Append(new type::Field((*field)->name_, entry));
  }
  return new type::RecordTy(field_ty_list);
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  auto entry = tenv->Look(array_);
  if (!entry) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().data());
    return type::VoidTy::Instance();
  }

  return new type::ArrayTy(entry);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}
} // namespace sem
