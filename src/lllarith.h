/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllarith.h
** Compiles the arithmetics opcodes
*/

#ifndef LLLARITH_H
#define LLLARITH_H

#include "lllvalue.h"
#include "lllopcode.h"

namespace lll {

class CompilerState;

class Arith : public Opcode<Arith> {
public:
    // Constructor
    Arith(CompilerState& cs);

    // Returns the list of steps
    std::vector<CompilationStep> GetSteps();

private:
    // Compilation steps
    void SwitchTags();
    void ComputeInt();
    void ComputeFloat();
    void ComputeTaggedMethod();

    // Returns whether the opcode can perform an integer operation
    bool HasIntegerOp();

    // Go to the target block depending on the tag:
    // switch (tag)
    //   case INT:          goto intop;
    //   case FLOAT:        goto floatop;
    //   default:
    //     if (tonumber())  goto convop;
    //     else             goto tmop_;
    void SwitchTagCase(Value& value, llvm::Value* tag, llvm::Value* convptr,
            llvm::BasicBlock* entry, llvm::BasicBlock* intop,
            llvm::BasicBlock* floatop, llvm::BasicBlock* convop);

    // Performs the integer/float binary operation
    llvm::Value* PerformIntOp(llvm::Value* a, llvm::Value* b);
    llvm::Value* PerformFloatOp(llvm::Value* a, llvm::Value* b);
    
    // Obtains the corresponding tag for the opcode
    int GetMethodTag();

    Value ra_;
    Value rb_;
    Value rc_;
    llvm::BasicBlock* intop_;
    llvm::BasicBlock* floatop_;
    llvm::BasicBlock* tmop_;
    IncomingList nbinc_;
    IncomingList ncinc_;
};

}

#endif

