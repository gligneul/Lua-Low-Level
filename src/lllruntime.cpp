/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** runtime.cpp
*/

#include <cstdio>

#include <llvm/Support/DynamicLibrary.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

extern "C" {
#include "lprefix.h"
#include "lfunc.h"
#include "lgc.h"
#include "lopcodes.h"
#include "luaconf.h"
#include "lvm.h"
#include "ltable.h"
}

#include "lllruntime.h"

static void lll_addrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc) {
  lua_Number nb, nc;
  if (ttisinteger(rb) && ttisinteger(rc)) {
    lua_Integer ib = ivalue(rb);
    lua_Integer ic = ivalue(rc);
    setivalue(ra, intop(+, ib, ic));
  } else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
    setfltvalue(ra, luai_numadd(L, nb, nc));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_ADD);
  }
}

static void lll_subrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc) {
  lua_Number nb, nc;
  if (ttisinteger(rb) && ttisinteger(rc)) {
    lua_Integer ib = ivalue(rb);
    lua_Integer ic = ivalue(rc);
    setivalue(ra, intop(-, ib, ic));
  } else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
    setfltvalue(ra, luai_numsub(L, nb, nc));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_SUB);
  }
}

static void lll_mulrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc) {
  lua_Number nb, nc;
  if (ttisinteger(rb) && ttisinteger(rc)) {
    lua_Integer ib = ivalue(rb);
    lua_Integer ic = ivalue(rc);
    setivalue(ra, intop(*, ib, ic));
  } else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
    setfltvalue(ra, luai_nummul(L, nb, nc));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_MUL);
  }
}

static void lll_divrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc) {
  lua_Number nb; lua_Number nc;
  if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
    setfltvalue(ra, luai_numdiv(L, nb, nc));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_DIV);
  }
}

static void lll_bandrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc) {
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, intop(&, ib, ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_BAND);
  }
}

static void lll_borrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc) {
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, intop(|, ib, ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_BOR);
  }
}

static void lll_bxorrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc) {
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, intop(^, ib, ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_BXOR);
  }
}

static void lll_shlrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc) {
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, luaV_shiftl(ib, ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_SHL);
  }
}

static void lll_shrrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc) {
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, luaV_shiftl(ib, -ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_SHR);
  }
}

static void lll_modrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc) {
  lua_Number nb; lua_Number nc;
  if (ttisinteger(rb) && ttisinteger(rc)) {
    lua_Integer ib = ivalue(rb);
    lua_Integer ic = ivalue(rc);
    setivalue(ra, luaV_mod(L, ib, ic));
  } else if (tonumber(rb, &nb) && tonumber(rc, &nc)){
    lua_Number m;
    luai_nummod(L, nb, nc, m);
    setfltvalue(ra, m);
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_MOD);
  }
}

static void lll_idivrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc) {
  lua_Number nb, nc;
  if (ttisinteger(rb) && ttisinteger(rc)) {
    lua_Integer ib = ivalue(rb);
    lua_Integer ic = ivalue(rc);
    setivalue(ra, luaV_div(L, ib, ic));
  } else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
    setfltvalue(ra, luai_numidiv(L, nb, nc));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_IDIV);
  }
}

static void lll_powrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc) {
  lua_Number nb, nc;
  if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
    setfltvalue(ra, luai_numpow(L, nb, nc));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_POW);
  }
}

static void lll_unm (lua_State *L, TValue *ra, TValue *rb) {
  lua_Number nb;
  if (ttisinteger(rb)) {
    lua_Integer ib = ivalue(rb);
    setivalue(ra, intop(-, 0, ib));
  } else if (tonumber(rb, &nb)) {
    setfltvalue(ra, luai_numunm(L, nb));
  } else {
    luaT_trybinTM(L, rb, rb, ra, TM_UNM);
  }
}

static void lll_bnot (lua_State *L, TValue *ra, TValue *rb) {
  lua_Integer ib;
  if (tointeger(rb, &ib)) {
    setivalue(ra, intop(^, ~l_castS2U(0), ib));
  } else {
    luaT_trybinTM(L, rb, rb, ra, TM_BNOT);
  }
}

static void lll_not (lua_State *L, TValue *ra, TValue *rb) {
  (void)L;
  int res = l_isfalse(rb);
  setbvalue(ra, res);
}

static int lll_test (int c, TValue *r) {
  return c ? l_isfalse(r) : !l_isfalse(r);
}

static void lll_checkcg (lua_State *L, CallInfo *ci, TValue *c) {
  // From lvm.c checkGC(L,c)
  luaC_condGC(L, {L->top = (c);
                  luaC_step(L);
                  L->top = ci->top;});
  luai_threadyield(L);
}

