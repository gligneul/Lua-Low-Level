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

    // Compiles the Lua proto instructions
    void CompileInstructions();

    // Returns true if the module doesn't have any error
    bool VerifyModule();

    // Creates the engine and returns true if it doesn't have any error
    bool CreateEngine();

    // Compiles the specific instruction
    void CompileMove();
    void CompileLoadk(bool extraarg);
    void CompileLoadbool();
    void CompileLoadnil();
    void CompileGetupval();
    void CompileGettabup();
    void CompileGettable();
    void CompileSettabup();
    void CompileSetupval();
    void CompileSettable();
    void CompileNewtable();
    void CompileSelf();
    void CompileBinop(const std::string& function);
    void CompileUnop(const std::string& function);
    void CompileConcat();
    void CompileJmp();
    void CompileCmp(const std::string& function);
    void CompileTest();
    void CompileTestset();
    void CompileCall();
    void CompileReturn();
    void CompileCheckcg(llvm::Value* reg);

    // Makes a llvm int type
    llvm::Type* MakeIntT(int nbytes);

    // Makes a llvm int value
    llvm::Value* MakeInt(int value);

    // Obtains the pointer to the field at $offset
    llvm::Value* GetFieldPtr(llvm::Value* strukt, llvm::Type* fieldtype,
            size_t offset, const std::string& name);

    // Loads the field at $offset
    llvm::Value* LoadField(llvm::Value* strukt, llvm::Type* fieldtype,
            size_t offset, const std::string& name);

    // Sets a field at $offset
    void SetField(llvm::Value* strukt, llvm::Value* fieldvalue, size_t offset,
            const std::string& fieldname);

    // Gets a value at $arg
    llvm::Value* GetValueR(int arg, const std::string& name);
    llvm::Value* GetValueK(int arg, const std::string& name);
    llvm::Value* GetValueRK(int arg, const std::string& name);

    // Gets the upvalue $n
    llvm::Value* GetUpval(int n);

    // Obtains a runtime function
    llvm::Function* GetFunction(const std::string& name);

    // Sets a register with a value
    void SetRegister(llvm::Value* reg, llvm::Value* value);

    // Sets the top of the stack
    void SetTop(int reg);

    // Updates the func variable; should be called before getting a register
    void UpdateStack();

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
        llvm::Value* k;
        llvm::Value* func;
    } values_;
    int curr_;
    Instruction instr_;
};

}

#endif

