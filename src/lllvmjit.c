/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllvmjit.h
*/

#define lllvmjit_c

#include "lprefix.h"

#include "lfunc.h"
#include "lgc.h"
#include "lllvmjit.h"
#include "lmem.h"
#include "lopcodes.h"
#include "luaconf.h"
#include "lvm.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Type definitions */

struct luaJ_Function
{
  LLVMModuleRef module;             /* Every function have it's own module. */
  LLVMValueRef value;               /* Pointer to the function */
  LClosure *closure;                /* Lua closure */
};

typedef struct luaJ_Jit
{
  LLVMExecutionEngineRef engine;    /* Execution engine for LLVM */
  LLVMModuleRef module;             /* Global module with aux functions */
  LLVMTypeRef state_type;
  LLVMTypeRef closure_type;
  LLVMTypeRef upval_type;
  LLVMTypeRef ci_type;
  LLVMTypeRef value_type;
  struct {                          /* runtime functions types */
    LLVMTypeRef luaJ_addrr;
    LLVMTypeRef luaJ_subrr;
    LLVMTypeRef luaJ_mulrr;
    LLVMTypeRef luaJ_divrr;
    LLVMTypeRef luaJ_bandrr;
    LLVMTypeRef luaJ_borrr;
    LLVMTypeRef luaJ_bxorrr;
    LLVMTypeRef luaJ_shlrr;
    LLVMTypeRef luaJ_shrrr;
    LLVMTypeRef luaJ_modrr;
    LLVMTypeRef luaJ_idivrr;
    LLVMTypeRef luaJ_powrr;
    LLVMTypeRef luaJ_unm;
    LLVMTypeRef luaJ_bnot;
    LLVMTypeRef luaJ_not;
    LLVMTypeRef luaJ_test;
    LLVMTypeRef luaJ_checkcg;
    LLVMTypeRef luaV_gettable;
    LLVMTypeRef luaV_settable;
    LLVMTypeRef luaV_objlen;
    LLVMTypeRef luaV_concat;
    LLVMTypeRef luaV_equalobj;
    LLVMTypeRef luaV_lessthan;
    LLVMTypeRef luaV_lessequal;
    LLVMTypeRef luaD_call;
  } rttypes;
} luaJ_Jit;
static luaJ_Jit *Jit = NULL;

typedef struct luaJ_CompileState
{
  luaJ_Jit *Jit;
  luaJ_Function *f;
  Proto *proto;
  LLVMBuilderRef builder;
  LLVMBasicBlockRef *blocks;    /* one block per instruction */
  LLVMValueRef state;           /* lua_State */
  LLVMValueRef closure;         /* Original lua function */
  LLVMValueRef ci;              /* CallInfo */
  LLVMValueRef func;            /* base = func + 1 */
  int idx;                      /* current block/instruction index */
  Instruction instr;            /* current instruction */
  LLVMBasicBlockRef block;      /* current block */
  struct {                      /* runtime functions */
    LLVMValueRef luaJ_addrr;
    LLVMValueRef luaJ_subrr;
    LLVMValueRef luaJ_mulrr;
    LLVMValueRef luaJ_divrr;
    LLVMValueRef luaJ_bandrr;
    LLVMValueRef luaJ_borrr;
    LLVMValueRef luaJ_bxorrr;
    LLVMValueRef luaJ_shlrr;
    LLVMValueRef luaJ_shrrr;
    LLVMValueRef luaJ_modrr;
    LLVMValueRef luaJ_idivrr;
    LLVMValueRef luaJ_powrr;
    LLVMValueRef luaJ_unm;
    LLVMValueRef luaJ_bnot;
    LLVMValueRef luaJ_not;
    LLVMValueRef luaJ_test;
    LLVMValueRef luaJ_checkcg;
    LLVMValueRef luaV_gettable;
    LLVMValueRef luaV_settable;
    LLVMValueRef luaV_objlen;
    LLVMValueRef luaV_concat;
    LLVMValueRef luaV_equalobj;
    LLVMValueRef luaV_lessthan;
    LLVMValueRef luaV_lessequal;
    LLVMValueRef luaD_call;
  } rt;
} luaJ_CompileState;



/* Macros and functions for creating LLVM types */

#define makeint_t()             LLVMIntType(8 * sizeof(int))
#define makeptr_t(type)         LLVMPointerType((type), 0)
#define makestring_t()          makeptr_t(LLVMInt8Type())
#define makesizeof_t(type)      LLVMIntType(8 * sizeof(type))
#define makestruct_t(strukt)    makenamedstruct_t(#strukt, sizeof(strukt))

