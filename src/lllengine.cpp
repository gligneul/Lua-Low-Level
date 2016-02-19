/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllengine.cpp
*/

#include <iostream>

#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

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

void Engine::Write(const std::string& path) {
    (void)path;
    std::string module_str;
    llvm::raw_string_ostream module_os(module_str);
    module_->print(module_os, nullptr);
    std::cout << module_str;
}

}

