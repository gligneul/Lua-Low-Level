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
** Holds all jit data of a function
*/
struct luaJ_Function {
  LLVMModuleRef module;             /* Every function have it's own module. */
  LLVMValueRef value;               /* Pointer to the function */
};


/*
** Global jit data
*/
typedef struct luaJ_Jit {
  LLVMExecutionEngineRef engine;    /* Execution engine for LLVM */
  LLVMModuleRef module;             /* Global module with aux functions */
  LLVMTypeRef t_tvalue;             /* TValue*/
  LLVMTypeRef t_gcobject;           /* GCValue */
  LLVMTypeRef t_callinfo;           /* CallInfo */
  LLVMTypeRef t_callinfo_l;         /* CallInfo.u.l */
  LLVMTypeRef t_callinfo_c;         /* CallInfo.u.c */
  LLVMTypeRef t_state;              /* lua_State */
} luaJ_Jit;


/*
** Global data
*/
static luaJ_Jit *Jit = NULL;


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

static void maket_tvalue (luaJ_Jit *Jit) {
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

static void maket_callinfo (luaJ_Jit *Jit) {
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

static void maket_state (luaJ_Jit *Jit) {
  LLVMTypeRef globalstate = maket_struct("global_State");

  LLVMTypeRef state = maket_struct("lua_State");
  Jit->t_state = maket_ptr(state);
  LLVMTypeRef state_elements[] = {
    maket_gccommonheader(Jit->t_gcobject),    /* 0-2: CommonHeader; */
    maket_sizeof(lu_byte),                  /* 3. lu_byte status; */
    Jit->t_tvalue,                            /* 4. StkId top; */
    maket_ptr(globalstate),                 /* 5. global_State *l_G; */
    Jit->t_callinfo,                          /* 6. CallInfo *ci; */
    maket_ptr(maket_sizeof(Instruction)),   /* 7. const Instruction *oldpc; */
    Jit->t_tvalue,                            /* 8. StkId stack_last; */
    Jit->t_tvalue,                            /* 9. StkId stack; */
    maket_ptr(maket_sizeof(UpVal)),         /* 10. UpVal *openupval; */
    Jit->t_gcobject,                          /* 11. GCObject *gclist; */
    Jit->t_state,                             /* 12. struct lua_State *twups; */
    maket_sizeof(struct lua_longjmp*),      /* 13. struct lua_longjmp *err... */
    LLVMGetElementType(Jit->t_callinfo),      /* 14. CallInfo base_ci; */
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

#define makev_loadfield(builder, structure, n) \
            LLVMBuildLoad(builder, makev_getfield(builder, structure, n), "")

static LLVMValueRef makev_getfield (LLVMBuilderRef builder,
                                    LLVMValueRef structure, int n) {
  LLVMValueRef indices[] = {makev_int(0), makev_int(n)};
  return LLVMBuildGEP(builder, structure, indices, 2, "");
}

static LLVMValueRef makev_base (LLVMBuilderRef builder, LLVMValueRef ci,
                                LLVMTypeRef t_ci_l) {
  LLVMValueRef ci_u = makev_getfield(builder, ci, 4);
  LLVMValueRef ci_l = LLVMBuildBitCast(builder, ci_u, t_ci_l, "");
  return makev_loadfield(builder, ci_l, 0);
}
 
static LLVMValueRef makev_printf (LLVMModuleRef module) {
  LLVMTypeRef pf_params[] = {maket_str()};
  LLVMTypeRef pf_type = LLVMFunctionType(LLVMInt32Type(), pf_params, 1, 1);
  return LLVMAddFunction(module, "printf", pf_type);
}


/*
** Compiles the bytecode into LLVM IR
*/

static void connectblocks (LLVMBuilderRef builder, LLVMBasicBlockRef last,
                           LLVMBasicBlockRef current) {
  if (LLVMGetBasicBlockTerminator(last) == NULL) {
    LLVMPositionBuilderAtEnd(builder, last);
    LLVMBuildBr(builder, current);
  }
}

static void compilefunction (luaJ_Jit *Jit, luaJ_Function *f, StkId closure) {
  (void)closure;

  LLVMBuilderRef builder = LLVMCreateBuilder();
  LLVMBasicBlockRef block = LLVMAppendBasicBlock(f->value, "entry");
  LLVMPositionBuilderAtEnd(builder, block);

  LLVMValueRef v_state = LLVMGetParam(f->value, 0);
  LLVMValueRef v_ci = makev_loadfield(builder, v_state, 6);
  LLVMValueRef v_base = makev_base(builder, v_ci, Jit->t_callinfo_l);

  /* Print base value: */
  LLVMValueRef v_printf = makev_printf(f->module);
  LLVMValueRef params[] = {
    LLVMBuildPointerCast(
      builder, LLVMBuildGlobalString(builder, "JIT: base = %p\n", ""),
      maket_str(), ""),
    v_base
  };
  LLVMBuildCall(builder, v_printf, params, 2, "");

#if 0
#define checkliveness(g,obj) \
	lua_longassert(!iscollectable(obj) || \
			(righttt(obj) && !isdead(g,gcvalue(obj))))

#define setobj(L,obj1,obj2) \
	{ TValue *io1=(obj1); *io1 = *(obj2); \
	  (void)L; checkliveness(G(L),io1); }

#define RA(i)	(base+GETARG_A(i))
#define RB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgR, base+GETARG_B(i))
#endif


#if 0
  for (int i = 0; i < p->sizecode; ++i) {
    LLVMBasicBlockRef last_block = block;
    block = LLVMAppendBasicBlock(f->value, "");
    connectblocks(builder, last_block, block);
    LLVMPositionBuilderAtEnd(builder, block);
    
    Instruction instruction = p->code[i];

    switch (GET_OPCODE(instruction)) {
      case OP_MOVE:
        LLVMValueRef rb = makev_rb(
        setobjs2s(L, ra, RB(i));

        break;
      default:
        fprintf(stderr, "Can't compile opcode: %s\n",
            luaP_opnames[GET_OPCODE(instruction)]);
        exit(1);
    }
  }

#endif

  LLVMBuildRet(builder, makev_int(0));
  LLVMDisposeBuilder(builder);

  // TODO better error treatment
  char *error = NULL;
  LLVMVerifyModule(f->module, LLVMPrintMessageAction, &error);
  LLVMDisposeMessage(error);

  LLVMAddModule(Jit->engine, f->module);
}


/*
** Other auxiliary functions
*/

static void initJit (lua_State *L) {
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


/*
** API implementation
*/

LUAI_FUNC luaJ_Function *luaJ_compile (lua_State *L, StkId closure) {
  if (!Jit) initJit(L);

  luaJ_Function *f = luaM_new(L, luaJ_Function);

  /* Creates the module */
  char modulename[16];
  sprintf(modulename, "f%p", closure);
  f->module = LLVMModuleCreateWithName(modulename);

  /* Creates the LLVM function */
  LLVMTypeRef paramtypes[] = {Jit->t_state};
  LLVMTypeRef functype = LLVMFunctionType(LLVMInt32Type(), paramtypes, 1, 0);
  f->value = LLVMAddFunction(f->module, "", functype);

  compilefunction(Jit, f, closure);
  return f;
}

LUAI_FUNC void luaJ_call (lua_State *L, luaJ_Function *f) {
  printf("LUA: base = %p\n", L->ci->u.l.base);
  LLVMGenericValueRef args[] = {LLVMCreateGenericValueOfPointer(L)};
  LLVMRunFunction(Jit->engine, f->value, 1, args);
}

LUAI_FUNC void luaJ_dump (lua_State *L, luaJ_Function *f) {
  (void)L;
  LLVMDumpModule(f->module);
}

LUAI_FUNC void luaJ_freefunction (lua_State *L, luaJ_Function *f) {
  if (f) {
    LLVMModuleRef module;
    LLVMRemoveModule(Jit->engine, f->module, &module, NULL);
    LLVMDisposeModule(module);
    luaM_free(L, f);
  }
}

