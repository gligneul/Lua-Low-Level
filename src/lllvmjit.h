/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: eof
**
** llvmjit.h
** This is the interface for jit compiling Lua bytecode into LLVM IR
*/

#ifndef lllvmjit_h
#define lllvmjit_h

#include "lstate.h"
#include "lobject.h"

/* Holds all jit data of a function */
typedef struct luaJ_Function luaJ_Function;

/* Optimization level for LLVM's jit compiler */
#define LUAJ_LLVMOPT 3

/* Compiles a function */
LUAI_FUNC luaJ_Function *luaJ_compile (lua_State *L, StkId closure);

/* Executes the LLVM function; Returns the number of results */
LUAI_FUNC int luaJ_call (lua_State *L, luaJ_Function *f);

/* Dumps the LLMV function (debug) */
LUAI_FUNC void luaJ_dump (lua_State *L, luaJ_Function *f);

/* Destroys a Func instance */
LUAI_FUNC void luaJ_freefunction (lua_State *L, luaJ_Function *f);

/*
** The MIT License (MIT)
** 
** Copyright (c) 2015 Gabriel de Quadros Ligneul
** 
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
** 
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
** 
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
*/

#endif

