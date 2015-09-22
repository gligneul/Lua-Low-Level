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

#include <stdio.h>
#include <stdlib.h>
#include <llvm-c/Analysis.h>


/*
** Macros and functions for creating LLVM types
*/

#define maket_ptr(type)     (LLVMPointerType((type), 0))
#define maket_str()         (maket_ptr(LLVMInt8Type()))
#define maket_sizeof(type)  (LLVMIntType(8*sizeof(type)))

#define maket_struct(typename) \
            (LLVMStructCreateNamed(LLVMGetGlobalContext(), (typename)))

#define maket_gccommonheader(gcobjectptr) \
            (gcobjectptr),              /* 0. GCObject *next; */ \
            (maket_sizeof(lu_byte)),    /* 1. lu_byte tt; */ \
            (maket_sizeof(lu_byte))     /* 2. lu_byte marked */

static void maket_tvalue (luaJ_Engine *e) {
  LLVMTypeRef value = maket_struct("TValue");
  e->t_tvalue = maket_ptr(value);
  LLVMTypeRef value_elements[] = {
    maket_sizeof(Value),  /* 0. Value value_; */
    maket_sizeof(int),    /* 1. int tt_ */
  };
  LLVMStructSetBody(value, value_elements, 2, 0);

  LLVMTypeRef gcobject = maket_struct("GCObject");
  e->t_gcobject = maket_ptr(gcobject);
  LLVMTypeRef gcobject_elements[] = {maket_gccommonheader(e->t_gcobject)};
  LLVMStructSetBody(gcobject, gcobject_elements, 3, 0);
}

static void maket_callinfo (luaJ_Engine *e) {
  LLVMTypeRef callinfo_l = maket_struct("CallInfo.l");
  e->t_callinfo_l = maket_ptr(callinfo_l);
  LLVMTypeRef callinfo_l_elements[] = {
    e->t_tvalue,                            /* 0: StkId base; */
    maket_ptr(maket_sizeof(Instruction))    /* 1: const Instruction *savedpc; */
  };
  LLVMStructSetBody(callinfo_l, callinfo_l_elements, 2, 0);

  LLVMTypeRef callinfo_c = maket_struct("CallInfo.c");
  e->t_callinfo_c = maket_ptr(callinfo_c);
  LLVMTypeRef callinfo_c_elements[] = {
    maket_sizeof(lua_KFunction),    /* 0: lua_KFunction k; */
    maket_sizeof(ptrdiff_t),        /* 1: ptrdiff_t old_errfunc; */
    maket_sizeof(lua_KContext)      /* 2: lua_KContext ctx; */
  };
  LLVMStructSetBody(callinfo_c, callinfo_c_elements, 3, 0);

  LLVMTypeRef callinfo = maket_struct("CallInfo");
  e->t_callinfo = maket_ptr(callinfo);
  LLVMTypeRef elements[] = {
    e->t_tvalue,                /* 0: StkId func; */
    e->t_tvalue,                /* 1: StkId top; */
    e->t_callinfo,              /* 2: struct CallInfo *previous, */
    e->t_callinfo,              /* 3:                 *next; */
    callinfo_c,                 /* 4: union u; */
    maket_sizeof(ptrdiff_t),    /* 5: ptrdiff_t extra; */
    maket_sizeof(short),        /* 6: short nresults; */
    maket_sizeof(lu_byte)       /* 7: lu_byte callstatus; */
  };
  LLVMStructSetBody(callinfo, elements, 8, 0);
}

