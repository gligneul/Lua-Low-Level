/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllvmjit.h
*/

#define lllvmjit_c

#include "lprefix.h"

#include "lfunc.h"
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
  LLVMTypeRef value_type;
  LLVMTypeRef ci_type;
  LLVMTypeRef closure_type;
  LLVMTypeRef proto_type;
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
  LLVMValueRef ci;              /* CallInfo */
  LLVMValueRef func;            /* base = func + 1 */
  int idx;                      /* current block/instruction index */
  Instruction instr;            /* current instruction */
  LLVMBasicBlockRef block;      /* current block */
  struct {                      /* runtime functions */
    LLVMValueRef runtime_addrr;
    LLVMValueRef runtime_subrr;
    LLVMValueRef runtime_mulrr;
    LLVMValueRef runtime_divrr;
    LLVMValueRef runtime_bandrr;
    LLVMValueRef runtime_borrr;
    LLVMValueRef runtime_bxorrr;
    LLVMValueRef runtime_shlrr;
    LLVMValueRef runtime_shrrr;
    LLVMValueRef runtime_modrr;
    LLVMValueRef runtime_idivrr;
    LLVMValueRef runtime_powrr;
    LLVMValueRef luaV_equalobj;
    LLVMValueRef luaV_lessthan;
    LLVMValueRef luaV_lessequal;
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

#if 0
static LLVMValueRef makeprintf (LLVMModuleRef module)
{
  LLVMTypeRef pf_params[] = {makestring_t()};
  LLVMTypeRef pf_type = LLVMFunctionType(LLVMInt32Type(), pf_params, 1, 1);
  return LLVMAddFunction(module, "printf", pf_type);
}
#endif

static LLVMValueRef gettvalue_r (luaJ_CompileState *cs, int arg,
        const char* name)
{
  LLVMValueRef indices[] = {makeint(1 + arg)};
  return LLVMBuildGEP(cs->builder, cs->func, indices, 1, name);
}

static LLVMValueRef gettvalue_k (luaJ_CompileState *cs, int arg,
        const char *name)
{
  TValue *value = cs->proto->k + arg;
  LLVMValueRef v_intptr = LLVMConstInt(makesizeof_t(TValue *),
    (uintptr_t)value, 0);
  return LLVMBuildIntToPtr(cs->builder, v_intptr, cs->Jit->value_type, name);
}

#define gettvalue_rk(cs, arg, name) \
  (ISK(arg) ? gettvalue_k(cs, INDEXK(arg), name) : gettvalue_r(cs, arg, name))

#define getvaluedata(cs, type, name) \
  LLVMBuildLoad(cs->builder, \
      LLVMBuildBitCast(cs->builder, cs->func, makeptr_t(type), name "_ptr"), \
      name)



/* Runtime functions */

static void runtime_addrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
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

static void runtime_subrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
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

static void runtime_mulrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
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

static void runtime_divrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Number nb; lua_Number nc;
  if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
    setfltvalue(ra, luai_numdiv(L, nb, nc));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_DIV);
  }
}

static void runtime_bandrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, intop(&, ib, ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_BAND);
  }
}

static void runtime_borrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, intop(|, ib, ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_BOR);
  }
}

static void runtime_bxorrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, intop(^, ib, ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_BXOR);
  }
}

static void runtime_shlrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, luaV_shiftl(ib, ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_SHL);
  }
}

static void runtime_shrrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Integer ib, ic;
  if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
    setivalue(ra, luaV_shiftl(ib, -ic));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_SHR);
  }
}

static void runtime_modrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
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

static void runtime_idivrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
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

static void runtime_powrr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Number nb, nc;
  if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
    setfltvalue(ra, luai_numpow(L, nb, nc));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_POW);
  }
}

