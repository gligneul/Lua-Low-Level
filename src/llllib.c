/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** llllib.c
** This is the Lua lib for LLL API
*/

#define llllib_c
#define LUA_LIB

#include <stdlib.h>

#include "lllcore.h"

#include "lauxlib.h"
#include "lobject.h"
#include "lprefix.h"
#include "lstate.h"
#include "lua.h"
#include "lualib.h"



/* Auxiliary functions */

static LClosure* getclosure(lua_State *L, int n) {
    luaL_checktype(L, n, LUA_TFUNCTION);
    StkId o = L->ci->func + n;
    if (!ttisLclosure(o))
        luaL_error(L, "#1 must be a Lua function");
    return clLvalue(o);
}

#define FUNCTION_METATABLE "lll.Function"

#define getfunction(L) *(LLLEngine **)luaL_checkudata(L, 1, FUNCTION_METATABLE)



/* API functions */

static int lll_compile(lua_State *L) {
    LLLEngine **e = (LLLEngine **)lua_newuserdata(L, sizeof(LLLEngine *));
    const char *err = NULL;
    if (!(*e = LLLCompile(L, getclosure(L, 1), &err)))
        luaL_error(L, err);
    luaL_getmetatable(L, FUNCTION_METATABLE);
    lua_setmetatable(L, -2); 
    return 1;
}

static int lll_call(lua_State *L) {
    return LLLCall(L, getfunction(L));
}

static int lll_gc(lua_State *L) {
    LLLFreeEngine(L, getfunction(L));
    return 0;
}

static int lll_dump(lua_State *L) {
    LLLDump(getfunction(L));
    return 0;
}

static const luaL_Reg lib_f[] = {
    {"compile", lll_compile},
    {NULL, NULL}
};

static const luaL_Reg lib_m[] = {
    {"__call", lll_call},
    {"__gc", lll_gc},
    {"dump", lll_dump},
    {NULL, NULL}
};

LUAMOD_API int luaopen_lll (lua_State *L) {
    luaL_newmetatable(L, FUNCTION_METATABLE);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, lib_m, 0);
    luaL_newlib(L, lib_f);
    return 1;
}

