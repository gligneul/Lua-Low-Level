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

namespace lll {

class Register;
class Value;

class Logical : public Opcode {
public:
    // Constructor
    Logical(CompilerState& cs, Stack& stack);

    // Compiles the opcode
    void Compile();

private:
    // Compilation steps
    void ComputeInteger();
    void ComputeTaggedMethod();

    // Performs the integer binary operation
    llvm::Value* PerformIntOp(llvm::Value* a, llvm::Value* b);
    
    // Obtains the corresponding tag for the opcode
    int GetMethodTag();

    Register& ra_;
    Value& rkb_;
    Value& rkc_;
    llvm::BasicBlock* trytm_;
};

}

#endif

