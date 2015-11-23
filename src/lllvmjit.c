/*
** LLL - Lua Low-Level
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


/*
** Type definitions
*/

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
  LLVMValueRef state;           /* lua_State */
  LLVMValueRef ci;              /* CallInfo */
  LLVMValueRef func;            /* base = func + 1 */
  LLVMValueRef k;               /* constants */
  Instruction instr;            /* current instruction */
  LLVMBasicBlockRef block;      /* current block */
  struct {                      /* runtime functions */
    LLVMValueRef runtime_arith_rr;
  } rt;
} luaJ_CompileState;



/*
** Macros and functions for creating LLVM types
*/

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



/*
** Macros and functions for creating LLVM values
*/

#define makeint(n)              LLVMConstInt(LLVMInt32Type(), (n), 1)

#define makestring(builder, str) \
  LLVMBuildPointerCast(builder, \
      LLVMBuildGlobalString(builder, str, ""), makestring_t(), "")

#define getfieldptr(cs, value, type, strukt, field) \
  getfieldbyoffset(cs, value, type, offsetof(strukt, field), #field "_ptr")

#define loadfield(cs, value, type, strukt, field) \
  LLVMBuildLoad(cs->builder, \
      getfieldptr(cs, value, type, strukt, field), #field)

LLVMValueRef getfieldbyoffset (luaJ_CompileState *cs, LLVMValueRef value,
                               LLVMTypeRef type, size_t offset,
                               const char *name)
{
  LLVMValueRef mem = LLVMBuildBitCast(cs->builder, value, makestring_t(), "");
  LLVMValueRef index[] = {makeint(offset)};
  LLVMValueRef element = LLVMBuildGEP(cs->builder, mem, index, 1, "");
  return LLVMBuildBitCast(cs->builder, element, makeptr_t(type), name);
}

static LLVMValueRef makeprintf (LLVMModuleRef module)
{
  LLVMTypeRef pf_params[] = {makestring_t()};
  LLVMTypeRef pf_type = LLVMFunctionType(LLVMInt32Type(), pf_params, 1, 1);
  return LLVMAddFunction(module, "printf", pf_type);
}

static LLVMValueRef getvalue_r (luaJ_CompileState *cs, int arg)
{
  LLVMValueRef indices[] = {makeint(1 + arg)};
  return LLVMBuildGEP(cs->builder, cs->func, indices, 1, "");
}

static LLVMValueRef getvalue_k (luaJ_CompileState *cs, int arg)
{
#if 0
  // We can only use this case when L->ci->func is a Lua closure
  LLVMValueRef indices[] = {makeint(arg)};
  return LLVMBuildGEP(cs->builder, cs->k, indices, 1, "");
#else
  TValue *value = cs->proto->k + arg;
  LLVMValueRef v_intptr = LLVMConstInt(makesizeof_t(TValue *),
    (uintptr_t)value, 0);
  return LLVMBuildIntToPtr(cs->builder, v_intptr, cs->Jit->value_type, "");
#endif
}

#define getvalue_rk(cs, arg) \
  (ISK(arg) ? getvalue_k(cs, INDEXK(arg)) : getvalue_r(cs, arg))

#define getvaluedata(cs, type, name) \
  LLVMBuildLoad(cs->builder, \
      LLVMBuildBitCast(cs->builder, cs->func, makeptr_t(type), name "_ptr"), \
      name)

/*
** Runtime functions
*/

static void runtime_arith_rr (lua_State *L, TValue *ra, TValue *rb, TValue *rc)
{
  lua_Number nb; lua_Number nc;
  if (ttisinteger(rb) && ttisinteger(rc)) {
    lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
    setivalue(ra, intop(+, ib, ic));
  } else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
    setfltvalue(ra, luai_numadd(L, nb, nc));
  } else {
    luaT_trybinTM(L, rb, rc, ra, TM_ADD);
  }
}

#define rt_declare(function, ret, ...) \
    do { \
      LLVMTypeRef params[] = {__VA_ARGS__}; \
      int nparams = sizeof(params) / sizeof(LLVMTypeRef); \
      LLVMTypeRef type = LLVMFunctionType(ret, params, nparams, 0); \
      cs->rt.function = LLVMAddFunction(cs->f->module, #function, type); \
    } while (0)

static void declare_runtime (luaJ_CompileState *cs)
{
  LLVMTypeRef lstate = cs->Jit->state_type;
  LLVMTypeRef tvalue = cs->Jit->value_type;
  rt_declare(runtime_arith_rr, LLVMVoidType(), lstate, tvalue, tvalue, tvalue);
}

#define rt_link(function) \
            LLVMAddGlobalMapping(cs->Jit->engine, cs->rt.function, function)

static void link_runtime (luaJ_CompileState *cs)
{
  rt_link(runtime_arith_rr);
}



/*
** Compiles the bytecode into LLVM IR
*/

static void createblocks (luaJ_CompileState *cs, LLVMBasicBlockRef *blocks)
{
  LLVMBasicBlockRef entry = LLVMAppendBasicBlock(cs->f->value, "bblock.entry");
  LLVMPositionBuilderAtEnd(cs->builder, entry);

  cs->state = LLVMGetParam(cs->f->value, 0);
  LLVMSetValueName(cs->state, "L");
  cs->ci = loadfield(cs, cs->state, cs->Jit->ci_type, lua_State, ci);
  cs->func = loadfield(cs, cs->ci, cs->Jit->value_type, CallInfo, func);
  LLVMValueRef closure = getvaluedata(cs, cs->Jit->closure_type, "closure");
  LLVMValueRef proto = loadfield(cs, closure, cs->Jit->proto_type, LClosure, p);
  cs->k = loadfield(cs, proto, cs->Jit->value_type, Proto, k);

  int nblocks = cs->proto->sizecode;
  for (int i = 0; i < nblocks; ++i) {
    char name[128];
    const char *instr = luaP_opnames[GET_OPCODE(cs->proto->code[i])];
    sprintf(name, "bblock.%d.%s", i, instr);
    blocks[i] = LLVMAppendBasicBlock(cs->f->value, name);
  }

  LLVMBuildBr(cs->builder, blocks[0]);
}

static void setregister (LLVMBuilderRef builder, LLVMValueRef reg,
                         LLVMValueRef value)
{
  LLVMValueRef value_iptr =
      LLVMBuildBitCast(builder, value, makeptr_t(makesizeof_t(TValue)), "");
  LLVMDumpValue(value_iptr);
  LLVMValueRef value_i = LLVMBuildLoad(builder, value_iptr, "");
  LLVMValueRef reg_iptr =
      LLVMBuildBitCast(builder, reg, makeptr_t(makesizeof_t(TValue)), "");
  LLVMBuildStore(builder, value_i, reg_iptr);
}

static void settop (luaJ_CompileState *cs, int offset)
{
  LLVMValueRef value = getvalue_r(cs, offset);
  LLVMValueRef top = getfieldptr(cs, cs->state, cs->Jit->value_type, lua_State, top);
  LLVMBuildStore(cs->builder, value, top);
}

static void compile_move (luaJ_CompileState *cs)
{
  int a = GETARG_A(cs->instr);
  int b = GETARG_B(cs->instr);
  LLVMValueRef v_ra = getvalue_r(cs, a);
  LLVMValueRef v_rb = getvalue_r(cs, b);
  setregister(cs->builder, v_ra, v_rb);
}

static void compile_loadk (luaJ_CompileState *cs)
{
  int a = GETARG_A(cs->instr);
  LLVMValueRef v_ra = getvalue_r(cs, a);
  LLVMValueRef v_rb = getvalue_k(cs, GETARG_Bx(cs->instr));
  setregister(cs->builder, v_ra, v_rb);
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
    settop(cs, a + nresults);
  }
  LLVMBuildRet(cs->builder, makeint(nresults));
}

static void compile_arith (luaJ_CompileState *cs)
{
  LLVMValueRef args[] = {
      cs->state,
      getvalue_r(cs, GETARG_A(cs->instr)),
      getvalue_rk(cs, GETARG_B(cs->instr)),
      getvalue_rk(cs, GETARG_C(cs->instr))
  };
  LLVMBuildCall(cs->builder, cs->rt.runtime_arith_rr, args, 4, "");
}

static void compile (luaJ_Jit *Jit, luaJ_Function *f)
{
  luaJ_CompileState cs;
  cs.Jit = Jit;
  cs.f = f;
  cs.proto = f->closure->p;
  cs.builder = LLVMCreateBuilder();
  declare_runtime(&cs);

  LLVMBasicBlockRef blocks[cs.proto->sizecode];
  createblocks(&cs, blocks);

  for (int i = 0; i < cs.proto->sizecode; ++i) {
    cs.instr = cs.proto->code[i];
    cs.block = blocks[i];
    LLVMPositionBuilderAtEnd(cs.builder, cs.block);

    switch (GET_OPCODE(cs.instr)) {
      case OP_MOVE:     compile_move(&cs); break;
      case OP_LOADK:    compile_loadk(&cs); break;
      case OP_RETURN:   compile_return(&cs); break;
      case OP_ADD:      compile_arith(&cs); break;
      default: break;
    }

    if (LLVMGetBasicBlockTerminator(cs.block) == NULL)
      LLVMBuildBr(cs.builder, blocks[i + 1]);
  }

  LLVMDisposeBuilder(cs.builder);

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



/*
** Other auxiliary functions
*/

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



/*
** API implementation
*/

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