static LLVMTypeRef makenamedstruct_t (const char *name, size_t size)
{
  LLVMTypeRef type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);
  LLVMTypeRef fields[] = {LLVMIntType(8 * size)};
  LLVMStructSetBody(type, fields, 1, 0);
  return makeptr_t(type);
}



/* Macros and functions for creating LLVM values */

#define makeint(n)              LLVMConstInt(LLVMInt32Type(), (n), 1)

#define makestring(builder, str) \
  LLVMBuildPointerCast(builder, \
      LLVMBuildGlobalString(builder, str, ""), makestring_t(), "")

/* Converts integer $value to a boolean */
#define tobool(builder, i, name) \
  LLVMBuildICmp(builder, LLVMIntNE, makeint(0), i, name)



/* Struct access */

#define getfieldptr(cs, value, fieldtype, strukt, field) \
  getfieldbyoffset(cs, value, fieldtype, offsetof(strukt, field), #field "_ptr")

#define loadfield(cs, value, fieldtype, strukt, field) \
  LLVMBuildLoad(cs->builder, \
      getfieldptr(cs, value, fieldtype, strukt, field), #field)

LLVMValueRef getfieldbyoffset (luaJ_CompileState *cs, LLVMValueRef value,
                               LLVMTypeRef fieldtype, size_t offset,
                               const char *name)
{
  LLVMValueRef mem = LLVMBuildBitCast(cs->builder, value, makestring_t(), "");
  LLVMValueRef index[] = {makeint(offset)};
  LLVMValueRef element = LLVMBuildGEP(cs->builder, mem, index, 1, "");
  return LLVMBuildBitCast(cs->builder, element, makeptr_t(fieldtype), name);
}



/* Lua stack manipulation */

static LLVMValueRef gettvaluer (luaJ_CompileState *cs, int arg,
        const char* name)
{
  LLVMValueRef indices[] = {makeint(1 + arg)};
  return LLVMBuildGEP(cs->builder, cs->func, indices, 1, name);
}

static LLVMValueRef gettvaluek (luaJ_CompileState *cs, int arg,
        const char *name)
{
  TValue *value = cs->proto->k + arg;
  LLVMValueRef v_intptr = LLVMConstInt(makesizeof_t(TValue *),
    (uintptr_t)value, 0);
  return LLVMBuildIntToPtr(cs->builder, v_intptr, cs->Jit->value_type, name);
}

#define gettvaluerk(cs, arg, name) \
  (ISK(arg) ? gettvaluek(cs, INDEXK(arg), name) : gettvaluer(cs, arg, name))

#define getvaluedata(cs, type, name) \
  LLVMBuildLoad(cs->builder, \
      LLVMBuildBitCast(cs->builder, cs->func, makeptr_t(type), name "_ptr"), \
      name)

static void setregister (LLVMBuilderRef builder, LLVMValueRef reg,
                         LLVMValueRef value)
{
  LLVMValueRef value_iptr =
      LLVMBuildBitCast(builder, value, makeptr_t(makesizeof_t(TValue)), "");
  LLVMValueRef value_i = LLVMBuildLoad(builder, value_iptr, "");
  LLVMValueRef reg_iptr =
      LLVMBuildBitCast(builder, reg, makeptr_t(makesizeof_t(TValue)), "");
  LLVMBuildStore(builder, value_i, reg_iptr);
}

static void settop (luaJ_CompileState *cs, int offset)
{
  LLVMValueRef value = gettvaluer(cs, offset, "topvalue");
  LLVMValueRef top =
        getfieldptr(cs, cs->state, cs->Jit->value_type, lua_State, top);
  LLVMBuildStore(cs->builder, value, top);
}

/* Obtains the $nth upvalue */
static LLVMValueRef getupval (luaJ_CompileState *cs, int n)
{
  LLVMValueRef upvals = loadfield(cs, cs->closure, cs->Jit->upval_type,
      LClosure, upvals);
  LLVMValueRef idx[] = {makeint(n)};
  LLVMValueRef upval = LLVMBuildGEP(cs->builder, upvals, idx, 1, "upval");
  return loadfield(cs, upval, cs->Jit->value_type, UpVal, v);
}



/* Basic block functions */

/* Creates a new subblock, and inserts it after @previousblock. */
static LLVMBasicBlockRef addsubblock (luaJ_CompileState *cs,
        LLVMBasicBlockRef mainblock, LLVMBasicBlockRef previousblock,
        const char* suffix)
{
  const char *basename = LLVMGetValueName(LLVMBasicBlockAsValue(mainblock));
  char newname[strlen(basename) + strlen(suffix) + 2];
  sprintf(newname, "%s.%s", basename, suffix);
  LLVMBasicBlockRef newblock = LLVMAppendBasicBlock(cs->f->value, newname);
  LLVMMoveBasicBlockAfter(newblock, previousblock);
  return newblock;
}



/* Runtime functions */

static void luaJ_addrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
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

static void luaJ_subrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
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

static void luaJ_mulrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
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

static void luaJ_divrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Number nb; lua_Number nc;
  if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
    setfltvalue(ra, luai_numdiv(L, nb, nc));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_DIV);
  }
}

