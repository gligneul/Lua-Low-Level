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

#include "lllcompiler.h"
#include "lllengine.h"

extern "C" {
#include "lmem.h"
#include "lllcore.h"
}

#define GETENGINE(e) static_cast<lll::Engine *>(e->data)

static void precall (lua_State *L, LLLEngine *e) {
    Proto *proto = e->lclosure->p;
    StkId args = L->ci->func + 1;
    int in = L->top - args - 1;
    int expected = proto->numparams;

    // Remove first arg (userdata)
    for (int i = 0; i < in; ++i)
        setobj2s(L, args + i, args + i + 1);

    // Fill missing args with nil
    for (int i = in; i < expected; ++i)
        setnilvalue(args + i);
}

LLLEngine *LLLCompile (lua_State *L, LClosure *lclosure, char **error_message) {
    lll::Compiler compiler(L, lclosure);
    if (!compiler.Compile()) {
        auto& err = compiler.GetErrorMessage();
        *error_message = luaM_newvector(L, err.size(), char);
        strcpy(*error_message, err.c_str());
        return NULL;
    }
    auto engine = compiler.GetEngine();
    LLLEngine *e = luaM_new(L, LLLEngine);
    e->function = reinterpret_cast<LLLFunction>(engine->GetFunction());
    e->lclosure = lclosure;
    e->data = engine;
    return e;
}

void LLLFreeEngine (lua_State *L, LLLEngine *e) {
    delete GETENGINE(e);
    luaM_free(L, e);
}

int LLLCall (lua_State *L, LLLEngine *e) {
    precall(L, e);
    return e->function(L, e->lclosure);
}

void LLLDump (LLLEngine *e) {
    GETENGINE(e)->Dump();
}

