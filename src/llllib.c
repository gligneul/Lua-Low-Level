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

#define getfunction(L, n) *(LLLEngine **)luaL_checkudata(L, n, "lll.Function")



/* API functions */

static int compile(lua_State *L) {
    char *error = NULL;
    LLLEngine* engine = LLLCompile(L, getclosure(L, 1), &error);
    if (!engine) {
        fputs(error, stderr);
        free(error);
        luaL_error(L, "Compilation error");
    }
    luaL_getmetatable(L, "lll.Function");
    lua_setmetatable(L, -2); 
    return 1;
}

static int call(lua_State *L) {
    return LLLCall(L, getfunction(L, 1));
}

static int gc(lua_State *L) {
    LLLFreeEngine(L, getfunction(L, 1));
    return 0;
}

static int dump(lua_State *L) {
    LLLDump(getfunction(L, 1));
    return 0;
}

static const luaL_Reg lib_f[] = {
    {"compile", compile},
    {NULL, NULL}
};

static const luaL_Reg lib_m[] = {
    {"__call", call},
    {"__gc", gc},
    {"dump", dump},
    {NULL, NULL}
};

LUAMOD_API int luaopen_lll (lua_State *L) {
    luaL_newmetatable(L, "lll.Function");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, lib_m, 0);
    luaL_newlib(L, lib_f);
    return 1;
}

