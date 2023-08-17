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

            for (PHINode* phi : phiNodes) {
                DemotePHIToStack(phi, F.begin()->getTerminator());
            }
        } while (!phiNodes.empty());
    }

    void printFunction(Function& F) {
        for (BasicBlock& BB : F) {
            outs() << "New basic block " << getSimpleNodeLabel(&BB) << "\n";
            for(Instruction& instruction : BB) {
                outs() << " Instruction: " << instruction << "\n";
            }
        }
    }

    SmallVector<BasicBlock*, 20> getBlocksToFlatten(Function& F) {
        SmallVector<BasicBlock*, 20> flattenedBB;

        for (BasicBlock& BB : F) {
            if (&BB == &(F.getEntryBlock())) {
                outs() << "Not flattening entry block " << getSimpleNodeLabel(&BB) << "\n";
                continue;
            }
            outs() << "Adding block to flatten: " << getSimpleNodeLabel(&BB) << "\n";
            flattenedBB.push_back(&BB);
        }

        return flattenedBB;
    }

    AllocaInst* initDispatchVar(Function& F, int initialValue) {
        BasicBlock &EntryBlock = F.getEntryBlock();
        IRBuilder<> EntryBuilder(&EntryBlock, EntryBlock.begin());
        AllocaInst *DispatchVar = EntryBuilder.CreateAlloca(EntryBuilder.getInt32Ty(), nullptr, "dispatch_var");
        EntryBuilder.CreateStore(ConstantInt::get(EntryBuilder.getInt32Ty(), initialValue), DispatchVar);

        return DispatchVar;
    }

    BasicBlock& splitBranchOffEntryBlock(Function& F) {
        BasicBlock &entryBlockTail = F.getEntryBlock();
        BasicBlock* pNewEntryBlock = entryBlockTail.splitBasicBlockBefore(entryBlockTail.getTerminator(), "");

        return entryBlockTail;
    }

    BasicBlock* insertDispatchBlockAfterEntryBlock(Function& F, AllocaInst *DispatchVar) {
        BasicBlock &EntryBlock = F.getEntryBlock();
        auto* br = dyn_cast<BranchInst>(EntryBlock.getTerminator());
        BasicBlock *Successor = br->getSuccessor(0);

        // EntryBlock -> Successor
        // we create DispatchBlock and plug it in at both ends

        // DispatchBlock -> Successor        
        BasicBlock* DispatchBlock = BasicBlock::Create(F.getContext(), "dispatch_block", &F);
        IRBuilder<> DispatchBuilder(DispatchBlock, DispatchBlock->begin());
        DispatchBuilder.CreateBr(Successor);

        // EntryBlock -> DispatchBlock
        br->setSuccessor(0, DispatchBlock);
        DispatchBlock->moveAfter(&EntryBlock);

        return DispatchBlock;
    }

    void flattenBlock(BasicBlock* block, int& dispatchVal, Function& F, AllocaInst *DispatchVar, BasicBlock* DispatchBlock) {
        // only handle branches for now
        outs() << "Flattening block " << getSimpleNodeLabel(block) << "\n";
        if (auto* br = dyn_cast<BranchInst>(block->getTerminator())) {
            for (unsigned i = 0; i < br->getNumSuccessors(); ++i) {
                // we start with block -> successor
                BasicBlock *Successor = br->getSuccessor(i);

                // create detour block
                // DispatchVar = X
                // jmp DispatchBlock
                BasicBlock *DetourBlock = BasicBlock::Create(F.getContext(), "", &F);
                IRBuilder<> Builder(DetourBlock);
                Builder.CreateStore(ConstantInt::get(Builder.getInt32Ty(), ++dispatchVal), DispatchVar);
                Builder.CreateBr(DispatchBlock);

                // insert block after our current one
                // block -> DetourBlock
                br->setSuccessor(i, DetourBlock);
                DetourBlock->moveAfter(block);

                // Add a new branch in the dispatcher to jump to the new block
                // DispatchBlock
                // if (DispatchVar == dispatchVal) goto successor;
                Instruction* FirstInst = DispatchBlock->getFirstNonPHI();
                IRBuilder<> DispatchBuilder(FirstInst);
                LoadInst* loadSwitchVar = DispatchBuilder.CreateLoad(DispatchBuilder.getInt32Ty(), DispatchVar, "dispatch_var"); // some PHI weirdness means we need to load this here
                auto *Cond = DispatchBuilder.CreateICmpEQ(ConstantInt::get(DispatchBuilder.getInt32Ty(), dispatchVal), loadSwitchVar);
                SplitBlockAndInsertIfThen(Cond, FirstInst, false, nullptr, (DomTreeUpdater *)nullptr, nullptr, Successor);
            }
        }
    }

    bool flattenFunction(Function& F) {
        outs() << "Flattening " << F.getName() << "\n";
        outs() << F.getInstructionCount() << " instructions\n";

        if(F.getInstructionCount() < 1) {
            outs() << "Skipping\n";
            return false;
        }

        int dispatchVal = 0x1001;

        printFunction(F);
        auto flattenedBB = getBlocksToFlatten(F);
        demotePhiNodes(F);
        AllocaInst *DispatchVar = initDispatchVar(F, dispatchVal);
        BasicBlock &entryBlockTail = splitBranchOffEntryBlock(F);
        flattenedBB.push_back(&entryBlockTail);
        BasicBlock* DispatchBlock = insertDispatchBlockAfterEntryBlock(F, DispatchVar);

        for (BasicBlock* pToflatBB : flattenedBB){
            flattenBlock(pToflatBB, dispatchVal, F, DispatchVar, DispatchBlock);
        }

        outs() << "After flattening:\n";
        printFunction(F);

        return true;
    }

    struct ControlFlowFlattening : public PassInfoMixin<ControlFlowFlattening> {
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {                     
            for (Function& F : M) {
                flattenFunction(F);
            }
            return PreservedAnalyses::none();
        }
    };
}

PassPluginLibraryInfo getPassPluginInfo() {
  static std::atomic<bool> ONCE_FLAG(false);
  return {LLVM_PLUGIN_API_VERSION, "obfs", "0.0.1",
          [](PassBuilder &PB) {

            try {
              PB.registerPipelineEarlySimplificationEPCallback(
                [&] (ModulePassManager &MPM, OptimizationLevel opt) {
                  if (ONCE_FLAG) {
                    return true;
                  }
                MPM.addPass(obfs::ControlFlowFlattening());
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
