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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#pragma clang diagnostic pop

namespace lll {

class Engine {
public:
    Engine(llvm::ExecutionEngine* ee, llvm::Module* module,
            llvm::Function* function);

    // Gets the compiled function
    void* GetFunction();

    // Dumps the compiled modules
    void Dump();

    // Writes the bytecode and asm files
    void Write(const std::string& path);

private:
    std::unique_ptr<llvm::ExecutionEngine> ee_;
    llvm::Module* module_;
    void* function_;
};

}

#endif

