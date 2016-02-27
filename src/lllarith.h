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
    llvm::BasicBlock* Compute(llvm::BasicBlock* entry);
    llvm::BasicBlock* ComputeTaggedMethod(llvm::BasicBlock* entry);

    // Returns whether the opcode can perform an integer operation
    bool HasIntegerOp();

    // Go to the target block depending on the tag
    void SwitchTag(Value& value, llvm::Value* tag, llvm::Value* convptr,
            llvm::BasicBlock* entry, llvm::BasicBlock* intop,
            llvm::BasicBlock* floatop, llvm::BasicBlock* convop,
            llvm::BasicBlock* tmop);

    // Performs the integer/float binary operation
    llvm::Value* PerformIntOp(llvm::Value* lhs, llvm::Value* rhs);
    llvm::Value* PerformFloatOp(llvm::Value* lhs, llvm::Value* rhs);
    
    // Obtains the corresponding tag for the opcode
    int GetMethodTag();

    CompilerState& cs_;
    llvm::Value* ra_;
    Value b_;
    Value c_;
};

}

#endif

