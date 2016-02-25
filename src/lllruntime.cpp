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
#include "ldebug.h"
#include "lfunc.h"
#include "lgc.h"
#include "lopcodes.h"
#include "luaconf.h"
#include "lvm.h"
#include "ltable.h"
}

#include "lllruntime.h"

namespace {

#if 0
/* useful debug function: */
static void dumpstack (StkId begin, StkId end) {
  printf("Stack dump:\n");
  for (StkId o = begin; o != end; ++o) {
    printf("  %p", o);
    switch (novariant(ttype(o))) {
      case LUA_TNIL:
        printf("  nil\n"); break;
      case LUA_TBOOLEAN:
        printf("  boolean: %s\n", bvalue(o) ? "true" : "false"); break;
      case LUA_TLIGHTUSERDATA:
        printf("  light user data: %p\n", pvalue(o)); break;
      case LUA_TNUMBER:
        printf("  number: %f\n", nvalue(o)); break;
      case LUA_TSTRING:
        printf("  string: %s\n", getstr(o)); break;
      case LUA_TTABLE:
        printf("  table: %p\n", hvalue(o)); break;
      case LUA_TFUNCTION:
        printf("  function: %p\n", clvalue(o)); break;
      case LUA_TUSERDATA:
        printf("  heavy user data: %p\n", uvalue(o)); break;
      case LUA_TTHREAD:
        printf("  thread: %p\n", thvalue(o)); break;
      default:
        printf("  Unknown type: %d\n", ttype(o)); break;
    }
  }
}
#endif

void LLLGetTable(lua_State* L, TValue* t, TValue* k, TValue* v) {
    const TValue *aux;
    if (luaV_fastget(L, t, k, aux, luaH_get)) {
        setobj2s(L, v, aux);
    } else {
        luaV_finishget(L, t, k, v, aux);
    }
}

void LLLSetTable(lua_State* L, TValue* t, TValue* k, TValue* v) {
    const TValue *slot;
    if (!luaV_fastset(L, t, k, slot, luaH_get, v))
        luaV_finishset(L, t, k, v, slot);
}

void LLLSelf(lua_State* L, TValue* ra, TValue* rb, TValue* rc) {
    const TValue *aux;
    TString *key = tsvalue(rc);
    setobjs2s(L, ra + 1, rb);
    if (luaV_fastget(L, rb, key, aux, luaH_getstr)) {
        setobj2s(L, ra, aux);
    } else {
        luaV_finishget(L, rb, rc, ra, aux);
    }
}

int LLLToNumber(const TValue* obj, lua_Number* n) {
    TValue v;
    if (ttisnumber(obj)) {
        *n = nvalue(obj);
        return 1;
    } else if (cvt2num(obj) &&
               luaO_str2num(svalue(obj), &v) == vslen(obj) + 1) {
        *n = nvalue(&v);
        return 1;
    } else {
        return 0;
    }
}

}

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
    luaC_condGC(L, L->top = (c), L->top = ci->top);
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

// Returs whether it is to go back
static int lll_forloop(TValue* ra) {
    int goback = 0;
    if (ttisinteger(ra)) {  /* integer loop? */
        lua_Integer step = ivalue(ra + 2);
        lua_Integer idx = ivalue(ra) + step; /* increment index */
        lua_Integer limit = ivalue(ra + 1);
        if ((0 < step) ? (idx <= limit) : (limit <= idx)) {
            goback = 1;  /* jump back */
            chgivalue(ra, idx);  /* update internal index... */
            setivalue(ra + 3, idx);  /* ...and external index */
        }
    }
    else {  /* floating loop */
        lua_Number step = fltvalue(ra + 2);
        lua_Number idx = luai_numadd(L, fltvalue(ra), step); /* inc. index */
        lua_Number limit = fltvalue(ra + 1);
        if (luai_numlt(0, step) ? luai_numle(idx, limit)
                                : luai_numle(limit, idx)) {
            goback = 1;  /* jump back */
            chgfltvalue(ra, idx);  /* update internal index... */
            setfltvalue(ra + 3, idx);  /* ...and external index */
        }
    }
    return goback;
}

