#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
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

                // thanks Romain!
                SmallVector<BasicBlock*, 20> flattenedBB;
                // demote PHI nodes
                //size_t count = 0;
                demotePhiNodes(F);

                for (BasicBlock& BB : F) {
                    if (&BB == &(F.getEntryBlock())) {
                        outs() << "Not flattening entry block " << getSimpleNodeLabel(&BB) << "\n";
                        continue;
                    }
                    outs() << "Adding block to flatten: " << getSimpleNodeLabel(&BB) << "\n";
                    flattenedBB.push_back(&BB);
                }

                // add our new var

                IRBuilder<> EntryBuilder(&(F.getEntryBlock()), F.getEntryBlock().begin());
                Type *Int32Ty = IntegerType::get(F.getContext(), 32);
                AllocaInst *DispatchVar = EntryBuilder.CreateAlloca(Int32Ty, nullptr, "dispatch_var");
                EntryBuilder.CreateStore(ConstantInt::get(Int32Ty, 0x1001), DispatchVar);

                // SR phase 2: add dispatch
                // split first block so we can treat it like any other
                BasicBlock &tempEntryBlock = F.getEntryBlock();
                BasicBlock* pNewEntryBlock = tempEntryBlock.splitBasicBlockBefore(tempEntryBlock.getTerminator(), "");
                // old EntryBlock points to the new one (?!)
                // add this to the list to flatten
                flattenedBB.push_back(&tempEntryBlock);

                // we'll add our dispatch block after the entry block
                BasicBlock* DispatchBlock = BasicBlock::Create(F.getContext(), "dispatch_block", &F);

                // get the successor from the new entry block
                auto* br = dyn_cast<BranchInst>(pNewEntryBlock->getTerminator());
                BasicBlock *Successor = br->getSuccessor(0);

                // add a load before this
                IRBuilder<> DispatchBuilder(br);
                LoadInst* loadSwitchVar = DispatchBuilder.CreateLoad(DispatchBuilder.getInt32Ty(), DispatchVar, "dispatch_var");

                IRBuilder<> EntryBuilder2(DispatchBlock, DispatchBlock->begin());
                EntryBuilder2.CreateBr(Successor);

                // Modify the existing terminator to point to the new block
                // we just split this so we can assume it's an unconditional branch
                // should probably assert here?
                br->setSuccessor(0, DispatchBlock);
                
                // move after our block
                DispatchBlock->moveAfter(pNewEntryBlock);


                // SR phase 2 end

                int dispatchVal = 0x1001;

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
                            Builder.CreateStore(ConstantInt::get(Int32Ty, ++dispatchVal), DispatchVar);
                            //Builder.CreateBr(Successor);
                            // jump to the dispatcher
                            Builder.CreateBr(DispatchBlock);

                            // Add a new branch in the dispatcher to jump to the new block
                            Instruction* FirstInst = DispatchBlock->getFirstNonPHI();
                            IRBuilder<> DispatchBuilder(FirstInst);

                            // we could load in the first block but we're handling the PHI wrong
                            // so let's just load it in each block
                            LoadInst* loadSwitchVar = DispatchBuilder.CreateLoad(DispatchBuilder.getInt32Ty(), DispatchVar, "dispatch_var");
                            
                            auto *Cond = DispatchBuilder.CreateICmpEQ(ConstantInt::get(Int32Ty, dispatchVal), loadSwitchVar);
                            SplitBlockAndInsertIfThen(Cond, FirstInst, false, nullptr, (DomTreeUpdater *)nullptr, nullptr, Successor);

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
