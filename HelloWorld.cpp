#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>

using namespace llvm;

namespace obfs {
    struct MyPass : public PassInfoMixin<MyPass> {
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
            outs() << F.getName() << "\n";
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
