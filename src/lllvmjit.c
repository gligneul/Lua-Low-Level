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

#include <stdio.h>

LUAI_FUNC void luaJ_compile (lua_State *L, Proto *f) {
    (void)L;
    printf("compiling: %p\n", f);
}

