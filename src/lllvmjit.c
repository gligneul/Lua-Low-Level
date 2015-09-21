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


static LLVMTypeRef createstatetype () {
    return NULL;
}


static LLVMTypeRef createvaluetype () {
    return NULL;
}


static void createengine (lua_State *L) {
  luaJ_Engine *e = luaM_new(L, luaJ_Engine);
  setengine(L, e);

  e->mod = LLVMModuleCreateWithName("main");

  char *error = NULL;
  if (LLVMCreateExecutionEngineForModule(&e->engine, e->mod, &error)) {
    // TODO better error treatment
    fputs(error, stderr);
    exit(1);
  }

  e->statetype = createstatetype();
  e->valuetype = createvaluetype();
}


static void createfunc (lua_State *L, Proto *p) {
  lua_assert(!getfunc(p));
  luaJ_Func *f = luaM_new(L, luaJ_Func);
  setfunc(p, f);

  f->mod = LLVMModuleCreateWithName("func");

  luaJ_Engine *e = getengine(L);
  LLVMTypeRef paramtypes[] = {e->statetype};
  LLVMTypeRef functype = LLVMFunctionType(LLVMInt32Type(), paramtypes, 1, 0);
  f->value = LLVMAddFunction(f->mod, "f", functype);
}


LUAI_FUNC void luaJ_compile (lua_State *L, Proto *f) {
  if (!hasengine(L))
    createengine(L);
  createfunc(L, f);
}


#define returnifnull(o) if ((o) == NULL) return

LUAI_FUNC void luaJ_freefunc (lua_State *L, luaJ_Func* f) {
  returnifnull(f);
  LLVMModuleRef mod;
  LLVMRemoveModule(G(L)->llvmengine->engine, f->mod, &mod, NULL);
  LLVMDisposeModule(mod);
  luaM_free(L, f);
}


LUAI_FUNC void luaJ_freeengine (lua_State *L, luaJ_Engine *e) {
  returnifnull(e);
  LLVMDisposeExecutionEngine(e->engine);
  luaM_free(L, e);
}

