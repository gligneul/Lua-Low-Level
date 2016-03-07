/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllopcode.cpp
*/

#include "lllcompilerstate.h"
#include "lllopcode.h"

namespace lll {

Opcode::Opcode(CompilerState& cs) :
    cs_(cs),
    entry_(cs.blocks_[cs.curr_]),
    exit_(cs.blocks_[cs.curr_ + 1]) {
}

llvm::Value* Opcode::CreatePHI(llvm::Type* type, const IncomingList& incoming,
            const std::string& name) {
    auto phi = cs_.B_.CreatePHI(type, incoming.size(), name);
    for (auto& i : incoming)
        phi->addIncoming(i.first, i.second);
    return phi;
}

}

