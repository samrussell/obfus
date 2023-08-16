#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include <llvm/Transforms/Utils/Local.h>
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

    static void demotePhiNodes(Function& F) {
        std::vector<PHINode*> phiNodes;
        do {
        phiNodes.clear();
        for (auto& BB : F) {
            for (auto& I : BB.phis()) {
            phiNodes.push_back(&I);
            }
        }
        //count += phiNodes.size();
        for (PHINode* phi : phiNodes) {
            DemotePHIToStack(phi, F.begin()->getTerminator());
        }
        } while (!phiNodes.empty());
    }

    struct MyPass : public PassInfoMixin<MyPass> {
        PreservedAnalyses run(Module &M,
                                             ModuleAnalysisManager &MAM) {
                                                
            for (Function& F : M) {
                outs() << F.getName() << "\n";
                outs() << F.getInstructionCount() << " instructions\n";
                if(F.getInstructionCount() < 10) {
                    // skip this
                    outs() << "Skipping\n";
                    continue;
                }
                // iterate over basic blocks
                for (BasicBlock& BB : F) {
                    outs() << "New basic block " << getSimpleNodeLabel(&BB) << "\n";
                    for(Instruction& instruction : BB) {
                        outs() << " Instruction: " << instruction << "\n";
                    }
                }
                // bool fChanged = runOnFunction(F, *RNG);

                // if (fChanged) {
                //     reg2mem(F);
                // }

                // Changed |= fChanged;
                // SR insert old code
                // let's try rerouting some blocks?

                BasicBlock &EntryBlock = F.getEntryBlock();

                // thanks Romain!
                SmallVector<BasicBlock*, 20> flattenedBB;
                // demote PHI nodes
                //size_t count = 0;
                demotePhiNodes(F);

                for (BasicBlock& BB : F) {
                // if (&BB == &EntryBlock) {
                //   outs() << "Not flattening entry block " << getSimpleNodeLabel(&BB) << "\n";
                //   continue;
                // }
                outs() << "Adding block to flatten: " << getSimpleNodeLabel(&BB) << "\n";
                flattenedBB.push_back(&BB);
                }

                // add our new var

                IRBuilder<> EntryBuilder(&EntryBlock, EntryBlock.begin());
                Type *Int32Ty = IntegerType::get(F.getContext(), 32);
                AllocaInst *Counter = EntryBuilder.CreateAlloca(Int32Ty, nullptr, "counter");
                EntryBuilder.CreateStore(ConstantInt::get(Int32Ty, 0), Counter);

                // we'll add our dispatch block after the entry block
                //BasicBlock* DispatchBlock = 

                for (BasicBlock* pToflatBB : flattenedBB){
                // only handle branches for now
                outs() << "Flattening block " << getSimpleNodeLabel(pToflatBB) << "\n";
                if (auto* br = dyn_cast<BranchInst>(pToflatBB->getTerminator())) {
                    // we'll just print them out
                    for (unsigned i = 0; i < br->getNumSuccessors(); ++i) {
                    BasicBlock *Successor = br->getSuccessor(i);
                    outs() << "Successor: " << getSimpleNodeLabel(Successor) << "\n";
                    // ok here we can add a block in the middle
                    BasicBlock *NewBlock = BasicBlock::Create(F.getContext(), "", &F);

                    // Increment the counter in the new block
                    IRBuilder<> Builder(NewBlock);
                    LoadInst *LoadedCounter = Builder.CreateLoad(Int32Ty, Counter);
                    Value *IncrementedCounter = Builder.CreateAdd(LoadedCounter, ConstantInt::get(Int32Ty, 1));
                    Builder.CreateStore(IncrementedCounter, Counter);
                    Builder.CreateBr(Successor);

                    // Modify the existing terminator to point to the new block
                    br->setSuccessor(i, NewBlock);
                    
                    // move after our block
                    NewBlock->moveAfter(pToflatBB);
                    }
                }
                }

                // print again to see what changed

                outs() << "After flattening:\n";
                
                for (BasicBlock& BB : F) {
                outs() << "New basic block " << getSimpleNodeLabel(&BB) << "\n";
                for(Instruction& instruction : BB) {
                    outs() << " Instruction: " << instruction << "\n";
                }
                }

                // OMVLL does a reg2mem() call here
                demotePhiNodes(F);


                // SR end old code
            }
            //return PreservedAnalyses::all();
            return PreservedAnalyses::none();
        }
    };
}

PassPluginLibraryInfo getPassPluginInfo() {
  static std::atomic<bool> ONCE_FLAG(false);
  return {LLVM_PLUGIN_API_VERSION, "OMVLL", "0.0.1",
          [](PassBuilder &PB) {

            try {
              PB.registerPipelineEarlySimplificationEPCallback(
                [&] (ModulePassManager &MPM, OptimizationLevel opt) {
                  if (ONCE_FLAG) {
                    return true;
                  }
                MPM.addPass(obfs::MyPass());
                  ONCE_FLAG = true;
                  return true;
                }
              );
            } catch (const std::exception& e) {
                outs() << "Error: " << e.what() << "\n";
            }
          }};
};

extern "C" __attribute__((visibility("default"))) LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
