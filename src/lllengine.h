/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllengine.h
*/

#ifndef LLLENGINE_H
#define LLLENGINE_H

#include <memory>

#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace lll {

class Engine {
public:
    Engine(llvm::ExecutionEngine* ee, llvm::Module* module);

    // Gets the compiled function
    void* GetFunction();

    // Dumps the compiled modules
    void Dump();

private:
    std::unique_ptr<llvm::ExecutionEngine> ee_;
    llvm::Module* module_;
    void* function_;
};

}

#endif

