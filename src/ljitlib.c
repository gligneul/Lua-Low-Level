/*
** LLL - Lua Low-Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllvmjit.h
**
** ljitlib.c
** This is the runtime lib for LLVM JIT API
*/

#define ljitlib_c
#define LUA_LIB

#include "lprefix.h"

#include "lua.h"

#include "lauxlib.h"
#include "lstate.h"
#include "lllvmjit.h"
#include "lobject.h"
#include "luaconf.h"
#include "lualib.h"

static Proto *checkluafunc(lua_State *L, int n) {
  luaL_checktype(L, n, LUA_TFUNCTION);

  TValue *f = L->ci->func + n;
  if (!ttisclosure(f))
    luaL_error(L, "must be a Lua function");
  return getproto(f);
}

/*
** Calls the luaJ_compile function
*/
static int jit_compile (lua_State *L) {
  luaJ_compile(L, checkluafunc(L, 1));
  return 0;
}

/*
** Dumps the module in stderr
*/
static int jit_dump (lua_State *L) {
  Proto *p = checkluafunc(L, 1);
  LLVMDumpModule(luaJ_getfunc(p)->module);
  return 0;
}

/*
** Executes the llvm function
*/
static int jit_exec (lua_State *L) {
  luaJ_exec(L, checkluafunc(L, 1));
  return 0;
}

/*
** functions for 'jit' library
*/
static const luaL_Reg jitlib[] = {
  {"compile", jit_compile},
  {"dump", jit_dump},
  {"exec", jit_exec},
  {NULL, NULL}
};

LUAMOD_API int luaopen_jit (lua_State *L) {
  luaL_newlib(L, jitlib);
  return 1;
}

