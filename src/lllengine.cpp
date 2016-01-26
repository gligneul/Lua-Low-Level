/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllengine.cpp
*/

#include <llvm/IR/Module.h>

#include "lllengine.h"

namespace lll {

Engine::Engine(llvm::ExecutionEngine* ee, llvm::Module* module) :
    ee_(ee),
    module_(module),
    function_(ee->getPointerToNamedFunction("lll")) {
}

void* Engine::GetFunction() {
    return function_;
}

void Engine::Dump() {
    module_->dump();
}

}

