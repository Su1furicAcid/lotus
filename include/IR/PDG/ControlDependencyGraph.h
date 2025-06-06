#pragma once
#include "IR/PDG/Graph.h"
#include "llvm/Analysis/PostDominators.h"



namespace pdg
{
  class ControlDependencyGraph : public llvm::FunctionPass
  {
  public:
    static char ID;
    ControlDependencyGraph() : llvm::FunctionPass(ID){};
    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    llvm::StringRef getPassName() const override { return "Control Dependency Graph"; }
    bool runOnFunction(llvm::Function &F) override;
    void addControlDepFromNodeToBB(Node &n, llvm::BasicBlock &bb, EdgeType edge_type);
    void addControlDepFromEntryNodeToInsts(llvm::Function &F);
    void addControlDepFromDominatedBlockToDominator(llvm::Function &F);
  private:
    llvm::PostDominatorTree *_PDT;
  };
} // namespace pdg