static int forlimit (const TValue *obj, lua_Integer *p, lua_Integer step,
                     int *stopnow) {
  *stopnow = 0;  /* usually, let loops run */
  if (!luaV_tointeger(obj, p, (step < 0 ? 2 : 1))) {  /* not fit in integer? */
    lua_Number n;  /* try to convert to float */
    if (!tonumber(obj, &n)) /* cannot convert to float? */
      return 0;  /* not a number */
    if (luai_numlt(0, n)) {  /* if true, float is larger than max integer */
      *p = LUA_MAXINTEGER;
      if (step < 0) *stopnow = 1;
    }
    else {  /* float is smaller than min integer */
      *p = LUA_MININTEGER;
      if (step >= 0) *stopnow = 1;
    }
  }
  return 1;
}

static void lll_forprep (lua_State *L, TValue *ra)
{
    TValue *init = ra;
    TValue *plimit = ra + 1;
    TValue *pstep = ra + 2;
    lua_Integer ilimit;
    int stopnow;
    if (ttisinteger(init) && ttisinteger(pstep) &&
        forlimit(plimit, &ilimit, ivalue(pstep), &stopnow)) {
      /* all values are integer */
      lua_Integer initv = (stopnow ? 0 : ivalue(init));
      setivalue(plimit, ilimit);
      setivalue(init, initv - ivalue(pstep));
    }
    else {  /* try making all values floats */
      lua_Number ninit; lua_Number nlimit; lua_Number nstep;
      if (!tonumber(plimit, &nlimit))
        luaG_runerror(L, "'for' limit must be a number");
      setfltvalue(plimit, nlimit);
      if (!tonumber(pstep, &nstep))
        luaG_runerror(L, "'for' step must be a number");
      setfltvalue(pstep, nstep);
      if (!tonumber(init, &ninit))
        luaG_runerror(L, "'for' initial value must be a number");
      setfltvalue(init, luai_numsub(L, ninit, nstep));
    }
}

static void lll_setlist(lua_State* L, TValue* ra, int fields, int n)
{
    Table* h = hvalue(ra);
    unsigned int last = fields + n;
    if (last > h->sizearray)
        luaH_resizearray(L, h, last);
    for (; n > 0; n--) {
        TValue* val = ra + n;
        luaH_setint(L, h, last--, val);
        luaC_barrierback(L, h, val);
    }
}

static LClosure *getcached (Proto *p, UpVal **encup, StkId base) {
  LClosure *c = p->cache;
  if (c != NULL) {  /* is there a cached closure? */
    int nup = p->sizeupvalues;
    Upvaldesc *uv = p->upvalues;
    int i;
    for (i = 0; i < nup; i++) {  /* check whether it has right upvalues */
      TValue *v = uv[i].instack ? base + uv[i].idx : encup[uv[i].idx]->v;
      if (c->upvals[i]->v != v)
        return NULL;  /* wrong upvalue; cannot reuse closure */
    }
  }
  return c;  /* return cached closure (or NULL if no cached closure) */
}

static void pushclosure (lua_State *L, Proto *p, UpVal **encup, StkId base,
                         StkId ra) {
  int nup = p->sizeupvalues;
  Upvaldesc *uv = p->upvalues;
  int i;
  LClosure *ncl = luaF_newLclosure(L, nup);
  ncl->p = p;
  setclLvalue(L, ra, ncl);  /* anchor new closure in stack */
  for (i = 0; i < nup; i++) {  /* fill in its upvalues */
    if (uv[i].instack)  /* upvalue refers to local variable? */
      ncl->upvals[i] = luaF_findupval(L, base + uv[i].idx);
    else  /* get upvalue from enclosing function */
      ncl->upvals[i] = encup[uv[i].idx];
    ncl->upvals[i]->refcount++;
    /* new closure is white, so we do not need a barrier here */
  }
  if (!isblack(p))  /* cache will not break GC invariant? */
    p->cache = ncl;  /* save it on cache for reuse */
}

static void lll_closure(lua_State* L, LClosure* cl, TValue* base, TValue* ra,
        int bx) {
    Proto *p = cl->p->p[bx];
    LClosure *ncl = getcached(p, cl->upvals, base);  /* cached closure */
    if (ncl == NULL)  /* no match? */
        pushclosure(L, p, cl->upvals, base, ra);  /* create a new one */
    else
        setclLvalue(L, ra, ncl);  /* push cashed closure */
}