static void luaJ_bandrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, intop(&, ib, ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_BAND);
  }
}

static void luaJ_borrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, intop(|, ib, ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_BOR);
  }
}

static void luaJ_bxorrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, intop(^, ib, ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_BXOR);
  }
}

static void luaJ_shlrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, luaV_shiftl(ib, ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_SHL);
  }
}

static void luaJ_shrrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, luaV_shiftl(ib, -ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_SHR);
  }
}

static void luaJ_modrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
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

static void luaJ_idivrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
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

static void luaJ_powrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Number nb, nc;
  if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
    setfltvalue(ra, luai_numpow(L, nb, nc));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_POW);
  }
}

static void luaJ_unm (lua_State *L, TValue *ra, TValue *rb)
{
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

static void luaJ_bnot (lua_State *L, TValue *ra, TValue *rb)
{
  lua_Integer ib;
  if (tointeger(rb, &ib)) {
    setivalue(ra, intop(^, ~l_castS2U(0), ib));
  } else {
    luaT_trybinTM(L, rb, rb, ra, TM_BNOT);
  }
}

static void luaJ_not (lua_State *L, TValue *ra, TValue *rb)
{
  (void)L;
  int res = l_isfalse(rb);
  setbvalue(ra, res);
}

static int luaJ_test (int c, TValue *r)
{
  return c ? l_isfalse(r) : !l_isfalse(r);
}

static void luaJ_checkcg (lua_State *L, CallInfo *ci, TValue *c)
{
  // From lvm.c checkGC(L,c)
  luaC_condGC(L, {L->top = (c);
                  luaC_step(L);
                  L->top = ci->top;});
  luai_threadyield(L);
}

static void runtime_loadtypes (luaJ_Jit *Jit)
{
  #define rt_loadtype(function, ret, ...) \
    do { \
      LLVMTypeRef params[] = {__VA_ARGS__}; \
      int nparams = sizeof(params) / sizeof(LLVMTypeRef); \
      Jit->rttypes.function = LLVMFunctionType(ret, params, nparams, 0); \
    } while (0)

  #define rt_loadbinop(function) \
    rt_loadtype(function, LLVMVoidType(), lstate, tvalue, tvalue, tvalue)

  #define rt_loadunop(function) \
    rt_loadtype(function, LLVMVoidType(), lstate, tvalue, tvalue)

  LLVMTypeRef lstate = Jit->state_type;
  LLVMTypeRef tvalue = Jit->value_type;
  LLVMTypeRef ci = Jit->ci_type;
  LLVMTypeRef voidt = LLVMVoidType();

  rt_loadbinop(luaJ_addrr);
  rt_loadbinop(luaJ_subrr);
  rt_loadbinop(luaJ_mulrr);
  rt_loadbinop(luaJ_divrr);
  rt_loadbinop(luaJ_bandrr);
  rt_loadbinop(luaJ_borrr);
  rt_loadbinop(luaJ_bxorrr);
  rt_loadbinop(luaJ_shlrr);
  rt_loadbinop(luaJ_shrrr);
  rt_loadbinop(luaJ_modrr);
  rt_loadbinop(luaJ_idivrr);
  rt_loadbinop(luaJ_powrr);
  rt_loadunop(luaJ_unm);
  rt_loadunop(luaJ_bnot);
  rt_loadunop(luaJ_not);
  rt_loadtype(luaJ_test, makeint_t(), makeint_t(), tvalue);
  rt_loadtype(luaJ_checkcg, voidt, lstate, ci, tvalue);
  rt_loadtype(luaV_gettable, voidt, lstate, tvalue, tvalue, tvalue);
  rt_loadtype(luaV_settable, voidt, lstate, tvalue, tvalue, tvalue);
  rt_loadunop(luaV_objlen);
  rt_loadtype(luaV_concat, voidt, lstate, makeint_t());
  rt_loadtype(luaV_equalobj, makeint_t(), lstate, tvalue, tvalue);
  rt_loadtype(luaV_lessthan, makeint_t(), lstate, tvalue, tvalue);
  rt_loadtype(luaV_lessequal, makeint_t(), lstate, tvalue, tvalue);
  rt_loadtype(luaD_call, voidt, lstate, tvalue, makeint_t(), makeint_t());
}

static void runtime_init (luaJ_CompileState *cs)
{
  #define rt_init(function) \
    cs->rt.function = NULL

  rt_init(luaJ_addrr);
  rt_init(luaJ_subrr);
  rt_init(luaJ_mulrr);
  rt_init(luaJ_divrr);
  rt_init(luaJ_bandrr);
  rt_init(luaJ_borrr);
  rt_init(luaJ_bxorrr);
  rt_init(luaJ_shlrr);
  rt_init(luaJ_shrrr);
  rt_init(luaJ_modrr);
  rt_init(luaJ_idivrr);
  rt_init(luaJ_powrr);
  rt_init(luaJ_unm);
  rt_init(luaJ_bnot);
  rt_init(luaJ_not);
  rt_init(luaJ_test);
  rt_init(luaJ_checkcg);
  rt_init(luaV_gettable);
  rt_init(luaV_settable);
  rt_init(luaV_objlen);
  rt_init(luaV_concat);
  rt_init(luaV_equalobj);
  rt_init(luaV_lessthan);
  rt_init(luaV_lessequal);
  rt_init(luaD_call);
}

static void runtime_link (luaJ_CompileState *cs)
{
  #define rt_link(function) \
    if (cs->rt.function) \
      LLVMAddGlobalMapping(cs->Jit->engine, cs->rt.function, function)

  rt_link(luaJ_addrr);
  rt_link(luaJ_subrr);
  rt_link(luaJ_mulrr);
  rt_link(luaJ_divrr);
  rt_link(luaJ_bandrr);
  rt_link(luaJ_borrr);
  rt_link(luaJ_bxorrr);
  rt_link(luaJ_shlrr);
  rt_link(luaJ_shrrr);
  rt_link(luaJ_modrr);
  rt_link(luaJ_idivrr);
  rt_link(luaJ_powrr);
  rt_link(luaJ_unm);
  rt_link(luaJ_bnot);
  rt_link(luaJ_not);
  rt_link(luaJ_test);
  rt_link(luaJ_checkcg);
  rt_link(luaV_gettable);
  rt_link(luaV_settable);
  rt_link(luaV_objlen);
  rt_link(luaV_concat);
  rt_link(luaV_equalobj);
  rt_link(luaV_lessthan);
  rt_link(luaV_lessequal);
  rt_link(luaD_call);
}

#define runtime_call(cs, function) \
  (cs->rt.function ? \
    (cs->rt.function) : \
    (cs->rt.function = LLVMAddFunction(cs->f->module, #function, \
                                       cs->Jit->rttypes.function)))



/* Compiles the bytecode into LLVM IR */

#define updatestack(cs) \
  cs->func = loadfield(cs, cs->ci, cs->Jit->value_type, CallInfo, func)

static void initblocks (luaJ_CompileState *cs)
{
  LLVMBasicBlockRef entry = LLVMAppendBasicBlock(cs->f->value, "bblock.entry");
  LLVMPositionBuilderAtEnd(cs->builder, entry);

  cs->ci = loadfield(cs, cs->state, cs->Jit->ci_type, lua_State, ci);

  int nblocks = cs->proto->sizecode;
  for (int i = 0; i < nblocks; ++i) {
    char name[128];
    const char *instr = luaP_opnames[GET_OPCODE(cs->proto->code[i])];
    sprintf(name, "bblock.%d.%s", i, instr);
    cs->blocks[i] = LLVMAppendBasicBlock(cs->f->value, name);
  }

  LLVMBuildBr(cs->builder, cs->blocks[0]);
}

static void initcs (luaJ_CompileState *cs, luaJ_Jit *Jit, luaJ_Function *f)
{
  cs->Jit = Jit;
  cs->f = f;
  cs->proto = f->closure->p;
  cs->builder = LLVMCreateBuilder();
  cs->blocks = (LLVMBasicBlockRef *)malloc(
        sizeof(LLVMBasicBlockRef) * cs->proto->sizecode);

  cs->state = LLVMGetParam(cs->f->value, 0);
  LLVMSetValueName(cs->state, "L");
  cs->closure = LLVMGetParam(cs->f->value, 1);
  LLVMSetValueName(cs->closure, "lclosure");

  runtime_init(cs);
  initblocks(cs);
}

static void closecs (luaJ_CompileState *cs)
{
  free(cs->blocks);
  LLVMDisposeBuilder(cs->builder);
}

static void compile_checkcg (luaJ_CompileState *cs, LLVMValueRef reg)
{
  LLVMValueRef params[] = {cs->state, cs->ci, reg};
  LLVMBuildCall(cs->builder, runtime_call(cs, luaJ_checkcg), params, 3, "");
  updatestack(cs);
}

static void compile_move (luaJ_CompileState *cs)
{
  updatestack(cs);
  int a = GETARG_A(cs->instr);
  int b = GETARG_B(cs->instr);
  LLVMValueRef v_ra = gettvaluer(cs, a, "ra");
  LLVMValueRef v_rb = gettvaluer(cs, b, "rb");
  setregister(cs->builder, v_ra, v_rb);
}

static void compile_loadk (luaJ_CompileState *cs)
{
  updatestack(cs);
  int kposition = (GET_OPCODE(cs->instr) == OP_LOADK) ?
    GETARG_Bx(cs->instr) : GETARG_Ax(cs->instr + 1);
  LLVMValueRef ra = gettvaluer(cs, GETARG_A(cs->instr), "ra");
  LLVMValueRef kvalue = gettvaluek(cs, kposition, "kvalue");
  setregister(cs->builder, ra, kvalue);
}

static void compile_loadbool (luaJ_CompileState *cs)
{
  updatestack(cs);
  LLVMValueRef ra = gettvaluer(cs, GETARG_A(cs->instr), "ra");
  LLVMValueRef ra_value = getfieldptr(cs, ra, makeint_t(), TValue, value_);
  LLVMBuildStore(cs->builder, makeint(GETARG_B(cs->instr)), ra_value);
  LLVMValueRef ra_tag = getfieldptr(cs, ra, makeint_t(), TValue, tt_);
  LLVMBuildStore(cs->builder, makeint(LUA_TBOOLEAN), ra_tag);

  if (GETARG_C(cs->instr))
    LLVMBuildBr(cs->builder, cs->blocks[cs->idx + 2]);
}

static void compile_loadnil (luaJ_CompileState *cs)
{
  updatestack(cs);
  int start = GETARG_A(cs->instr);
  int end = start + GETARG_B(cs->instr);
  for (int i = start; i <= end; ++i) {
    LLVMValueRef r = gettvaluer(cs, i, "r");
    LLVMValueRef tag = getfieldptr(cs, r, makeint_t(), TValue, tt_);
    LLVMBuildStore(cs->builder, makeint(LUA_TNIL), tag);
  }
}

static void compile_gettabup (luaJ_CompileState *cs)
{
  updatestack(cs);
  LLVMValueRef params[] = {
      cs->state,
      getupval(cs, GETARG_B(cs->instr)),
      gettvaluerk(cs, GETARG_C(cs->instr), "rkc"),
      gettvaluer(cs, GETARG_A(cs->instr), "ra")
  };
  LLVMBuildCall(cs->builder, runtime_call(cs, luaV_gettable), params, 4, "");
}

static void compile_settabup (luaJ_CompileState *cs)
{
  updatestack(cs);
  LLVMValueRef params[] = {
    cs->state,
    getupval(cs, GETARG_A(cs->instr)),
    gettvaluerk(cs, GETARG_B(cs->instr), "rkb"),
    gettvaluerk(cs, GETARG_C(cs->instr), "rkc")
  };
  LLVMBuildCall(cs->builder, runtime_call(cs, luaV_settable), params, 4, "");
}

static void compile_binop (luaJ_CompileState *cs, LLVMValueRef op)
{
  updatestack(cs);
  LLVMValueRef args[] = {
      cs->state,
      gettvaluer(cs, GETARG_A(cs->instr), "ra"),
      gettvaluerk(cs, GETARG_B(cs->instr), "rkb"),
      gettvaluerk(cs, GETARG_C(cs->instr), "rkc")
  };
  LLVMBuildCall(cs->builder, op, args, 4, "");
}

static void compile_unop (luaJ_CompileState *cs, LLVMValueRef op)
{
  updatestack(cs);
  LLVMValueRef args[] = {
      cs->state,
      gettvaluer(cs, GETARG_A(cs->instr), "ra"),
      gettvaluerk(cs, GETARG_B(cs->instr), "rkb"),
  };
  LLVMBuildCall(cs->builder, op, args, 3, "");
}

static void compile_concat (luaJ_CompileState *cs)
{
  int a = GETARG_A(cs->instr);
  int b = GETARG_B(cs->instr);
  int c = GETARG_C(cs->instr);
  updatestack(cs);
  settop(cs, c + 1);
  LLVMValueRef params[] = {cs->state, makeint(c - b + 1)};
  LLVMBuildCall(cs->builder, runtime_call(cs, luaV_concat), params, 2, "");
  updatestack(cs);
  LLVMValueRef ra = gettvaluer(cs, a, "ra");
  LLVMValueRef rb = gettvaluer(cs, b, "rb");
  setregister(cs->builder, ra, rb);
  if (a >= b)
    compile_checkcg(cs, gettvaluer(cs, a + 1, "ra_plus_1"));
  else
    compile_checkcg(cs, rb);
  LLVMValueRef oldtop = loadfield(cs, cs->ci, cs->Jit->value_type, CallInfo,
      top);
  LLVMValueRef topptr = getfieldptr(cs, cs->state, cs->Jit->value_type,
      lua_State, top);
  LLVMBuildStore(cs->builder, oldtop, topptr);
}

static void compile_jmp (luaJ_CompileState *cs)
{
  LLVMBuildBr(cs->builder, cs->blocks[cs->idx + GETARG_sBx(cs->instr) + 1]);
}

static void compile_cmp (luaJ_CompileState *cs)
{
  updatestack(cs);
  LLVMValueRef args[] = {
      cs->state,
      gettvaluerk(cs, GETARG_B(cs->instr), "rkb"),
      gettvaluerk(cs, GETARG_C(cs->instr), "rkc")
  };

  LLVMValueRef function = NULL;
  switch (GET_OPCODE(cs->instr)) {
    case OP_EQ: function = runtime_call(cs, luaV_equalobj); break;
    case OP_LT: function = runtime_call(cs, luaV_lessthan); break;
    case OP_LE: function = runtime_call(cs, luaV_lessequal); break;
    default:    lua_assert(false);
  }
  LLVMValueRef result = LLVMBuildCall(cs->builder, function, args, 3, "");
  LLVMValueRef a = makeint(GETARG_A(cs->instr));
  LLVMValueRef cmp = LLVMBuildICmp(cs->builder, LLVMIntNE, result, a, "");
  LLVMBasicBlockRef next_block = cs->blocks[cs->idx + 2];
  LLVMBasicBlockRef jmp_block = cs->blocks[cs->idx + 1];
  LLVMBuildCondBr(cs->builder, cmp, next_block, jmp_block);
}

static void compile_test (luaJ_CompileState *cs)
{
  updatestack(cs);
  LLVMValueRef function = runtime_call(cs, luaJ_test);
  LLVMValueRef ra = gettvaluer(cs, GETARG_A(cs->instr), "ra");
  LLVMValueRef params[] = {makeint(GETARG_C(cs->instr)), ra};
  LLVMValueRef ret = LLVMBuildCall(cs->builder, function, params, 2, "test");

  LLVMValueRef test = tobool(cs->builder, ret, "test");
  LLVMBasicBlockRef nextblock = cs->blocks[cs->idx + 2];
  LLVMBasicBlockRef jmpblock = cs->blocks[cs->idx + 1];
  LLVMBuildCondBr(cs->builder, test, nextblock, jmpblock);
}

static void compile_testset (luaJ_CompileState *cs)
{
  updatestack(cs);
  LLVMValueRef function = runtime_call(cs, luaJ_test);
  LLVMValueRef rb = gettvaluer(cs, GETARG_B(cs->instr), "rb");
  LLVMValueRef params[] = {makeint(GETARG_C(cs->instr)), rb};
  LLVMValueRef ret = LLVMBuildCall(cs->builder, function, params, 2, "test");

  LLVMValueRef test = tobool(cs->builder, ret, "test");
  LLVMBasicBlockRef setblock = addsubblock(cs, cs->block, cs->block, "set");
  LLVMBasicBlockRef nextblock = cs->blocks[cs->idx + 2];
  LLVMBuildCondBr(cs->builder, test, nextblock, setblock);

  LLVMPositionBuilderAtEnd(cs->builder, setblock);
  LLVMValueRef ra = gettvaluer(cs, GETARG_A(cs->instr), "ra");
  setregister(cs->builder, ra, rb);
  LLVMBuildBr(cs->builder, cs->blocks[cs->idx + 1]);
}

static void compile_call (luaJ_CompileState *cs)
{
  int a = GETARG_A(cs->instr);
  int b = GETARG_B(cs->instr);
  if (b != 0)
    settop(cs, a + b);
  updatestack(cs);
  LLVMValueRef params[] = {
    cs->state,
    gettvaluer(cs, a, "ra"),
    makeint(GETARG_C(cs->instr) - 1),
    makeint(0)
  };
  LLVMBuildCall(cs->builder, runtime_call(cs, luaD_call), params, 4, "");
}

static void compile_return (luaJ_CompileState *cs)
{
  int b = GETARG_B(cs->instr);
  int nresults;
  if (b == 1) {
    nresults = 0;
  } else {
    nresults = b - 1;
    int a = GETARG_A(cs->instr);
    updatestack(cs);
    settop(cs, a + nresults);
  }
  LLVMBuildRet(cs->builder, makeint(nresults));
}

static void compile_opcode (luaJ_CompileState *cs)
{
    switch (GET_OPCODE(cs->instr)) {
      case OP_MOVE:     compile_move(cs); break;
      case OP_LOADK:    compile_loadk(cs); break;
      case OP_LOADKX:   compile_loadk(cs); break;
      case OP_LOADBOOL: compile_loadbool(cs); break;
      case OP_LOADNIL:  compile_loadnil(cs); break;
      case OP_GETUPVAL: /* TODO */ break;
      case OP_GETTABUP: compile_gettabup(cs); break;
      case OP_GETTABLE: /* TODO */ break;
      case OP_SETTABUP: compile_settabup(cs); break;
      case OP_SETUPVAL: /* TODO */ break;
      case OP_SETTABLE: /* TODO */ break;
      case OP_NEWTABLE: /* TODO */ break;
      case OP_SELF:     /* TODO */ break;
      case OP_ADD:      compile_binop(cs, runtime_call(cs, luaJ_addrr)); break;
      case OP_SUB:      compile_binop(cs, runtime_call(cs, luaJ_subrr)); break;
      case OP_MUL:      compile_binop(cs, runtime_call(cs, luaJ_mulrr)); break;
      case OP_MOD:      compile_binop(cs, runtime_call(cs, luaJ_modrr)); break;
      case OP_POW:      compile_binop(cs, runtime_call(cs, luaJ_powrr)); break;
      case OP_DIV:      compile_binop(cs, runtime_call(cs, luaJ_divrr)); break;
      case OP_IDIV:     compile_binop(cs, runtime_call(cs, luaJ_idivrr)); break;
      case OP_BAND:     compile_binop(cs, runtime_call(cs, luaJ_bandrr)); break;
      case OP_BOR:      compile_binop(cs, runtime_call(cs, luaJ_borrr)); break;
      case OP_BXOR:     compile_binop(cs, runtime_call(cs, luaJ_bxorrr)); break;
      case OP_SHL:      compile_binop(cs, runtime_call(cs, luaJ_shlrr)); break;
      case OP_SHR:      compile_binop(cs, runtime_call(cs, luaJ_shrrr)); break;
      case OP_UNM:      compile_unop(cs, runtime_call(cs, luaJ_unm)); break;
      case OP_BNOT:     compile_unop(cs, runtime_call(cs, luaJ_bnot)); break;
      case OP_NOT:      compile_unop(cs, runtime_call(cs, luaJ_not)); break;
      case OP_LEN:      compile_unop(cs, runtime_call(cs, luaV_objlen)); break;
      case OP_CONCAT:   compile_concat(cs); break;
      case OP_JMP:      compile_jmp(cs); break;
      case OP_EQ:       compile_cmp(cs); break;
      case OP_LT:       compile_cmp(cs); break;
      case OP_LE:       compile_cmp(cs); break;
      case OP_TEST:     compile_test(cs); break;
      case OP_TESTSET:  compile_testset(cs); break;
      case OP_CALL:     compile_call(cs); break;
      case OP_TAILCALL: /* TODO */ break;
      case OP_RETURN:   compile_return(cs); break;
      case OP_FORLOOP:  /* TODO */ break;
      case OP_FORPREP:  /* TODO */ break;
      case OP_TFORCALL: /* TODO */ break;
      case OP_TFORLOOP: /* TODO */ break;
      case OP_SETLIST:  /* TODO */ break;
      case OP_CLOSURE:  /* TODO */ break;
      case OP_VARARG:   /* TODO */ break;
      case OP_EXTRAARG: /* ignored */ break;
    }
}

static void compile (luaJ_Jit *Jit, luaJ_Function *f)
{
  luaJ_CompileState cs;
  initcs(&cs, Jit, f);

  for (int i = 0; i < cs.proto->sizecode; ++i) {
    cs.idx = i;
    cs.instr = cs.proto->code[i];
    cs.block = cs.blocks[i];
    LLVMPositionBuilderAtEnd(cs.builder, cs.block);
    compile_opcode(&cs);
    if (LLVMGetBasicBlockTerminator(cs.block) == NULL) {
      LLVMPositionBuilderAtEnd(cs.builder, cs.block);
      LLVMBuildBr(cs.builder, cs.blocks[i + 1]);
    }
  }

  // TODO better error treatment
  char *error = NULL;
  if (LLVMVerifyModule(f->module, LLVMReturnStatusAction, &error)) {
    fprintf(stderr, "\n>>>>> MODULE\n");
    LLVMDumpModule(f->module);
    fprintf(stderr, "\n>>>>> ERROR\n%s\n", error);
    LLVMDisposeMessage(error);
    exit(1);
  }

  LLVMAddModule(Jit->engine, f->module);
  runtime_link(&cs);
  closecs(&cs);
}



/* Other auxiliary functions */

static void initJit (lua_State *L)
{
  Jit = luaM_new(L, luaJ_Jit);
  Jit->module = LLVMModuleCreateWithName("main");
  LLVMLinkInJIT();
  LLVMInitializeNativeTarget();
  char *error = NULL;
  if (LLVMCreateJITCompilerForModule(&Jit->engine, Jit->module, LUAJ_LLVMOPT,
      &error)) {
    // TODO better error treatment
    fputs(error, stderr);
    exit(1);
  }
  Jit->state_type = makestruct_t(lua_State);
  Jit->closure_type = makestruct_t(LClosure);
  Jit->upval_type = makestruct_t(UpVal);
  Jit->ci_type = makestruct_t(CallInfo);
  Jit->value_type = makestruct_t(TValue);
  runtime_loadtypes(Jit);
}

static void precall (lua_State *L, luaJ_Function *f)
{
  Proto *proto = f->closure->p;
  StkId args = L->ci->func + 1;
  int in = L->top - args- 1;
  int expected = proto->numparams;

  // Remove first arg (userdata luaJ_Function)
  for (int i = 0; i < in; ++i)
    setobj2s(L, args + i, args + i + 1);

  // Fill missing args with nil
  for (int i = in; i < expected; ++i)
    setnilvalue(args + i);
}



/* API implementation */

LUAI_FUNC luaJ_Function *luaJ_compile (lua_State *L, StkId closure)
{
  if (!Jit) initJit(L);

  luaJ_Function *f = luaM_new(L, luaJ_Function);

  // Creates the module
  char modulename[16];
  sprintf(modulename, "f%p", closure);
  f->module = LLVMModuleCreateWithName(modulename);

  // Creates the LLVM function
  LLVMTypeRef paramtypes[] = {Jit->state_type, Jit->closure_type};
  LLVMTypeRef functype = LLVMFunctionType(LLVMInt32Type(), paramtypes, 2, 0);
  f->value = LLVMAddFunction(f->module, "", functype);
  f->closure = clLvalue(closure);

  compile(Jit, f);
  return f;
}

LUAI_FUNC int luaJ_call (lua_State *L, luaJ_Function *f)
{
  precall(L, f);
  LLVMGenericValueRef args[] = {
      LLVMCreateGenericValueOfPointer(L),
      LLVMCreateGenericValueOfPointer(f->closure)
  };
  LLVMGenericValueRef result = LLVMRunFunction(Jit->engine, f->value, 2, args);
  return LLVMGenericValueToInt(result, 0);
}

LUAI_FUNC void luaJ_dump (lua_State *L, luaJ_Function *f)
{
  (void)L;
  LLVMDumpModule(f->module);
}

LUAI_FUNC void luaJ_freefunction (lua_State *L, luaJ_Function *f)
{
  if (f) {
    LLVMModuleRef module;
    LLVMRemoveModule(Jit->engine, f->module, &module, NULL);
    LLVMDisposeModule(module);
    luaM_free(L, f);
  }
}

