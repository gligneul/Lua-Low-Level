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
    int err = LLLCompileAll(L, getclosure(L)->p, &errmsg);
    lua_pushboolean(L, !err);
    if (err) {
        lua_pushstring(L, errmsg);
        luaM_freearray(L, errmsg, strlen(errmsg) + 1);
        return 2;
    } else {
        return 1;
    }
}

static int lll_setautocompileenable (lua_State *L) {
    luaL_checktype(L, 1, LUA_TBOOLEAN);
    LLLSetAutoCompileEnable(lua_toboolean(L, 1));
    return 0;
}

static int lll_isautocompileenable (lua_State *L) {
    lua_pushboolean(L, LLLIsAutoCompileEnable());
    return 1;
}

static int lll_setcallstocompile (lua_State *L) {
    luaL_checktype(L, 1, LUA_TNUMBER);
    LLLSetCallsToCompile(lua_tointeger(L, 1));
    return 0;
}

static int lll_getcallstocompile (lua_State *L) {
    lua_pushinteger(L, LLLGetCallsToCompile());
    return 1;
}

static int lll_iscompiled (lua_State *L) {
    lua_pushboolean(L, LLLIsCompiled(getclosure(L)->p));
    return 1;
}

static int lll_dump (lua_State *L) {
    (void)L;
    LLLDump(getclosure(L)->p);
    return 0;
}

static int lll_write (lua_State *L) {
    Proto *p = getclosure(L)->p;
    const char *path = lua_tolstring(L, 2, NULL);
    if (!path) path = "f";
    LLLWrite(p, path);
    return 0;
}

static const luaL_Reg lib_f[] = {
    {"compile", lll_compile},
    {"setAutoCompileEnable", lll_setautocompileenable},
    {"isAutoCompileEnable", lll_isautocompileenable},
    {"setCallsToCompile", lll_setcallstocompile},
    {"getCallsToCompile", lll_getcallstocompile},
    {"isCompiled", lll_iscompiled},
    {"dump", lll_dump},
    {"write", lll_write},
    {NULL, NULL}
};

LUAMOD_API int luaopen_lll (lua_State *L) {
    luaL_newlib(L, lib_f);
    return 1;
}

