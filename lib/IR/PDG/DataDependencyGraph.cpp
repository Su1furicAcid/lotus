/**
 * @file DataDependencyGraph.cpp
 * @brief Implementation of the data dependency analysis for the PDG
 *
 * This file implements the DataDependencyGraph pass, which analyzes data dependencies
 * between program elements. Data dependencies occur when one instruction defines a value
 * that is used by another instruction (def-use chains).
 *
 * Key features:
 * - Analysis of def-use chains in LLVM IR
 * - Support for different types of data dependencies (direct, memory, etc.)
 * - Function-level data dependency analysis
 * - Integration with the overall PDG framework
 * - Support for memory-based dependencies through load/store analysis
 *
 * The data dependency analysis is a fundamental component of the PDG system,
 * complementing control dependency analysis to provide a complete view of
 * program dependencies.
 */

#include "IR/PDG/DataDependencyGraph.h"

char pdg::DataDependencyGraph::ID = 0;

using namespace llvm;

bool pdg::DataDependencyGraph::runOnModule(Module &M)
{
  ProgramGraph &g = ProgramGraph::getInstance();
  if (!g.isBuild())
  {
    g.build(M);
    // TODO: add comment
    g.bindDITypeToNodes(M);
  }
  
  for (auto &F : M)
  {
    if (F.isDeclaration() || F.empty())
      continue;
    _mem_dep_res = &getAnalysis<MemoryDependenceWrapperPass>(F).getMemDep();
    // setup alias query interface for each function
    for (auto inst_iter = inst_begin(F); inst_iter != inst_end(F); inst_iter++)
    {
      addDefUseEdges(*inst_iter);
      addRAWEdges(*inst_iter);
      addAliasEdges(*inst_iter);
    }
  }
  return false;
}

void pdg::DataDependencyGraph::addAliasEdges(Instruction &inst)
{
  ProgramGraph &g = ProgramGraph::getInstance();
  Function* func = inst.getFunction();
  for (auto inst_iter = inst_begin(func); inst_iter != inst_end(func); inst_iter++)
  {
    if (&inst == &*inst_iter)
      continue;
    auto alias_result = queryAliasUnderApproximate(inst, *inst_iter);
    if (alias_result != AliasResult::NoAlias)
    {
      Node* src = g.getNode(inst);
      Node* dst = g.getNode(*inst_iter);
      if (src == nullptr || dst == nullptr)
        continue;
      src->addNeighbor(*dst, EdgeType::DATA_ALIAS);
    }
  }
}

void pdg::DataDependencyGraph::addDefUseEdges(Instruction &inst)
{
  ProgramGraph &g = ProgramGraph::getInstance();
  for (auto user : inst.users())
  {
    Node *src = g.getNode(inst);
    Node *dst = g.getNode(*user);
    if (src == nullptr || dst == nullptr)
      continue;
    EdgeType edge_type = EdgeType::DATA_DEF_USE;
    if (dst->getNodeType() == GraphNodeType::ANNO_VAR)
      edge_type = EdgeType::ANNO_VAR;
    if (dst->getNodeType() == GraphNodeType::ANNO_GLOBAL)
      edge_type = EdgeType::ANNO_GLOBAL;
    src->addNeighbor(*dst, edge_type);
  }
}

void pdg::DataDependencyGraph::addRAWEdges(Instruction &inst)
{
  if (!isa<LoadInst>(&inst))
    return;

  ProgramGraph &g = ProgramGraph::getInstance();
  auto dep_res = _mem_dep_res->getDependency(&inst);
  auto dep_inst = dep_res.getInst();

  if (!dep_inst)
    return;
  if (!isa<StoreInst>(dep_inst))
    return;

  Node *src = g.getNode(inst);
  Node *dst = g.getNode(*dep_inst);
  if (src == nullptr || dst == nullptr)
    return;
  dst->addNeighbor(*src, EdgeType::DATA_RAW);
}

AliasResult pdg::DataDependencyGraph::queryAliasUnderApproximate(Value &v1, Value &v2)
{
  // only check alias for pointer type variables
  if (!v1.getType()->isPointerTy() || !v2.getType()->isPointerTy())
    return AliasResult::NoAlias;
  // check bit cast
  if (BitCastInst *bci = dyn_cast<BitCastInst>(&v1))
  {
    if (bci->getOperand(0) == &v2)
      return AliasResult::MustAlias;
  }
  // handle load instruction
  if (LoadInst *li = dyn_cast<LoadInst>(&v1))
  {
    auto load_addr = li->getPointerOperand();
    for (auto user : load_addr->users())
    {
      if (StoreInst *si = dyn_cast<StoreInst>(user))
      {
        if (si->getPointerOperand() == load_addr)
        {
          if (si->getValueOperand() == &v2)
            return AliasResult::MustAlias;
        }
      }
      if (LoadInst *alias_load_inst = dyn_cast<LoadInst>(user))
      {
        if (alias_load_inst == &v2)
          return AliasResult::MustAlias;
      }
    }
  }
  // Add return statement to prevent compiler warning
  return AliasResult::NoAlias;
}

  void pdg::DataDependencyGraph::getAnalysisUsage(AnalysisUsage & AU) const
  {
    AU.addRequired<MemoryDependenceWrapperPass>();
    AU.setPreservesAll();
  }

  static RegisterPass<pdg::DataDependencyGraph>
      DDG("ddg", "Data Dependency Graph Construction", false, true);