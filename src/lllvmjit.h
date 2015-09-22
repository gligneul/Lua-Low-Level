/*
** LLL - Lua Low-Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see bellow
**
** llvmjit.h
** This is the interface for jit compiling Lua bytecode into LLVM IR
*/

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

#ifndef lllvmjit_h
#define lllvmjit_h

#include "lstate.h"
#include "lobject.h"

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>


/*
** Holds all LLVM data that must be attached to the Proto
*/
typedef struct luaJ_Func {
  LLVMModuleRef module;             /* Every function have it's own module. */
  LLVMValueRef value;               /* Pointer to the function */
} luaJ_Func;


/*
** Global information that is attached to the lua_State
*/
typedef struct luaJ_Engine {
  LLVMExecutionEngineRef engine;    /* Execution engine for LLVM */
  LLVMModuleRef module;             /* Global module with aux functions */
  LLVMTypeRef t_tvalue;             /* TValue*/
  LLVMTypeRef t_gcobject;           /* GCValue */
  LLVMTypeRef t_callinfo;           /* CallInfo */
  LLVMTypeRef t_callinfo_l;         /* CallInfo.u.l */
  LLVMTypeRef t_callinfo_c;         /* CallInfo.u.c */
  LLVMTypeRef t_state;              /* lua_State */
} luaJ_Engine;


/* Optimization level por LLVM's jit compiler */
#define LLVMOPT 3


/*
** Useful macros
*/
#define luaJ_getengine(L)       (G(L)->llvmengine)
#define luaJ_setengine(L, e)    (G(L)->llvmengine = (e))
#define luaJ_getfunc(p)         ((p)->llvmfunc)
#define luaJ_setfunc(p, f)      ((p)->llvmfunc = (f))


/* Compiles a function and attach it to the proto */
LUAI_FUNC void luaJ_compile (lua_State *L, Proto *proto);

/* Executes the LLVM function */
LUAI_FUNC void luaJ_exec (lua_State *L, Proto* p);

/* Destroys a Func instance */
LUAI_FUNC void luaJ_freefunc (lua_State *L, luaJ_Func *f);

/* Destroys an Engine instance */
LUAI_FUNC void luaJ_freeengine (lua_State *L, luaJ_Engine *e);

#endif

