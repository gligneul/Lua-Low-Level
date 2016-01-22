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

#include <string>

struct lua_State;
struct LClosure;

namespace lll {

class Engine;

class Compiler {
public:
    Compiler(lua_State* L, LClosure* lclosure);

    // Starts the function compilation
    // Returns false if it fails
    bool Compile();

    // Gets the compilation error message
    const std::string& GetErrorMessage();

    // Gets the engine (only if compilation succeeds)
    Engine* GetEngine();

private:
    lua_State* lua_state_;
    LClosure* lclosure_;
    std::string error_;
    Engine* engine_;
};

}

#endif

