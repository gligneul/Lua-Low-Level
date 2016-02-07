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

/* Number of calls necessary to auto-compile */
#ifndef LLL_CALLS_TO_COMPILE 
#define LLL_CALLS_TO_COMPILE 50
#endif

/* Compiles a function and attachs it to the closure
** In success returns 0, else returns 1
** If errmsg != NULL also returns the error message (must be freed) */
int LLLCompile (lua_State *L, LClosure *cl, char **errmsg);

/* Destroys the engine */
void LLLFreeEngine (lua_State *L, LClosure *cl);

/* Dumps the LLMV function (debug) */
void LLLDump (LClosure *cl);

#endif

