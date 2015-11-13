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
  LClosure *closure;                 /* Lua closure */
};

typedef struct luaJ_Jit
{
  LLVMExecutionEngineRef engine;    /* Execution engine for LLVM */
  LLVMModuleRef module;             /* Global module with aux functions */
  LLVMTypeRef t_tvalue;             /* TValue*/
  LLVMTypeRef t_gcobject;           /* GCValue */
  LLVMTypeRef t_callinfo;           /* CallInfo */
  LLVMTypeRef t_callinfo_l;         /* CallInfo.u.l */
  LLVMTypeRef t_callinfo_c;         /* CallInfo.u.c */
  LLVMTypeRef t_state;              /* lua_State */
} luaJ_Jit;
static luaJ_Jit *Jit = NULL;

typedef struct luaJ_CompileState
{
  luaJ_Jit *Jit;
  luaJ_Function *f;
  Proto *proto;
  LLVMBuilderRef builder;
  LLVMValueRef v_state;
  LLVMValueRef v_ci;
  Instruction instr;
  LLVMBasicBlockRef block;
} luaJ_CompileState;


/*
** Macros and functions for creating LLVM types
*/

#define maket_ptr(type)     (LLVMPointerType((type), 0))
#define maket_str()         (maket_ptr(LLVMInt8Type()))
#define maket_sizeof(type)  (LLVMIntType(8 * sizeof(type)))

#define maket_struct(typename) \
            (LLVMStructCreateNamed(LLVMGetGlobalContext(), (typename)))

#define maket_gccommonheader(gcobjectptr) \
            (gcobjectptr),              /* 0. GCObject *next; */ \
            (maket_sizeof(lu_byte)),    /* 1. lu_byte tt; */ \
            (maket_sizeof(lu_byte))     /* 2. lu_byte marked */

static void maket_tvalue (luaJ_Jit *Jit)
{
  LLVMTypeRef value = maket_struct("TValue");
  Jit->t_tvalue = maket_ptr(value);
  LLVMTypeRef value_elements[] = {
    maket_sizeof(Value),  /* 0. Value value_; */
    maket_sizeof(int),    /* 1. int tt_ */
  };
  LLVMStructSetBody(value, value_elements, 2, 0);

  LLVMTypeRef gcobject = maket_struct("GCObject");
  Jit->t_gcobject = maket_ptr(gcobject);
  LLVMTypeRef gcobject_elements[] = {maket_gccommonheader(Jit->t_gcobject)};
  LLVMStructSetBody(gcobject, gcobject_elements, 3, 0);
}

static void maket_callinfo (luaJ_Jit *Jit)
{
  LLVMTypeRef callinfo_l = maket_struct("CallInfo.l");
  Jit->t_callinfo_l = maket_ptr(callinfo_l);
  LLVMTypeRef callinfo_l_elements[] = {
    Jit->t_tvalue,                            /* 0: StkId base; */
    maket_ptr(maket_sizeof(Instruction))    /* 1: const Instruction *savedpc; */
  };
  LLVMStructSetBody(callinfo_l, callinfo_l_elements, 2, 0);

  LLVMTypeRef callinfo_c = maket_struct("CallInfo.c");
  Jit->t_callinfo_c = maket_ptr(callinfo_c);
  LLVMTypeRef callinfo_c_elements[] = {
    maket_sizeof(lua_KFunction),    /* 0: lua_KFunction k; */
    maket_sizeof(ptrdiff_t),        /* 1: ptrdiff_t old_errfunc; */
    maket_sizeof(lua_KContext)      /* 2: lua_KContext ctx; */
  };
  LLVMStructSetBody(callinfo_c, callinfo_c_elements, 3, 0);

  LLVMTypeRef callinfo = maket_struct("CallInfo");
  Jit->t_callinfo = maket_ptr(callinfo);
  LLVMTypeRef elements[] = {
    Jit->t_tvalue,                /* 0: StkId func; */
    Jit->t_tvalue,                /* 1: StkId top; */
    Jit->t_callinfo,              /* 2: struct CallInfo *previous, */
    Jit->t_callinfo,              /* 3:                 *next; */
    callinfo_c,                 /* 4: union u; */
    maket_sizeof(ptrdiff_t),    /* 5: ptrdiff_t extra; */
    maket_sizeof(short),        /* 6: short nresults; */
    maket_sizeof(lu_byte)       /* 7: lu_byte callstatus; */
  };
  LLVMStructSetBody(callinfo, elements, 8, 0);
}

static void maket_state (luaJ_Jit *Jit)
{
  LLVMTypeRef globalstate = maket_struct("global_State");

  LLVMTypeRef state = maket_struct("lua_State");
  Jit->t_state = maket_ptr(state);
  LLVMTypeRef state_elements[] = {
    maket_gccommonheader(Jit->t_gcobject),  /* 0-2: CommonHeader; */
    maket_sizeof(lu_byte),                  /* 3. lu_byte status; */
    Jit->t_tvalue,                          /* 4. StkId top; */
    maket_ptr(globalstate),                 /* 5. global_State *l_G; */
    Jit->t_callinfo,                        /* 6. CallInfo *ci; */
    maket_ptr(maket_sizeof(Instruction)),   /* 7. const Instruction *oldpc; */
    Jit->t_tvalue,                          /* 8. StkId stack_last; */
    Jit->t_tvalue,                          /* 9. StkId stack; */
    maket_ptr(maket_sizeof(UpVal)),         /* 10. UpVal *openupval; */
    Jit->t_gcobject,                        /* 11. GCObject *gclist; */
    Jit->t_state,                           /* 12. struct lua_State *twups; */
    maket_sizeof(struct lua_longjmp*),      /* 13. struct lua_longjmp *err... */
    LLVMGetElementType(Jit->t_callinfo),    /* 14. CallInfo base_ci; */
    maket_sizeof(lua_Hook),                 /* 15. lua_Hook hook; */
    maket_sizeof(ptrdiff_t),                /* 16. ptrdiff_t errfunc; */
    maket_sizeof(int),                      /* 17. int stacksize; */
    maket_sizeof(int),                      /* 18. int basehookcount; */
    maket_sizeof(int),                      /* 19. int hookcount; */
    maket_sizeof(unsigned short),           /* 20. unsigned short nny; */
    maket_sizeof(unsigned short),           /* 21. unsigned short nCcalls; */
    maket_sizeof(lu_byte),                  /* 22. lu_byte hookmask; */
    maket_sizeof(lu_byte),                  /* 23. lu_byte allowhook; */
  };
  LLVMStructSetBody(state, state_elements, 24, 0);
}



