#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/Local.h"
#define DEBUG_TYPE "remove-dead-code"

using namespace llvm;
uint64_t getInstructionsNumber(BasicBlock &BB){
    uint64_t numInstructions = 0;
    for (Instruction &I : BB) {
      numInstructions++;
    }
    return numInstructions;  
}
bool findUse(BasicBlock &BB, Instruction &I , Value* address) {
    Instruction *Inst = I.getNextNode();
    // Iterate on the same block
    while (Inst) {

         for (llvm::Value* operand : Inst->operands()) {
                            if (operand == address) {
                                    fprintf(stderr,"used in :");
                                    errs() << *Inst << "\n";
                                    return true;
                            }
        }
        Inst = Inst->getNextNode();
        
    } 
    // Iterate on the rest blocks
    BasicBlock *B = BB.getNextNode();
    while(B) {
        for (Instruction &Instruction : *B) {
             for (llvm::Value* operand : Instruction.operands()) {
                            if (operand == address) {
                                    printf("used in :");
                                    errs() << Instruction << "\n";
                                    return true;
                            }
              }
        }
        B = B->getNextNode();
        }

    return false;
}
llvm::BasicBlock* getNextBlock(llvm::BasicBlock &BB) {
    llvm::Function *F = BB.getParent();  // Get the function to which the block belongs

    // Iterate over the basic blocks in the function
    for (auto it = F->begin(), end = F->end(); it != end; ++it) {
        // Check if we found the given block
        if (&*it == &BB) {
            // If it's not the last block, return the next block
            auto next = std::next(it);
            if (next != end) {
                return &*next;  // Return the next block
            }
            break;
        }
    }

    return nullptr;  // Return nullptr if the given block is the last one
}


namespace {
    struct DeadCodeEliminationPass : PassInfoMixin<DeadCodeEliminationPass> {
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
            errs() << "Running DeadCodeEliminationPass on function " << F.getName() << "\n";
            bool changed = false;
            bool block_mod = false;
            

      
                Instruction *Inst_toerase = NULL;
                BasicBlock *Block_toerase = NULL;
    do {
        block_mod = false;
        for (BasicBlock &BB : F) {
                    // check if a block has one instruction and that instruction is unconditional branch;
                    if (Block_toerase) {
                        Block_toerase->eraseFromParent();
                        Block_toerase = NULL;
                    }


                    // CHECK IF A BLOCK HAS A SINGLE UNCONDITIONAL BRANCH WHERE THE BRANCH DESTINATION IS THE NEXT BLOCK
                    uint64_t inst_counts = getInstructionsNumber(BB);
                    if (inst_counts == 1 && isa<llvm::BranchInst>(*BB.begin())) {
                        llvm::BranchInst &branchInst = cast<llvm::BranchInst>(*BB.begin());
                        if (!branchInst.isConditional()) {
                            // if our unconditional branch contains as a target the next block we remove it and link the previous block with the next block:
                            llvm::BasicBlock *block_dest = branchInst.getSuccessor(0);
                            printf("unconditional branch\n");
                            llvm::errs() << "Instruction: " << branchInst ;
                            llvm::errs() << "  BasicBlock Name: " << BB.getName() << "\n";
                            llvm::BasicBlock *block_next = getNextBlock(BB);

                            if (block_next == block_dest) {
                                // link the previous of this block with the next of the other block;
                                //to do
                                    BasicBlock *block_pred = NULL;

                                    for (llvm::BasicBlock *pred : predecessors(&BB)) {
                                         block_pred = pred;  // Get the first predecessor (there should be one for a single branch)
                                         break;
                                    }

                                  if (block_pred) {
                                      // Link the predecessor to the successor
                                         if (block_next) {
                                            block_pred->getTerminator()->replaceUsesOfWith(&BB, block_next);
                                         }                                     
                                         

                                         BB.replaceAllUsesWith(UndefValue::get(BB.getType()));  // Clear out uses of the block in other places
                                         Block_toerase = &BB;
                                         changed = true;  // Indicate that a change has been made
                                         block_mod = true;
                                         continue;
                                   }
                                   else {
                                        //if it's the first block
                                        Block_toerase = &BB;
                                        changed = true;  // Indicate that a change has been made
                                        block_mod = true;
                                        continue;

                                   }

                     }
                            

                        }
                    }
                    // END


                    for (Instruction &I : BB) {
                        
                        if (Inst_toerase) {
                            Inst_toerase->eraseFromParent();
                            Inst_toerase = NULL;
                        }

                        //  CHECK TRIVIAL
                        if (isInstructionTriviallyDead(&I)) {
                            printf("trivially dead\n");
                            // delete the marked Instruction at the next iteration so that we don't delete while iterating; causing iterator problems
                            Inst_toerase = &I;
                            block_mod = true;
                            changed = true;
                            continue;
                        }
                        // END
                        
                        // CHECK CONDITIONAL WITH SAME DESTINATION
                        if(isa<llvm::BranchInst>(I)) {
                                llvm::BranchInst &branchInst = cast<llvm::BranchInst>(I);
                                // check if the branch is conditional
                                if (branchInst.isConditional()) {
                                    // check if in both cases the branch will go to the same block;
                                    llvm::BasicBlock *block_true = branchInst.getSuccessor(0);
                                    llvm::BasicBlock *block_false = branchInst.getSuccessor(1);

                                    if (block_true == block_false) {
                                         // create an unconditional branch and insert it in the right pos;
                                         llvm::BranchInst::Create(block_true, &branchInst);
                                         block_mod = true;
                                         changed = true;
                                         Inst_toerase = &branchInst;  
                                         continue;         
                                    }
                                 
                                } 
                        }
                        // END  

                        // LOAD AND STORE PART :
                        if(isa<llvm::StoreInst>(I)) {
                               printf("store \n");
                               llvm::StoreInst &storeInst = cast<llvm::StoreInst>(I);
                               llvm::Value *address = storeInst.getOperand(1);
                               bool possible_use = false;
                               if (!findUse(BB,I,address)) {
                                Inst_toerase = &I;
                                changed = true;
                                block_mod = true;
                                continue;
                               }

                                                             
                        }
                        if (isa<llvm::LoadInst>(I)) {
                            if (I.use_empty()) {
                                  Inst_toerase = &I;   
                                  changed = true;
                                  block_mod = true;
                                  continue;
                            }
                        }

                    }


                    if (Inst_toerase) {
                            Inst_toerase->eraseFromParent();
                            Inst_toerase = NULL;
                        }

                }
    } while (block_mod);  
  
            return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
        }
    };
} // namespace

/// Registration
PassPluginLibraryInfo getPassPluginInfo() {
    const auto callback = [](PassBuilder &PB) {
        PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM, auto) {
                    if (Name == "dead-code-elimination") {
                        FPM.addPass(DeadCodeEliminationPass());
                        return true;
                    }
                    return false;
                });
    };
    return {LLVM_PLUGIN_API_VERSION, "DeadCodeEliminationPass",
            LLVM_VERSION_STRING, callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getPassPluginInfo();
}

#undef DEBUG_TYPE
