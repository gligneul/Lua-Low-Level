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

#include <memory>

#include "lllopcode.h"

namespace lll {

class Value;
class Register;

class Arith : public Opcode {
public:
    // Constructor
    Arith(CompilerState& cs);

    // Compiles the opcode
    void Compile();

private:
    // Compilation steps
    void SwitchTags();
    void ComputeInt();
    void ComputeFloat();
    void ComputeTaggedMethod();

    // Returns whether the opcode can perform an integer operation
    bool HasIntegerOp();

    // Go to the target block depending on the tag:
    // if istagint
    //   goto intop
    // elseif istagfloat
    //   goto floatop
    // elseif tonumber
    //   goto convop
    // else
    //   goto tmop
    void SwitchTagCase(Value* value, llvm::Value* tag, llvm::Value* convptr,
            llvm::BasicBlock* entry, llvm::BasicBlock* intop,
            llvm::BasicBlock* floatop, llvm::BasicBlock* convop, bool intfirst);

    // Performs the integer/float binary operation
    llvm::Value* PerformIntOp(llvm::Value* a, llvm::Value* b);
    llvm::Value* PerformFloatOp(llvm::Value* a, llvm::Value* b);
    
    // Obtains the corresponding tag for the opcode
    int GetMethodTag();

    std::unique_ptr<Register> ra_;
    std::unique_ptr<Value> rkb_;
    std::unique_ptr<Value> rkc_;
    llvm::BasicBlock* intop_;
    llvm::BasicBlock* floatop_;
    llvm::BasicBlock* tmop_;
    IncomingList nbinc_;
    IncomingList ncinc_;
};

}

#endif