/*
** Macros and functions for creating LLVM values
*/

#define makev_int(n) (LLVMConstInt(LLVMInt32Type(), (n), 1))
#define makev_intptr(n) (LLVMConstInt(maket_sizeof(void *), (n), 1))

#define makev_loadfield(builder, structure, n) \
            LLVMBuildLoad(builder, makev_getfield(builder, structure, n), "")

#define makev_string(builder, str) \
            LLVMBuildPointerCast(builder, \
                LLVMBuildGlobalString(builder, str, ""), maket_str(), "")

static LLVMValueRef makev_register (LLVMBuilderRef builder, LLVMValueRef v_func,
                                    int reg)
{
  LLVMValueRef indices[] = {makev_int(1 + reg)};
  return LLVMBuildGEP(builder, v_func, indices, 1, "");
}

static LLVMValueRef makev_kvalue (LLVMBuilderRef builder, TValue *value,
                                  luaJ_Jit *Jit)
{
  LLVMValueRef v_int = LLVMConstInt(maket_sizeof(TValue *),
    (uintptr_t)value, 0);
  return LLVMBuildIntToPtr(builder, v_int, Jit->t_tvalue, "");
}

static LLVMValueRef makev_getfield (LLVMBuilderRef builder,
                                    LLVMValueRef structure, int n)
{
  LLVMValueRef indices[] = {makev_int(0), makev_int(n)};
  return LLVMBuildGEP(builder, structure, indices, 2, "");
}

static LLVMValueRef makev_printf (LLVMModuleRef module)
{
  LLVMTypeRef pf_params[] = {maket_str()};
  LLVMTypeRef pf_type = LLVMFunctionType(LLVMInt32Type(), pf_params, 1, 1);
  return LLVMAddFunction(module, "printf", pf_type);
}



/*
** Compiles the bytecode into LLVM IR
*/

static void createblocks (luaJ_CompileState *cs, LLVMBasicBlockRef *blocks)
{
  LLVMBasicBlockRef entry = LLVMAppendBasicBlock(cs->f->value, "bblock.entry");
  LLVMPositionBuilderAtEnd(cs->builder, entry);
  cs->v_state = LLVMGetParam(cs->f->value, 0);
  cs->v_ci = makev_loadfield(cs->builder, cs->v_state, 6);

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
      LLVMBuildBitCast(builder, value, maket_ptr(maket_sizeof(TValue)), "");
  LLVMDumpValue(value_iptr);
  LLVMValueRef value_i = LLVMBuildLoad(builder, value_iptr, "");
  LLVMValueRef reg_iptr =
      LLVMBuildBitCast(builder, reg, maket_ptr(maket_sizeof(TValue)), "");
  LLVMBuildStore(builder, value_i, reg_iptr);
}

static void settop (LLVMBuilderRef builder, LLVMValueRef v_state,
                    LLVMValueRef v_ci, int offset)
{
  LLVMValueRef v_func = makev_loadfield(builder, v_ci, 0);
  LLVMValueRef v_reg = makev_register(builder, v_func, offset);
  LLVMValueRef v_top = makev_getfield(builder, v_state, 4);
  LLVMBuildStore(builder, v_reg, v_top);
}

static void compile_move (luaJ_CompileState *cs)
{
  int a = GETARG_A(cs->instr);
  int b = GETARG_B(cs->instr);
  LLVMValueRef v_func = makev_loadfield(cs->builder, cs->v_ci, 0);
  LLVMValueRef v_ra = makev_register(cs->builder, v_func, a);
  LLVMValueRef v_rb = makev_register(cs->builder, v_func, b);
  setregister(cs->builder, v_ra, v_rb);
}

static void compile_loadk (luaJ_CompileState *cs)
{
  int a = GETARG_A(cs->instr);
  LLVMValueRef v_func = makev_loadfield(cs->builder, cs->v_ci, 0);
  LLVMValueRef v_ra = makev_register(cs->builder, v_func, a);
  TValue *rb = cs->proto->k + GETARG_Bx(cs->instr);
  LLVMValueRef v_rb = makev_kvalue(cs->builder, rb, cs->Jit);
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
    settop(cs->builder, cs->v_state, cs->v_ci, a + nresults);
  }
  LLVMBuildRet(cs->builder, makev_int(nresults));
}

static void compile (luaJ_Jit *Jit, luaJ_Function *f)
{
  luaJ_CompileState cs;
  cs.Jit = Jit;
  cs.f = f;
  cs.proto = f->closure->p;
  cs.builder = LLVMCreateBuilder();

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
    fprintf(stderr, "\n>>>>> ERROR\n");
    LLVMDisposeMessage(error);
    exit(1);
  }

  LLVMAddModule(Jit->engine, f->module);
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
  maket_tvalue(Jit);
  maket_callinfo(Jit);
  maket_state(Jit);
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
  LLVMTypeRef paramtypes[] = {Jit->t_state};
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