static void lll_checkstack(lua_State* L, int n) {
    luaD_checkstack(L, n);
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

llvm::Type* Runtime::MakeIntT(int nbytes) {
    return llvm::IntegerType::get(context_, 8 * nbytes);
}

void Runtime::InitTypes() {
    #define ADDTYPE(T) AddStructType(#T, sizeof(T))

    ADDTYPE(lua_State);
    ADDTYPE(CallInfo);
    ADDTYPE(TValue);
    ADDTYPE(LClosure);
    ADDTYPE(Proto);
    ADDTYPE(UpVal);
    ADDTYPE(Table);

    types_["lua_Integer"] = MakeIntT(sizeof(lua_Integer));

    #if LUA_FLOAT_TYPE == LUA_FLOAT_FLOAT
    types_["lua_Number"] = llvm::Type::getFloatTy(context_);
    #elif LUA_FLOAT_TYPE == LUA_FLOAT_LONGDOUBLE
    types_["lua_Number"] = llvm::Type::getFP128Ty(context_);
    #elif LUA_FLOAT_TYPE == LUA_FLOAT_DOUBLE
    types_["lua_Number"] = llvm::Type::getDoubleTy(context_);
    #endif
}

void Runtime::InitFunctions() {
    #define ADDFUNCTION(function, ret, ...) { \
        auto type = llvm::FunctionType::get(ret, {__VA_ARGS__}, false); \
        AddFunction(#function, type, reinterpret_cast<void*>(function)); }

    #define LOADBINOP(function) \
        ADDFUNCTION(function, tvoid, tstate, tvalue, tvalue, tvalue)

    #define LOADUNOP(function) \
        ADDFUNCTION(function, tvoid, tstate, tvalue, tvalue)

    auto tstate = types_["lua_State"];
    auto tclosure = types_["LClosure"];
    auto tvalue = types_["TValue"];
    auto tci = types_["CallInfo"];
    auto ttable = types_["Table"];
    auto tupval = types_["UpVal"];
    auto tluanumber = types_["lua_Number"];
    auto tluanumberptr = llvm::PointerType::get(tluanumber, 0);
    auto tvoid = llvm::Type::getVoidTy(context_);
    auto tint = MakeIntT(sizeof(int));

    ADDFUNCTION(LLLGetTable, tvoid, tstate, tvalue, tvalue, tvalue);
    ADDFUNCTION(LLLSetTable, tvoid, tstate, tvalue, tvalue, tvalue);
    ADDFUNCTION(LLLSelf, tvoid, tstate, tvalue, tvalue, tvalue);
    ADDFUNCTION(LLLToNumber, tint, tvalue, tluanumberptr);

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

    ADDFUNCTION(lll_test, tint, tint, tvalue);
    ADDFUNCTION(lll_checkcg, tvoid, tstate, tci, tvalue);
    ADDFUNCTION(lll_newtable, ttable, tstate, tvalue);
    ADDFUNCTION(lll_upvalbarrier, tvoid, tstate, tupval);
    ADDFUNCTION(lll_forloop, tint, tvalue);
    ADDFUNCTION(lll_forprep, tvoid, tstate, tvalue);
    ADDFUNCTION(lll_setlist, tvoid, tstate, tvalue, tint, tint);
    ADDFUNCTION(lll_closure, tvoid, tstate, tclosure, tvalue, tvalue, tint);
    ADDFUNCTION(lll_checkstack, tvoid, tstate, tint);
    ADDFUNCTION(luaH_resize, tvoid, tstate, ttable, tint, tint);
    LOADUNOP(luaV_objlen);
    ADDFUNCTION(luaV_concat, tvoid, tstate, tint);
    ADDFUNCTION(luaV_equalobj, tint, tstate, tvalue, tvalue);
    ADDFUNCTION(luaV_lessthan, tint, tstate, tvalue, tvalue);
    ADDFUNCTION(luaV_lessequal, tint, tstate, tvalue, tvalue);
    ADDFUNCTION(luaD_callnoyield, tvoid, tstate, tvalue, tint);
    ADDFUNCTION(luaF_close, tvoid, tstate, tvalue);
    ADDFUNCTION(luaT_trybinTM, tvoid, tstate, tvalue, tvalue, tvalue, tint);
}

void Runtime::AddStructType(const std::string& name, size_t size) {
    auto memt = {MakeIntT(size)};
    auto structt = llvm::StructType::create(memt, name);
    types_[name] = llvm::PointerType::get(structt, 0);
}

void Runtime::AddFunction(const std::string& name, llvm::FunctionType* type,
                          void* address) {
    functions_[name] = type;
    llvm::sys::DynamicLibrary::AddSymbol(name, address);
}

}

