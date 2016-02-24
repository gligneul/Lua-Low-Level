/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllvararg.h
** Compiles the vararg opcode
*/

#ifndef LLLVARARG_H
#define LLLVARARG_H

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Value.h>

namespace lll {

class CompilerState;

class Vararg {
public:
    // Compiles the opcode
    static void Compile(CompilerState& cs);

private:
    Vararg(CompilerState& cs);

    void ComputeAvailableArgs();
    void ComputeRequiredArgs();
    llvm::BasicBlock* ComputeNMoves();
    llvm::BasicBlock* MoveAvailable(llvm::BasicBlock* entry);
    void FillRequired(llvm::BasicBlock* entry);

    llvm::Value* GetRegisterFromA(llvm::Value* offset);

    CompilerState& cs_;
    llvm::Value* available_;
    llvm::Value* required_;
    llvm::Value* nmoves_;
};

}

#endif

