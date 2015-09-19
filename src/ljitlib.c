/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllvmjit.h
**
** ljitlib.c
** This is the runtime lib for LLVM JIT API
**/

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

/*
** Calls the luaJ_compile function
*/
static int jit_compile (lua_State *L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);

  TValue *f = L->ci->func + 1;
  if (!ttisclosure(f))
    luaL_error(L, "must be a Lua function");

  luaJ_compile(L, getproto(f));
  return 0;
}

/*
** functions for 'jit' library
*/
static const luaL_Reg jitlib[] = {
  {"compile", jit_compile},
  {NULL, NULL}
};

LUAMOD_API int luaopen_jit (lua_State *L) {
  luaL_newlib(L, jitlib);
  return 1;
}

