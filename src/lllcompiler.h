/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllcompiler.h
*/

#ifndef LLLCOMPILER_H
#define LLLCOMPILER_H

#include <vector>
#include <string>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>

extern "C" {
#include "llimits.h"
}

struct lua_State;
struct LClosure;

namespace lll {

class Engine;
class Runtime;

class Compiler {
public:
    Compiler(LClosure* lclosure);

    // Starts the function compilation
    // Returns false if it fails
    bool Compile();

    // Gets the compilation error message
    const std::string& GetErrorMessage();

    // Gets the engine (only if compilation succeeds)
    Engine* GetEngine();

private:
    // Creates the main function
    llvm::Function* CreateMainFunction();

    // Creates one basic block for each instruction
    void CreateBlocks();

    // Obtains the current instruction
    Instruction GetInstruction();

    // Compiles the Lua proto instructions
    void CompileInstructions();

    // Compiles the specific instruction
    void CompileReturn();

    // Returns true if the module doesn't have any error
    bool VerifyModule();

    // Creates the engine and returns true if it doesn't have any error
    bool CreateEngine();

    llvm::Value* CreateInt(int value);

    // Obtains the pointer to the field at $offset
    llvm::Value* GetFieldPtr(llvm::Value* strukt, llvm::Type* fieldtype,
            size_t offset, const std::string& name);

    // Loads the field at $offset
    llvm::Value* LoadField(llvm::Value* strukt, llvm::Type* fieldtype,
            size_t offset, const std::string& name);

    LClosure* lclosure_;
    std::string error_;
    llvm::LLVMContext& context_;
    Runtime* rt_;
    std::unique_ptr<Engine> engine_;
    std::unique_ptr<llvm::Module> module_;
    llvm::Function* function_;
    std::vector<llvm::BasicBlock*> blocks_;
    llvm::IRBuilder<> builder_;
    struct Values {
        llvm::Value* state;
        llvm::Value* closure;
        llvm::Value* ci;
        llvm::Value* func;
    } values_;
    int curr_;
};

}

#endif

