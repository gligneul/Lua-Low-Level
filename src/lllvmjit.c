/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllvmjit.h
**/

#define lllvmjit_c

#include "lprefix.h"

#include "lllvmjit.h"
#include "luaconf.h"
#include "lopcodes.h"
#include "lmem.h"

#include <stdio.h>
#include <stdlib.h>

static luaJ_Engine *createengine (lua_State *L) {
  luaJ_Engine *e = luaM_new(L, luaJ_Engine);
  e->mod = LLVMModuleCreateWithName("main");
  char *error = NULL;
  if (!LLVMCreateExecutionEngineForModule(&e->engine, e->mod, &error)) {
    // TODO better error treatment
    fputs(error, stderr);
    exit(1);
  }
  return e;
}

static void compilefunc (lua_State *L, Proto *f) {
  LLVMModuleRef mod = LLVMModuleCreateWithName("func");
  f->llvmfunc->mod = mod;

  LLVMTypeRef type = LLVMFunctionType(LLVMInt32Type(), NULL, 0, 0);
}

LUAI_FUNC void luaJ_compile (lua_State *L, Proto *f) {
  if (G(L)->llvmengine == NULL)
    G(L)->llvmengine = createengine(L);
  lua_assert(f->llvmfunc == NULL);
  f->llvmfunc = luaM_new(L, luaJ_Func);
  compilefunc(L, f);
}

LUAI_FUNC void luaJ_freefunc (lua_State *L, luaJ_Func* f) {
  if (f == NULL) return;
  LLVMModuleRef mod;
  LLVMRemoveModule(G(L)->llvmengine->engine, f->mod, &mod, NULL);
  LLVMDisposeModule(mod);
  luaM_free(L, f);
}

LUAI_FUNC void luaJ_freeengine (lua_State *L, luaJ_Engine *e) {
  if (e == NULL) return;
  LLVMDisposeExecutionEngine(e->engine);
  luaM_free(L, e);
}

