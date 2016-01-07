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
    LLVMValueRef luaV_tonumber_;
    LLVMValueRef luaT_trybinTM;
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

#if LUA_FLOAT_TYPE == LUA_FLOAT_FLOAT
    #define makefloat_t LLVMFloatType
#elif LUA_FLOAT_TYPE == LUA_FLOAT_LONGDOUBLE
    #define makefloat_t LLVMFP128Type
#elif LUA_FLOAT_TYPE == LUA_FLOAT_DOUBLE
    #define makefloat_t LLVMDoubleType
#endif

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

#define setfield(cs, value, fieldvalue, strukt, field) \
  LLVMBuildStore(cs->builder, fieldvalue, \
                 getfieldptr(cs, value, LLVMTypeOf(fieldvalue), strukt, field))

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

#define checkttvaluetag(cs, tvalue, tag) \
  LLVMBuildICmp(cs->builder, LLVMIntEQ, \
                loadfield(cs, tvalue, makeint_t(), TValue, tt_), \
                makeint(tag), "")

/* Creates a new subblock after $mainblock. */
#define addsubblock(cs, mainblock, suffix) \
  addsubblockafter(cs, mainblock, mainblock, suffix)

/* Creates a new subblock, and inserts it after @previousblock. */
static LLVMBasicBlockRef addsubblockafter (luaJ_CompileState *cs,
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

/* Returns the float representation of $tvalue, if it exists.
** Jumps to $thenblock if it exists, otherwise jumps to $elseblock. */
static LLVMValueRef checkfloat (luaJ_CompileState *cs, LLVMValueRef tvalue,
        LLVMBasicBlockRef testblock, LLVMBasicBlockRef thenblock,
        LLVMBasicBlockRef elseblock)
{
  LLVMPositionBuilderAtEnd(cs->builder, testblock);
  LLVMValueRef isfloat = checkttvaluetag(cs, tvalue, LUA_TNUMFLT);
  LLVMBasicBlockRef floatblock = addsubblock(cs, testblock, "float");
  LLVMBasicBlockRef convertblock =
      addsubblockafter(cs, testblock, floatblock, "convert");
  LLVMBuildCondBr(cs->builder, isfloat, floatblock, convertblock);

  LLVMPositionBuilderAtEnd(cs->builder, floatblock);
  LLVMValueRef floatval = loadfield(cs, tvalue, makefloat_t(), TValue, value_);
  LLVMBuildBr(cs->builder, thenblock);

  LLVMPositionBuilderAtEnd(cs->builder, convertblock);
  LLVMValueRef convertptr = LLVMBuildAlloca(cs->builder, makefloat_t(),
      "convertptr");
  LLVMValueRef tonumber_params[] = {tvalue, convertptr};
  LLVMValueRef tonumber_ret = LLVMBuildCall(cs->builder, cs->rt.luaV_tonumber_,
      tonumber_params, 2, "");
  LLVMValueRef convertval = LLVMBuildLoad(cs->builder, convertptr,
      "convertval");
  LLVMValueRef isconverted = LLVMBuildICmp(cs->builder, LLVMIntNE, makeint(0), 
      tonumber_ret, "");
  LLVMBuildCondBr(cs->builder, isconverted, thenblock, elseblock);

  LLVMPositionBuilderAtEnd(cs->builder, thenblock);
  LLVMValueRef phi = LLVMBuildPhi(cs->builder, makefloat_t(), "");
  LLVMValueRef incoming_values[] = {floatval, convertval};
  LLVMBasicBlockRef incoming_blocks[] = {floatblock, convertblock};
  LLVMAddIncoming(phi, incoming_values, incoming_blocks, 2);
  return phi;
}



/* Runtime functions */

static void declare_runtime (luaJ_CompileState *cs)
{
  #define rt_declare(function, ret, ...) \
    do { \
      LLVMTypeRef params[] = {__VA_ARGS__}; \
      int nparams = sizeof(params) / sizeof(LLVMTypeRef); \
      LLVMTypeRef type = LLVMFunctionType(ret, params, nparams, 0); \
      cs->rt.function = LLVMAddFunction(cs->f->module, #function, type); \
    } while (0)

  LLVMTypeRef lstate = cs->Jit->state_type;
  LLVMTypeRef tvalue = cs->Jit->value_type;
  rt_declare(luaV_tonumber_, makeint_t(), tvalue, makeptr_t(makefloat_t()));
  rt_declare(luaT_trybinTM, LLVMVoidType(), lstate, tvalue, tvalue, tvalue,
       makeint_t());
  rt_declare(luaV_equalobj, makeint_t(), lstate, tvalue, tvalue);
  rt_declare(luaV_lessthan, makeint_t(), lstate, tvalue, tvalue);
  rt_declare(luaV_lessequal, makeint_t(), lstate, tvalue, tvalue);
}

static void link_runtime (luaJ_CompileState *cs)
{
  #define rt_link(function) \
    LLVMAddGlobalMapping(cs->Jit->engine, cs->rt.function, function)

  rt_link(luaV_tonumber_);
  rt_link(luaT_trybinTM);
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

static void compile_intarith (luaJ_CompileState *cs,
        LLVMBasicBlockRef entryblock, LLVMBasicBlockRef elseblock,
        LLVMBasicBlockRef endblock, LLVMValueRef rb, LLVMValueRef rc)
{
  LLVMPositionBuilderAtEnd(cs->builder, entryblock);
  LLVMValueRef is_rb_int = checkttvaluetag(cs, rb, LUA_TNUMINT);
  LLVMBasicBlockRef tmpblock = addsubblock(cs, entryblock, "tmp");
  LLVMBuildCondBr(cs->builder, is_rb_int, tmpblock, elseblock);
  
  LLVMPositionBuilderAtEnd(cs->builder, tmpblock);
  LLVMValueRef is_rc_int = checkttvaluetag(cs, rc, LUA_TNUMINT);
  LLVMBasicBlockRef intblock =
      addsubblockafter(cs, entryblock, tmpblock, "computeint");
  LLVMBuildCondBr(cs->builder, is_rc_int, intblock, elseblock);

  LLVMPositionBuilderAtEnd(cs->builder, intblock);
  LLVMValueRef ra = gettvalue_r(cs, GETARG_A(cs->instr), "ra");
  setfield(cs, ra, makeint(LUA_TNUMINT), TValue, tt_);
  LLVMTypeRef luaint_t = makesizeof_t(lua_Integer);
  LLVMValueRef valb = loadfield(cs, rb, luaint_t, TValue, value_);
  LLVMValueRef valc = loadfield(cs, rc, luaint_t, TValue, value_);
  LLVMValueRef vala= LLVMBuildBinOp(cs->builder, LLVMAdd, valb, valc, "vala");
  setfield(cs, ra, vala, TValue, value_);
  LLVMBuildBr(cs->builder, endblock);

  // TODO verify:
  // #define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))
}

static void compile_floatarith (luaJ_CompileState *cs,
        LLVMBasicBlockRef entryblock, LLVMBasicBlockRef elseblock,
        LLVMBasicBlockRef endblock, LLVMValueRef rb, LLVMValueRef rc)
{
  LLVMBasicBlockRef tmpblock = addsubblock(cs, entryblock, "tmp");
  LLVMValueRef valb = checkfloat(cs, rb, entryblock, tmpblock, elseblock);

  LLVMBasicBlockRef floatblock = addsubblockafter(cs, entryblock, tmpblock, 
      "compute");
  LLVMValueRef valc = checkfloat(cs, rc, tmpblock, floatblock, elseblock);

  LLVMPositionBuilderAtEnd(cs->builder, floatblock);
  LLVMValueRef ra = gettvalue_r(cs, GETARG_A(cs->instr), "ra");
  setfield(cs, ra, makeint(LUA_TNUMFLT), TValue, tt_);
  LLVMValueRef vala = LLVMBuildBinOp(cs->builder, LLVMFAdd, valb, valc, "vala");
  setfield(cs, ra, vala, TValue, value_);
  LLVMBuildBr(cs->builder, endblock);
}

static void compile_tmarith (luaJ_CompileState *cs, LLVMBasicBlockRef tmblock,
        LLVMBasicBlockRef endblock, LLVMValueRef rb, LLVMValueRef rc)
{
  LLVMPositionBuilderAtEnd(cs->builder, tmblock);
  LLVMValueRef args[] = {
    cs->state, rb, rc,
    gettvalue_rk(cs, GETARG_A(cs->instr), "ra"),
    makeint(TM_ADD)
  };
  LLVMBuildCall(cs->builder, cs->rt.luaT_trybinTM, args, 5, "");
  LLVMBuildBr(cs->builder, endblock);
}

static void compile_arith (luaJ_CompileState *cs)
{
  // Create blocks
  LLVMBasicBlockRef floatblock = addsubblock(cs, cs->block, "float");
  LLVMBasicBlockRef tmblock = addsubblockafter(cs, cs->block, floatblock, "tm");
  LLVMBasicBlockRef endblock = addsubblockafter(cs, cs->block, tmblock, "end");

  // Get operands
  updatestack(cs);
  LLVMPositionBuilderAtEnd(cs->builder, cs->block);
  LLVMValueRef rb = gettvalue_rk(cs, GETARG_B(cs->instr), "rb");
  LLVMValueRef rc = gettvalue_rk(cs, GETARG_C(cs->instr), "rc");

  // Compile sub-cases
  compile_intarith(cs, cs->block, floatblock, endblock, rb, rc);
  compile_floatarith(cs, floatblock, tmblock, endblock, rb, rc);
  compile_tmarith(cs, tmblock, endblock, rb, rc);
  cs->block = endblock;
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
      case OP_LOADK:    /* bellow */
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
      case OP_ADD:      compile_arith(cs); break;
      case OP_SUB:      /* TODO */ break;
      case OP_MUL:      /* TODO */ break;
      case OP_MOD:      /* TODO */ break;
      case OP_POW:      /* TODO */ break;
      case OP_DIV:      /* TODO */ break;
      case OP_IDIV:     /* TODO */ break;
      case OP_BAND:     /* TODO */ break;
      case OP_BOR:      /* TODO */ break;
      case OP_BXOR:     /* TODO */ break;
      case OP_SHL:      /* TODO */ break;
      case OP_SHR:      /* TODO */ break;
      case OP_UNM:      /* TODO */ break;
      case OP_BNOT:     /* TODO */ break;
      case OP_NOT:      /* TODO */ break;
      case OP_LEN:      /* TODO */ break;
      case OP_CONCAT:   /* TODO */ break;
      case OP_JMP:      compile_jmp(cs); break;
      case OP_EQ:       /* bellow */
      case OP_LT:       /* bellow */
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
    if (LLVMGetBasicBlockTerminator(cs.block) == NULL) {
      LLVMPositionBuilderAtEnd(cs.builder, cs.block);
      LLVMBuildBr(cs.builder, blocks[i + 1]);
    }
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

