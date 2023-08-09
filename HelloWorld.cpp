#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include <iostream>

using namespace llvm;

namespace obfs {
  // https://stackoverflow.com/questions/26281823/llvm-how-to-get-the-label-of-basic-blocks
  static std::string getSimpleNodeLabel(const BasicBlock *Node) {
    if (!Node->getName().empty())
        return Node->getName().str();

    std::string Str;
    raw_string_ostream OS(Str);

    Node->printAsOperand(OS, false);
    return OS.str();
}
    struct MyPass : public PassInfoMixin<MyPass> {
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
            outs() << F.getName() << "\n";
            outs() << F.getInstructionCount() << " instructions\n";
            // iterate over basic blocks
            for (BasicBlock& BB : F) {
              outs() << "New basic block " << getSimpleNodeLabel(&BB) << "\n";
              for(Instruction& instruction : BB) {
                outs() << " Instruction: " << instruction << "\n";
              }
            }
            return PreservedAnalyses::all();
        }
    };
}

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerPipelineEarlySimplificationEPCallback(
        [&](ModulePassManager &MPM, auto) {
          MPM.addPass(createModuleToFunctionPassAdaptor(obfs::MyPass()));
          return true;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "name", "0.0.1", callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
