/*
** LLL - Lua Low-Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllvmjit.h
*/

#define lllvmjit_c

#include "lprefix.h"

#include "lllvmjit.h"
#include "luaconf.h"
#include "lopcodes.h"
#include "lmem.h"

#include <stdio.h>
#include <stdlib.h>
#include <llvm-c/Analysis.h>


/*
** Macros and functions for creating LLVM types
*/

#define maket_struct(typename) \
          (LLVMStructCreateNamed(LLVMGetGlobalContext(), (typename)))
#define maket_ptr(type) (LLVMPointerType((type), 0))
#define maket_str() (maket_ptr(LLVMInt8Type()))

static void maket_tvalue (luaJ_Engine *e) {
  LLVMTypeRef value = maket_struct("TValue");
  e->t_tvalue = maket_ptr(value);
}

static void maket_callinfo (luaJ_Engine *e) {
  LLVMTypeRef callinfo =  maket_struct("CallInfo");
  e->t_callinfo = maket_ptr(callinfo);
}

static void maket_state (luaJ_Engine *e) {
  LLVMTypeRef state = maket_struct("lua_State");
  e->t_state = maket_ptr(state);

  LLVMTypeRef elements[] = {
    maket_str()
  };

  LLVMStructSetBody(state, elements, 1, 0);
}


/*
** Macros and functions for creating LLVM values
*/

#define makev_int(n) (LLVMConstInt(LLVMInt32Type(), (n), 1))


/*
** Compiles the bytecode into LLVM IR
*/

static void compilefunc (luaJ_Engine *e, luaJ_Func *f) {

  (void)e;

  LLVMTypeRef pf_params[] = {maket_str()};
  LLVMTypeRef pf_type = LLVMFunctionType(LLVMInt32Type(), pf_params, 1, 1);
  LLVMValueRef v_printf = LLVMAddFunction(f->module, "printf", pf_type);

  LLVMBuilderRef builder = LLVMCreateBuilder();

  LLVMValueRef v_state = LLVMGetParam(f->value, 0);

  LLVMBasicBlockRef entry = LLVMAppendBasicBlock(f->value, "entry");

  LLVMPositionBuilderAtEnd(builder, entry);
  LLVMValueRef indices[] = {makev_int(0), makev_int(0)};
  LLVMValueRef v1 = LLVMBuildGEP(builder, v_state, indices, 2, "");
  LLVMValueRef v2 = LLVMBuildLoad(builder, v1, "");
  LLVMValueRef params[] = {v2};
  LLVMBuildCall(builder, v_printf, params, 1, "");
  LLVMBuildRet(builder, makev_int(0));

  LLVMDisposeBuilder(builder);

  char *error = NULL;
  LLVMVerifyModule(f->module, LLVMPrintMessageAction, &error);
  LLVMDisposeMessage(error);
}


/*
** Create the structures
*/

static void createengine (lua_State *L) {
  luaJ_Engine *e = luaM_new(L, luaJ_Engine);
  luaJ_setengine(L, e);

  e->module = LLVMModuleCreateWithName("main");

  LLVMLinkInJIT();
  LLVMInitializeNativeTarget();
  char *error = NULL;
  if (LLVMCreateExecutionEngineForModule(&e->engine, e->module, &error)) {
    // TODO better error treatment
    fputs(error, stderr);
    exit(1);
  }

  maket_tvalue(e);
  maket_callinfo(e);
  maket_state(e);
}

static void createfunc (lua_State *L, Proto *p) {
  lua_assert(!luaJ_getfunc(p));
  luaJ_Func *f = luaM_new(L, luaJ_Func);
  luaJ_setfunc(p, f);

  // TODO change name to Proto's address
  f->module = LLVMModuleCreateWithName("func");

  luaJ_Engine *e = luaJ_getengine(L);
  LLVMTypeRef paramtypes[] = {e->t_state};
  LLVMTypeRef functype = LLVMFunctionType(LLVMInt32Type(), paramtypes, 1, 0);
  f->value = LLVMAddFunction(f->module, "", functype);

  compilefunc(e, f);

  LLVMAddModule(e->engine, f->module);
}


/*
** API implementation
*/

LUAI_FUNC void luaJ_compile (lua_State *L, Proto *p) {
  if (!luaJ_getengine(L))
    createengine(L);
  createfunc(L, p);
}

LUAI_FUNC void luaJ_exec (lua_State *L, Proto* p) {
  LLVMExecutionEngineRef engine = luaJ_getengine(L)->engine;
  LLVMValueRef function = luaJ_getfunc(p)->value;

  struct Test {
    char *s;
  } test = {"Hello JIT!\n"};
  LLVMGenericValueRef args[] = {
    LLVMCreateGenericValueOfPointer(&test)
  };
  LLVMRunFunction(engine, function, 1, args);
}

LUAI_FUNC void luaJ_freefunc (lua_State *L, luaJ_Func* f) {
  if (f == NULL) return;
  LLVMModuleRef module;
  LLVMRemoveModule(G(L)->llvmengine->engine, f->module, &module, NULL);
  LLVMDisposeModule(module);
  luaM_free(L, f);
}

LUAI_FUNC void luaJ_freeengine (lua_State *L, luaJ_Engine *e) {
  if (e == NULL) return;
  LLVMDisposeExecutionEngine(e->engine);
  luaM_free(L, e);
}

