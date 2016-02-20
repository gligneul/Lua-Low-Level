/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllengine.cpp
*/

#include <fstream>
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
    std::string module_str;
    llvm::raw_string_ostream module_os(module_str);
    module_->print(module_os, nullptr);
    auto filename = path + ".bc";
    std::ofstream fout(filename);
    if (fout.is_open()) {
        fout << module_str;
        fout.close();
        system(("llc -O2 " + filename).c_str());
    } else {
        std::cerr << "Engine::Write: Couldn't save the file " << filename
                  << "\n";
    }
}

}

