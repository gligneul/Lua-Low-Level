/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllcore.cpp
** This is the Lua lib for LLL API
*/

#include <cstring>
#include <iostream>

#include "lllcompiler.h"
#include "lllengine.h"

extern "C" {
#include "lprefix.h"
#include "lapi.h"
#include "lauxlib.h"
#include "lmem.h"
#include "lllcore.h"
}

#define GETENGINE(cl) static_cast<lll::Engine *>(cl->llldata)
#define SETENGINE(cl, e) { \
    auto engine = e; \
    cl->llldata = engine; \
    cl->lllfunction = reinterpret_cast<LLLFunction>(engine->GetFunction()); }

void writeerror (lua_State *L, char **outerr, const char *err) {
    if (outerr) {
        *outerr = luaM_newvector(L, strlen(err) + 1, char);
        strcpy(*outerr, err);
    }
}

int LLLCompile (lua_State *L, LClosure *cl, char **errmsg) {
    if (GETENGINE(cl) != NULL) {
        writeerror(L, errmsg, "Function already compiled");
        return 1;
    }

    lll::Compiler compiler(cl);
    if (!compiler.Compile()) {
        writeerror(L, errmsg, compiler.GetErrorMessage().c_str());
        return 1;
    }

    SETENGINE(cl, compiler.GetEngine());
    return 0;
}

void LLLFreeEngine (lua_State *L, LClosure *cl) {
    (void)L;
    delete GETENGINE(cl);
}

void LLLDump (LClosure *cl) {
    GETENGINE(cl)->Dump();
}

