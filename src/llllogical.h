/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** llllogical.h
** Compiles the logical opcodes:
** OP_BAND, OP_BOR, OP_BXOR, OP_SHL, OP_SHR
*/

#ifndef LLLLOGICAL_H
#define LLLLOGICAL_H

#include "lllopcode.h"
#include "lllvalue.h"

namespace lll {

class CompilerState;

class Logical : public Opcode<Logical> {
public:
    // Constructor
    Logical(CompilerState& cs);

    // Returns the list of steps
    std::vector<CompilationStep> GetSteps();

private:
    // Compilation steps
    void ComputeInteger();
    void ComputeTaggedMethod();

    // Performs the integer binary operation
    llvm::Value* PerformIntOp(llvm::Value* a, llvm::Value* b);
    
    // Obtains the corresponding tag for the opcode
    int GetMethodTag();

    Value ra_;
    Value rb_;
    Value rc_;
    llvm::BasicBlock* trytm_;
};

}

#endif