static Table *lll_newtable (lua_State *L, TValue *r) {
  Table *t = luaH_new(L);
  sethvalue(L, r, t);
  return t;
}

static void lll_upvalbarrier (lua_State *L, UpVal *uv) {
    luaC_upvalbarrier(L, uv);
}

namespace lll {

Runtime* Runtime::instance_ = nullptr;

Runtime::Runtime() :
    context_(llvm::getGlobalContext()) {
    InitTypes();
    InitFunctions();
}

Runtime* Runtime::Instance() {
    if (!instance_)
        instance_ = new Runtime();
    return instance_;
}

llvm::Type* Runtime::GetType(const std::string& name) {
    assert(types_.find(name) != types_.end() ||
           (fprintf(stderr, "Not found: %s\n", name.c_str()), 0));
    return types_[name];
}

llvm::Function* Runtime::GetFunction(llvm::Module* module,
                                     const std::string& name) {
    assert(functions_.find(name) != functions_.end() ||
           (fprintf(stderr, "Not found: %s\n", name.c_str()), 0));
    auto function = module->getFunction(name);
    if (!function)
        function = llvm::Function::Create(functions_[name],
                llvm::Function::ExternalLinkage, name, module);
    return function;
}

void Runtime::InitTypes() {
    #define ADDTYPE(T) AddType(#T, sizeof(T))

    ADDTYPE(lua_State);
    ADDTYPE(CallInfo);
    ADDTYPE(TValue);
    ADDTYPE(LClosure);
    ADDTYPE(Proto);
    ADDTYPE(UpVal);
    ADDTYPE(Table);
}

void Runtime::InitFunctions() {
    #define ADDFUNCTION(function, ret, ...) { \
        auto type = llvm::FunctionType::get(ret, {__VA_ARGS__}, false); \
        AddFunction(#function, type, reinterpret_cast<void*>(function)); }

    #define LOADBINOP(function) \
        ADDFUNCTION(function, voidt, lstate, tvalue, tvalue, tvalue)

    #define LOADUNOP(function) \
        ADDFUNCTION(function, voidt, lstate, tvalue, tvalue)

    llvm::Type* lstate = types_["lua_State"];
    llvm::Type* tvalue = types_["TValue"];
    llvm::Type* ci = types_["CallInfo"];
    llvm::Type* table = types_["Table"];
    llvm::Type* upval = types_["UpVal"];
    llvm::Type* voidt = llvm::Type::getVoidTy(context_);
    llvm::Type* intt = llvm::IntegerType::get(context_, 8 * sizeof(int));

    LOADBINOP(lll_addrr);
    LOADBINOP(lll_subrr);
    LOADBINOP(lll_mulrr);
    LOADBINOP(lll_divrr);
    LOADBINOP(lll_bandrr);
    LOADBINOP(lll_borrr);
    LOADBINOP(lll_bxorrr);
    LOADBINOP(lll_shlrr);
    LOADBINOP(lll_shrrr);
    LOADBINOP(lll_modrr);
    LOADBINOP(lll_idivrr);
    LOADBINOP(lll_powrr);
    LOADUNOP(lll_unm);
    LOADUNOP(lll_bnot);
    LOADUNOP(lll_not);
    ADDFUNCTION(lll_test, intt, intt, tvalue);
    ADDFUNCTION(lll_checkcg, voidt, lstate, ci, tvalue);
    ADDFUNCTION(lll_newtable, table, lstate, tvalue);
    ADDFUNCTION(lll_upvalbarrier, voidt, lstate, upval);
    ADDFUNCTION(luaH_resize, voidt, lstate, table, intt, intt);
    ADDFUNCTION(luaV_gettable, voidt, lstate, tvalue, tvalue, tvalue);
    ADDFUNCTION(luaV_settable, voidt, lstate, tvalue, tvalue, tvalue);
    LOADUNOP(luaV_objlen);
    ADDFUNCTION(luaV_concat, voidt, lstate, intt);
    ADDFUNCTION(luaV_equalobj, intt, lstate, tvalue, tvalue);
    ADDFUNCTION(luaV_lessthan, intt, lstate, tvalue, tvalue);
    ADDFUNCTION(luaV_lessequal, intt, lstate, tvalue, tvalue);
    ADDFUNCTION(luaD_call, voidt, lstate, tvalue, intt, intt);
}

void Runtime::AddType(const std::string& name, size_t size) {
    std::vector<llvm::Type*> elements =
            {llvm::IntegerType::get(context_, 8 * size)};
    types_[name] = llvm::PointerType::get(
            llvm::StructType::create(elements, name), 0);
}

void Runtime::AddFunction(const std::string& name, llvm::FunctionType* type,
                          void* address) {
    functions_[name] = type;
    llvm::sys::DynamicLibrary::AddSymbol(name, address);
}

}