static void declare_runtime (luaJ_CompileState *cs)
{
  #define rt_declare(function, ret, ...) \
    do { \
      LLVMTypeRef params[] = {__VA_ARGS__}; \
      int nparams = sizeof(params) / sizeof(LLVMTypeRef); \
      LLVMTypeRef type = LLVMFunctionType(ret, params, nparams, 0); \
      cs->rt.function = LLVMAddFunction(cs->f->module, #function, type); \
    } while (0)

  #define declarebinop(function) \
    rt_declare(function, LLVMVoidType(), lstate, tvalue, tvalue, tvalue)

  LLVMTypeRef lstate = cs->Jit->state_type;
  LLVMTypeRef tvalue = cs->Jit->value_type;

  declarebinop(runtime_addrr);
  declarebinop(runtime_subrr);
  declarebinop(runtime_mulrr);
  declarebinop(runtime_divrr);
  declarebinop(runtime_bandrr);
  declarebinop(runtime_borrr);
  declarebinop(runtime_bxorrr);
  declarebinop(runtime_shlrr);
  declarebinop(runtime_shrrr);
  declarebinop(runtime_modrr);
  declarebinop(runtime_idivrr);
  declarebinop(runtime_powrr);

  rt_declare(luaV_equalobj, makeint_t(), lstate, tvalue, tvalue);
  rt_declare(luaV_lessthan, makeint_t(), lstate, tvalue, tvalue);
  rt_declare(luaV_lessequal, makeint_t(), lstate, tvalue, tvalue);
}

static void link_runtime (luaJ_CompileState *cs)
{
  #define rt_link(function) \
    LLVMAddGlobalMapping(cs->Jit->engine, cs->rt.function, function)

  rt_link(runtime_addrr);
  rt_link(runtime_subrr);
  rt_link(runtime_mulrr);
  rt_link(runtime_divrr);
  rt_link(runtime_bandrr);
  rt_link(runtime_borrr);
  rt_link(runtime_bxorrr);
  rt_link(runtime_shlrr);
  rt_link(runtime_shrrr);
  rt_link(runtime_modrr);
  rt_link(runtime_idivrr);
  rt_link(runtime_powrr);
  rt_link(luaV_equalobj);
  rt_link(luaV_lessthan);
  rt_link(luaV_lessequal);
}



/* Compiles the bytecode into LLVM IR */

#define updatestack(cs) \
  cs->func = loadfield(cs, cs->ci, cs->Jit->value_type, CallInfo, func);

static void createblocks (luaJ_CompileState *cs)
{
  LLVMBasicBlockRef entry = LLVMAppendBasicBlock(cs->f->value, "bblock.entry");
  LLVMPositionBuilderAtEnd(cs->builder, entry);

  cs->state = LLVMGetParam(cs->f->value, 0);
  LLVMSetValueName(cs->state, "L");
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
  LLVMValueRef value = gettvalue_r(cs, offset, "topvalue");
  LLVMValueRef top =
        getfieldptr(cs, cs->state, cs->Jit->value_type, lua_State, top);
  LLVMBuildStore(cs->builder, value, top);
}

static void compile_move (luaJ_CompileState *cs)
{
  updatestack(cs);
  int a = GETARG_A(cs->instr);
  int b = GETARG_B(cs->instr);
  LLVMValueRef v_ra = gettvalue_r(cs, a, "ra");
  LLVMValueRef v_rb = gettvalue_r(cs, b, "rb");
  setregister(cs->builder, v_ra, v_rb);
}

static void compile_loadk (luaJ_CompileState *cs)
{
  updatestack(cs);
  int kposition = (GET_OPCODE(cs->instr) == OP_LOADK) ?
    GETARG_Bx(cs->instr) : GETARG_Ax(cs->instr + 1);
  LLVMValueRef ra = gettvalue_r(cs, GETARG_A(cs->instr), "ra");
  LLVMValueRef kvalue = gettvalue_k(cs, kposition, "kvalue");
  setregister(cs->builder, ra, kvalue);
}

static void compile_loadbool (luaJ_CompileState *cs)
{
  updatestack(cs);
  LLVMValueRef ra = gettvalue_r(cs, GETARG_A(cs->instr), "ra");
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
    LLVMValueRef r = gettvalue_r(cs, i, "r");
    LLVMValueRef tag = getfieldptr(cs, r, makeint_t(), TValue, tt_);
    LLVMBuildStore(cs->builder, makeint(LUA_TNIL), tag);
  }
}

