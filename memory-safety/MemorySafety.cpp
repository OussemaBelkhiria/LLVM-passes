#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#define DEBUG_TYPE "memory-safety"

using namespace llvm;

namespace {
    struct MemorySafetyPass : PassInfoMixin<MemorySafetyPass> {
    public:
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
            errs() << "Running MemorySafetyPass on function " << F.getName() << "\n";
            Instruction* Inst_toerase = NULL;
            bool changed = false;
            for (BasicBlock &BB : F) {
                for (Instruction &I : BB) {
                
                    if (Inst_toerase) {
                        Inst_toerase->eraseFromParent();
                        Inst_toerase = NULL;
                    }

                    if(isa<llvm::CallInst>(I)) {
                        llvm::CallInst &Inst = cast<llvm::CallInst>(I);
                        if (Inst.getCalledFunction()) {
                           
                            if (Inst.getCalledFunction()->getName() == "malloc") {
                                
                                IRBuilder <> Builder(&Inst);
                                FunctionCallee run_malloc = Inst.getModule()->getOrInsertFunction("__runtime_malloc", Inst.getFunctionType());
                                CallInst* newInst = Builder.CreateCall(run_malloc, {Inst.getArgOperand(0)});
                                Inst.replaceAllUsesWith(newInst);
                                Inst_toerase = &Inst;
                                changed = true;
                                
                            }

                            else if (Inst.getCalledFunction()->getName() == "free") {
                                IRBuilder <> Builder(&Inst);
                                FunctionCallee run_free = Inst.getModule()->getOrInsertFunction("__runtime_free", Inst.getFunctionType());
                                CallInst* newInst = Builder.CreateCall(run_free, {Inst.getArgOperand(0)});
                                Inst.replaceAllUsesWith(newInst);
                                Inst_toerase = &Inst;
                                changed = true;
                            }
            
                        }
                    }
                    else if (isa<llvm::StoreInst>(I)) {
                        llvm::StoreInst &Inst = cast<llvm::StoreInst>(I);
                        IRBuilder <> Builder(&Inst);
                        LLVMContext &Context = Inst.getModule()->getContext();
                        FunctionCallee run_chck = Inst.getModule()->getOrInsertFunction("__runtime_check_addr", FunctionType::get(Type::getVoidTy(Context), {Type::getInt8PtrTy(Context)}, false));
                        Builder.CreateCall(run_chck, {Inst.getPointerOperand()}); 
                        changed = true;
                    }
                    else if (isa<llvm::LoadInst>(I)) {
                        llvm::LoadInst &Inst = cast<llvm::LoadInst>(I);
                        IRBuilder <> Builder(&Inst);
                        LLVMContext &Context = Inst.getModule()->getContext();
                        FunctionCallee run_chck = Inst.getModule()->getOrInsertFunction("__runtime_check_addr", FunctionType::get(Type::getVoidTy(Context), {Type::getInt8PtrTy(Context)}, false));
                        Builder.CreateCall(run_chck, {Inst.getPointerOperand()}); 
                        changed = true;
                    }
                    else if (isa<llvm::AllocaInst>(I)) {
                        llvm::AllocaInst &Inst = cast<llvm::AllocaInst>(I);
                        llvm::Type *allocaType = Inst.getAllocatedType();
                    
                        std::optional<llvm::TypeSize> rv = Inst.getAllocationSize(Inst.getParent()->getParent()->getParent()->getDataLayout());
                        if (rv.has_value()) {
                            llvm::TypeSize size = rv.value();
                            size_t TotalSize = size.getKnownMinValue() + 32;

                            fprintf(stderr,"Totalsize %lu\n", TotalSize);

                            //create a new Instruction that do alloca with the given totalsize and preserve the given align parameter
                            llvm::IRBuilder<> Builder(&Inst);
                            llvm::ConstantInt *TotalSizeConst = llvm::ConstantInt::get(Inst.getContext(), llvm::APInt(64, TotalSize));
                            llvm::AllocaInst *newAlloca = Builder.CreateAlloca(llvm::Type::getInt32Ty(Inst.getContext()), TotalSizeConst);
                            
                            newAlloca->setAlignment(Inst.getAlign());
                            
                            //mark the old alloca to erase;
                            Inst_toerase = &Inst;
                            changed = true;

                            // create a next instruction that uses a function from the runtime library and insert it directly after the new alloca :
                            llvm::FunctionCallee run_stack = Inst.getModule()->getOrInsertFunction("__runtime_stack",llvm::FunctionType::get(llvm::Type::getInt8PtrTy(Inst.getContext()), 
                                                                 {llvm::Type::getInt8PtrTy(Inst.getContext()), llvm::Type::getInt64Ty(Inst.getContext())}, false));
                            
                            
                            llvm :: Value *newAlloca_cast = Builder.CreateBitCast(newAlloca, llvm::Type::getInt8PtrTy(Inst.getContext()));
        
                            llvm::Value *stackPtr = Builder.CreateCall(run_stack, {newAlloca_cast, TotalSizeConst});
                            
                            llvm::Value * stackPtr_cast = Builder.CreateBitCast(stackPtr, Inst.getType());

                            Inst.replaceAllUsesWith(stackPtr); 

                            }
                        
                    }
                }
            }

    
            if (Inst_toerase) {
                        Inst_toerase->eraseFromParent();
                    }
            return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
        }

        // do not skip this pass for functions annotated with optnone
        static bool isRequired() { return true; }
    };
} // namespace

/// Registration
PassPluginLibraryInfo getPassPluginInfo() {
    const auto callback = [](PassBuilder &PB) {
        PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM, auto) {
                    if (Name == "memory-safety") {
                        FPM.addPass(MemorySafetyPass());
                        return true;
                    }
                    return false;
                });
    };
    return {LLVM_PLUGIN_API_VERSION, "MemorySafetyPass",
            LLVM_VERSION_STRING, callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getPassPluginInfo();
}

#undef DEBUG_TYPE
