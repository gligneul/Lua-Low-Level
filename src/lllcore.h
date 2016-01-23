/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL:
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
**
** lllcore.h
** LLL C interface
*/

#ifndef LLLCORE_H
#define LLLCORE_H

#include "lstate.h"
#include "lobject.h"

/* Compiled function type */
typedef int (*LLLFunction) (lua_State*, LClosure*);

/* Compiled function engine */
typedef struct LLLEngine {
    LLLFunction function;
    LClosure *lclosure;
    void *data;
} LLLEngine;

/* Compiles a Lua function and returns the engine
** If an error occurs, returns NULL and a message */
LLLEngine *LLLCompile (lua_State *L, LClosure *lclosure, const char **error);

/* Destroys an engine */
void LLLFreeEngine (lua_State *L, LLLEngine *e);

/* Executes the compiled function; Returns the number of results */
int LLLCall (lua_State *L, LLLEngine *e);

/* Dumps the LLMV function (debug) */
void LLLDump (LLLEngine *e);

#endif

