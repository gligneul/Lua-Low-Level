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
#include <string.h>

#include "lllcore.h"

#include "lauxlib.h"
#include "lobject.h"
#include "lprefix.h"
#include "lstate.h"
#include "lua.h"
#include "lualib.h"
#include "lmem.h"

static LClosure *getclosure (lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    StkId o = L->ci->func + 1;
    if (!ttisLclosure(o))
        luaL_error(L, "#1 must be a Lua function");
    return clLvalue(o);
}

static int lll_compile (lua_State *L) {
    char *errmsg = NULL;
    int err = LLLCompile(L, getclosure(L), &errmsg);
    lua_pushboolean(L, !err);
    if (err) {
        lua_pushstring(L, errmsg);
        luaM_freearray(L, errmsg, strlen(errmsg) + 1);
        return 2;
    } else {
        return 1;
    }
}

static int lll_dump (lua_State *L) {
    (void)L;
    LLLDump(getclosure(L));
    return 0;
}

static const luaL_Reg lib_f[] = {
    {"compile", lll_compile},
    {"dump", lll_dump},
    {NULL, NULL}
};

LUAMOD_API int luaopen_lll (lua_State *L) {
    luaL_newlib(L, lib_f);
    return 1;
}