static void maket_state (luaJ_Engine *e) {
  LLVMTypeRef globalstate = maket_struct("global_State");

  LLVMTypeRef state = maket_struct("lua_State");
  e->t_state = maket_ptr(state);
  LLVMTypeRef state_elements[] = {
    maket_gccommonheader(e->t_gcobject),    /* 0-2: CommonHeader; */
    maket_sizeof(lu_byte),                  /* 3. lu_byte status; */
    e->t_tvalue,                            /* 4. StkId top; */
    maket_ptr(globalstate),                 /* 5. global_State *l_G; */
    e->t_callinfo,                          /* 6. CallInfo *ci; */
    maket_ptr(maket_sizeof(Instruction)),   /* 7. const Instruction *oldpc; */
    e->t_tvalue,                            /* 8. StkId stack_last; */
    e->t_tvalue,                            /* 9. StkId stack; */
    maket_ptr(maket_sizeof(UpVal)),         /* 10. UpVal *openupval; */
    e->t_gcobject,                          /* 11. GCObject *gclist; */
    e->t_state,                             /* 12. struct lua_State *twups; */
    maket_sizeof(struct lua_longjmp*),      /* 13. struct lua_longjmp *err... */
    LLVMGetElementType(e->t_callinfo),      /* 14. CallInfo base_ci; */
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

static LLVMValueRef makev_printf (LLVMModuleRef module) {
  LLVMTypeRef pf_params[] = {maket_str()};
  LLVMTypeRef pf_type = LLVMFunctionType(LLVMInt32Type(), pf_params, 1, 1);
  return LLVMAddFunction(module, "printf", pf_type);
}

/*
** Compiles the bytecode into LLVM IR
*/

static void compilefunc (luaJ_Engine *e, luaJ_Func *f) {

  (void)e;

  LLVMBuilderRef builder = LLVMCreateBuilder();
  LLVMBasicBlockRef entry = LLVMAppendBasicBlock(f->value, "entry");
  LLVMPositionBuilderAtEnd(builder, entry);

  makev_printf(f->module);

#if 0
  LLVMValueRef indices[] = {makev_int(0), makev_int(0)};
  LLVMValueRef v1 = LLVMBuildGEP(builder, v_state, indices, 2, "");
  LLVMValueRef v2 = LLVMBuildLoad(builder, v1, "");
  LLVMValueRef params[] = {v2};
  LLVMBuildCall(builder, v_printf, params, 1, "");
#endif

  LLVMBuildRet(builder, makev_int(0));
  LLVMDisposeBuilder(builder);

  // TODO better error treatment
  char *error = NULL;
  LLVMVerifyModule(f->module, LLVMPrintMessageAction, &error);
  LLVMDisposeMessage(error);
}


/*
** Create the structures
*/

static void createengine (lua_State *L) {
  luaJ_Engine *e = luaM_new(L, luaJ_Engine);
  luaJ_setengine(L, e);

  e->module = LLVMModuleCreateWithName("main");

  LLVMLinkInJIT();
  LLVMInitializeNativeTarget();
  char *error = NULL;
  if (LLVMCreateExecutionEngineForModule(&e->engine, e->module, &error)) {
    // TODO better error treatment
    fputs(error, stderr);
    exit(1);
  }

  maket_tvalue(e);
  maket_callinfo(e);
  maket_state(e);
}

static void createfunc (lua_State *L, Proto *p) {
  lua_assert(!luaJ_getfunc(p));
  luaJ_Func *f = luaM_new(L, luaJ_Func);
  luaJ_setfunc(p, f);

  char pname[16];
  sprintf(pname, "f%p", p);
  f->module = LLVMModuleCreateWithName(pname);

  luaJ_Engine *e = luaJ_getengine(L);
  LLVMTypeRef paramtypes[] = {e->t_state};
  LLVMTypeRef functype = LLVMFunctionType(LLVMInt32Type(), paramtypes, 1, 0);
  f->value = LLVMAddFunction(f->module, "", functype);

  compilefunc(e, f);

  LLVMAddModule(e->engine, f->module);
}


/*
** API implementation
*/

LUAI_FUNC void luaJ_compile (lua_State *L, Proto *p) {
  if (!luaJ_getengine(L))
    createengine(L);
  createfunc(L, p);
}

LUAI_FUNC void luaJ_exec (lua_State *L, Proto* p) {
  LLVMExecutionEngineRef engine = luaJ_getengine(L)->engine;
  LLVMValueRef function = luaJ_getfunc(p)->value;
  LLVMGenericValueRef args[] = {LLVMCreateGenericValueOfPointer(L)};
  LLVMRunFunction(engine, function, 1, args);
}

LUAI_FUNC void luaJ_freefunc (lua_State *L, luaJ_Func* f) {
  if (f == NULL) return;
  LLVMModuleRef module;
  LLVMRemoveModule(G(L)->llvmengine->engine, f->module, &module, NULL);
  LLVMDisposeModule(module);
  luaM_free(L, f);
}

LUAI_FUNC void luaJ_freeengine (lua_State *L, luaJ_Engine *e) {
  if (e == NULL) return;
  LLVMDisposeExecutionEngine(e->engine);
  luaM_free(L, e);
}

