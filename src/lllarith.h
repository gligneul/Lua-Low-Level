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

#include "lllopcode.h"

namespace lll {

class Register;
class Value;

class Arith : public Opcode {
public:
    // Constructor
    Arith(CompilerState& cs, Stack& stack);

    // Compiles the opcode
    void Compile();

private:
    // Compilation steps
    void CheckXTag();
    void CheckYTag();
    void ComputeInt();
    void ComputeFloat();
    void ComputeTaggedMethod();

    // Returns whether the opcode can perform an integer operation
    bool HasIntegerOp();

    // Performs the integer/float binary operation
    llvm::Value* PerformIntOp(llvm::Value* lhs, llvm::Value* rhs);
    llvm::Value* PerformFloatOp(llvm::Value* lhs, llvm::Value* rhs);
    
    // Obtains the corresponding tag for the opcode
    int GetMethodTag();

    Register& ra_;
    Value& rkb_;
    Value& rkc_;
    Value& x_;
    Value& y_;
    llvm::BasicBlock* check_y_;
    llvm::BasicBlock* intop_;
    llvm::BasicBlock* floatop_;
    llvm::BasicBlock* tmop_;
    llvm::Value* x_int_;
    llvm::Value* x_float_;
    IncomingList x_float_inc_;
    IncomingList y_float_inc_;
};

}

#endif

