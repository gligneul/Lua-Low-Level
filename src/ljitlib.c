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
#include "lualib.h"

/*
** Auxiliary functions
*/

static StkId getclosure (lua_State *L, int n) {
  luaL_checktype(L, n, LUA_TFUNCTION);
  StkId o = L->ci->func + n;
  if (!ttisLclosure(o))
    luaL_error(L, "#1 must be a Lua function");
  return o;
}

#define getfunction(L, n) \
  *(luaJ_Function **)luaL_checkudata(L, n, "jit.Function")


/*
** API functions
*/

static int jit_compile (lua_State *L) {
  luaJ_Function **f =
    (luaJ_Function **)lua_newuserdata(L, sizeof(luaJ_Function *));
  *f = luaJ_compile(L, getclosure(L, 1));
  luaL_getmetatable(L, "jit.Function");
  lua_setmetatable(L, -2); 
  return 1;
}

static int jit_call (lua_State *L) {
  return luaJ_call(L, getfunction(L, 1));
}

static int jit_gc (lua_State *L) {
  luaJ_freefunction(L, getfunction(L, 1));
  return 0;
}

static int jit_dump (lua_State *L) {
  luaJ_dump(L, getfunction(L, 1));
  return 0;
}

static const luaL_Reg jitlib_f[] = {
  {"compile", jit_compile},
  {NULL, NULL}
};

static const luaL_Reg jitlib_m[] = {
  {"__call", jit_call},
  {"__gc", jit_gc},
  {"dump", jit_dump},
  {NULL, NULL}
};

LUAMOD_API int luaopen_jit (lua_State *L) {
  luaL_newmetatable(L, "jit.Function");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, jitlib_m, 0);
  luaL_newlib(L, jitlib_f);
  return 1;
}

