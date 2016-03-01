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
    // The derived class must implement the GetSteps method that returns a list
    // of compilations steps
    typedef void (Op::*CompilationStep)();

    // List of incomming values and blocks
    typedef std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>
            IncomingList;

    // Static method that gather the compilation steps and calls one by one
    template<typename... Args>
    static void Compile(CompilerState& cs, Args... args) {
        Op o(cs, args...);
        auto steps = o.GetSteps();
        for (auto& step : steps)
            (o.*step)();
    }

    // Default contructor
    Opcode(CompilerState& cs) :
        cs_(cs),
        B_(cs_.builder_),
        entry_(cs.blocks_[cs.curr_]),
        exit_(cs.blocks_[cs.curr_ + 1]) {
    }

protected:
    // Creates a phi value and adds it's incoming values
    llvm::Value* CreatePHI(llvm::Type* type, const IncomingList& incoming,
            const std::string& name) {
        auto phi = B_.CreatePHI(type, incoming.size(), name);
        for (auto& i : incoming)
            phi->addIncoming(i.first, i.second);
        return phi;
    }

    CompilerState& cs_;
    llvm::IRBuilder<>& B_;
    llvm::BasicBlock* entry_;
    llvm::BasicBlock* exit_;
};

}

#endif

