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

#include <vector>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Value.h>

#include "lllopcode.h"

namespace lll {

class Vararg : public Opcode<Vararg> {
public:
    // Constructor
    Vararg(CompilerState& cs);

    // Returns the list of steps
    std::vector<CompilationStep> GetSteps();

private:
    // Compilation steps
    llvm::BasicBlock* ComputeAvailableArgs(llvm::BasicBlock* entry);
    llvm::BasicBlock* ComputeRequiredArgs(llvm::BasicBlock* entry);
    llvm::BasicBlock* ComputeNMoves(llvm::BasicBlock* entry);
    llvm::BasicBlock* MoveAvailable(llvm::BasicBlock* entry);
    llvm::BasicBlock* FillRequired(llvm::BasicBlock* entry);

    // Retuns the register at ra + offset
    llvm::Value* GetRegisterFromA(llvm::Value* offset);

    CompilerState& cs_;
    llvm::Value* available_;
    llvm::Value* required_;
    llvm::Value* nmoves_;
};

}

#endif

