/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllopcode.h
** Superclass for opcodes compilations
*/

#ifndef LLLOPCODE_H
#define LLLOPCODE_H

#include "lllcompilerstate.h"

namespace lll {

// Op should be the derived class
template<typename Op>
class Opcode {
public:
    // The type of a compilation step
    // It will receive the entry block and must return the exit block
    // The derived class must implement the GetSteps method that returns a list
    // of compilations steps
    typedef llvm::BasicBlock* (Op::*CompilationStep)(llvm::BasicBlock*);

    // Static method that gather the compilation steps and calls one by one
    template<typename... Args>
    static void Compile(CompilerState& cs, Args... args) {
        Op o(cs, args...);
        auto steps = o.GetSteps();
        auto b = cs.blocks_[cs.curr_];
        for (auto& step : steps) {
            cs.builder_.SetInsertPoint(b);
            b = (o.*step)(b);
        }
        if (!b->getTerminator()) {
            cs.builder_.SetInsertPoint(b);
            cs.builder_.CreateBr(cs.blocks_[cs.curr_ + 1]);
        }
    }

    // Default contructor
    Opcode(CompilerState& cs) :
        cs_(cs),
        B_(cs_.builder_) {
    }

protected:
    CompilerState& cs_;
    llvm::IRBuilder<>& B_;
};

}

#endif

