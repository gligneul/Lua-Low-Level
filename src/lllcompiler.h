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

#include <memory>
#include <string>

#include "lllcompilerstate.h"
#include "lllvalue.h"

namespace lll {

class Engine;

class Compiler {
public:
    // Constructor, receiver the proto that will be compiled
    Compiler(lua_State* L, Proto* proto);

    // Starts the function compilation
    // Returns false if it fails
    bool Compile();

    // Gets the compilation error message
    const std::string& GetErrorMessage();

    // Gets the engine (only if compilation succeeds)
    Engine* GetEngine();

private:
    // Compiles the Lua proto instructions
    bool CompileInstructions();

    // Returns true if the module doesn't have any error
    bool VerifyModule();

    // Optimize the generated module
    bool OptimizeModule();

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
    void CompileUnop(const std::string& function);
    void CompileConcat();
    void CompileJmp();
    void CompileCmp(const std::string& function);
    void CompileTest();
    void CompileTestset();
    void CompileCall();
    void CompileTailcall();
    void CompileReturn();
    void CompileForloop();
    void CompileForprep();
    void CompileTforcall();
    void CompileTforloop();
    void CompileSetlist();
    void CompileClosure();
    void CompileCheckcg(llvm::Value* reg);

    std::string error_;
    CompilerState cs_;
    Stack stack_;
    std::unique_ptr<Engine> engine_;
};

}

#endif