static void compile_binop (luaJ_CompileState *cs, LLVMValueRef op)
{
  updatestack(cs);
  LLVMValueRef args[] = {
      cs->state,
      gettvalue_r(cs, GETARG_A(cs->instr), "ra"),
      gettvalue_rk(cs, GETARG_B(cs->instr), "rkb"),
      gettvalue_rk(cs, GETARG_C(cs->instr), "rkc")
  };
  LLVMBuildCall(cs->builder, op, args, 4, "");
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
      gettvalue_rk(cs, GETARG_B(cs->instr), "rkb"),
      gettvalue_rk(cs, GETARG_C(cs->instr), "rkc")
  };

  LLVMValueRef function = NULL;
  switch (GET_OPCODE(cs->instr)) {
    case OP_EQ: function = cs->rt.luaV_equalobj; break;
    case OP_LT: function = cs->rt.luaV_lessthan; break;
    case OP_LE: function = cs->rt.luaV_lessequal; break;
    default:    lua_assert(false);
  }
  LLVMValueRef result = LLVMBuildCall(cs->builder, function, args, 3, "");
  LLVMValueRef a = makeint(GETARG_A(cs->instr));
  LLVMValueRef cmp = LLVMBuildICmp(cs->builder, LLVMIntNE, result, a, "");
  LLVMBasicBlockRef next_block = cs->blocks[cs->idx + 2];
  LLVMBasicBlockRef jmp_block = cs->blocks[cs->idx + 1];
  LLVMBuildCondBr(cs->builder, cmp, next_block, jmp_block);
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
      case OP_GETTABUP: /* TODO */ break;
      case OP_GETTABLE: /* TODO */ break;
      case OP_SETTABUP: /* TODO */ break;
      case OP_SETUPVAL: /* TODO */ break;
      case OP_SETTABLE: /* TODO */ break;
      case OP_NEWTABLE: /* TODO */ break;
      case OP_SELF:     /* TODO */ break;
      case OP_ADD:      compile_binop(cs, cs->rt.runtime_addrr); break;
      case OP_SUB:      compile_binop(cs, cs->rt.runtime_subrr); break;
      case OP_MUL:      compile_binop(cs, cs->rt.runtime_mulrr); break;
      case OP_MOD:      compile_binop(cs, cs->rt.runtime_modrr); break;
      case OP_POW:      compile_binop(cs, cs->rt.runtime_powrr); break;
      case OP_DIV:      compile_binop(cs, cs->rt.runtime_divrr); break;
      case OP_IDIV:     compile_binop(cs, cs->rt.runtime_idivrr); break;
      case OP_BAND:     compile_binop(cs, cs->rt.runtime_bandrr); break;
      case OP_BOR:      compile_binop(cs, cs->rt.runtime_borrr); break;
      case OP_BXOR:     compile_binop(cs, cs->rt.runtime_bxorrr); break;
      case OP_SHL:      compile_binop(cs, cs->rt.runtime_shlrr); break;
      case OP_SHR:      compile_binop(cs, cs->rt.runtime_shrrr); break;
      case OP_UNM:      /* TODO */ break;
      case OP_BNOT:     /* TODO */ break;
      case OP_NOT:      /* TODO */ break;
      case OP_LEN:      /* TODO */ break;
      case OP_CONCAT:   /* TODO */ break;
      case OP_JMP:      compile_jmp(cs); break;
      case OP_EQ:       compile_cmp(cs); break;
      case OP_LT:       compile_cmp(cs); break;
      case OP_LE:       compile_cmp(cs); break;
      case OP_TEST:     /* TODO */ break;
      case OP_TESTSET:  /* TODO */ break;
      case OP_CALL:     /* TODO */ break;
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
  cs.Jit = Jit;
  cs.f = f;
  cs.proto = f->closure->p;
  cs.builder = LLVMCreateBuilder();
  LLVMBasicBlockRef blocks[cs.proto->sizecode];
  cs.blocks = blocks;
  
  declare_runtime(&cs);
  createblocks(&cs);

  for (int i = 0; i < cs.proto->sizecode; ++i) {
    cs.idx = i;
    cs.instr = cs.proto->code[i];
    cs.block = blocks[i];
    LLVMPositionBuilderAtEnd(cs.builder, cs.block);
    compile_opcode(&cs);
    if (LLVMGetBasicBlockTerminator(cs.block) == NULL)
      LLVMBuildBr(cs.builder, blocks[i + 1]);
  }

  LLVMDisposeBuilder(cs.builder);

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
  link_runtime(&cs);
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
  Jit->ci_type = makestruct_t(CallInfo);
  Jit->value_type = makestruct_t(TValue);
  Jit->closure_type = makestruct_t(Closure);
  Jit->proto_type = makestruct_t(Proto);
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
  LLVMTypeRef paramtypes[] = {Jit->state_type};
  LLVMTypeRef functype = LLVMFunctionType(LLVMInt32Type(), paramtypes, 1, 0);
  f->value = LLVMAddFunction(f->module, "", functype);
  f->closure = clLvalue(closure);

  compile(Jit, f);
  return f;
}

LUAI_FUNC int luaJ_call (lua_State *L, luaJ_Function *f)
{
  precall(L, f);
  LLVMGenericValueRef args[] = {LLVMCreateGenericValueOfPointer(L)};
  LLVMGenericValueRef result = LLVMRunFunction(Jit->engine, f->value, 1, args);
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

