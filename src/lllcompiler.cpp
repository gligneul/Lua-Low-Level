/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllcompiler.cpp
*/

#include <iostream>
#include <sstream>

#include <llvm/ADT/StringRef.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>

#include "lllcompiler.h"
#include "lllengine.h"
#include "lllruntime.h"

extern "C" {
#include "lprefix.h"
#include "lfunc.h"
#include "lgc.h"
#include "lopcodes.h"
#include "luaconf.h"
#include "lvm.h"
#include "ltable.h"
}

namespace lll {

Compiler::Compiler(LClosure* lclosure) :
    lclosure_(lclosure),
    context_(llvm::getGlobalContext()),
    rt_(Runtime::Instance()),
    engine_(nullptr),
    module_(new llvm::Module("lll_module", context_)),
    function_(CreateMainFunction()),
    blocks_(lclosure->p->sizecode, nullptr),
    builder_(context_),
    curr_(0) {
    static bool init = true;
    if (init) {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();
        init = false;
    }
}

bool Compiler::Compile() {
    CreateBlocks();
    CompileInstructions();
    return VerifyModule() && CreateEngine();
}

const std::string& Compiler::GetErrorMessage() {
    return error_;
}

Engine* Compiler::GetEngine() {
    return engine_.release();
}

llvm::Function* Compiler::CreateMainFunction() {
    auto ret = MakeIntT(sizeof(int));
    auto params = {rt_->GetType("lua_State"), rt_->GetType("LClosure")};
    auto type = llvm::FunctionType::get(ret, params, false);
    return llvm::Function::Create(type, llvm::Function::ExternalLinkage,
            "lll", module_.get());
}

void Compiler::CreateBlocks() {
    auto& args = function_->getArgumentList();
    values_.state = &args.front();
    values_.state->setName("state");
    values_.closure = &args.back();
    values_.closure->setName("closure");

    auto entry = llvm::BasicBlock::Create(context_, "entry", function_);
    builder_.SetInsertPoint(entry);
    values_.ci = LoadField(values_.state, rt_->GetType("CallInfo"),
            offsetof(lua_State, ci), "ci");

    for (size_t i = 0; i < blocks_.size(); ++i) {
        auto instruction = luaP_opnames[GET_OPCODE(lclosure_->p->code[i])];
        std::stringstream name;
        name << "block." << i << "." << instruction;
        blocks_[i] = llvm::BasicBlock::Create(context_, name.str(), function_);
    }

    builder_.CreateBr(blocks_[0]);
}

void Compiler::CompileInstructions() {
    for (curr_ = 0; curr_ < lclosure_->p->sizecode; ++curr_) {
        builder_.SetInsertPoint(blocks_[curr_]);
        instr_ = lclosure_->p->code[curr_];
        switch(GET_OPCODE(instr_)) {
            case OP_MOVE:     CompileMove(); break;
            case OP_LOADK:    CompileLoadk(false); break;
            case OP_LOADKX:   CompileLoadk(true); break;
            case OP_LOADBOOL: CompileLoadbool(); break;
            case OP_LOADNIL:  CompileLoadnil(); break;
            case OP_GETUPVAL: CompileGetupval(); break;
            case OP_GETTABUP: CompileGettabup(); break;
            case OP_GETTABLE: CompileGettable(); break;
            case OP_SETTABUP: CompileSettabup(); break;
            case OP_SETUPVAL: CompileSetupval(); break;
            case OP_SETTABLE: CompileSettable(); break;
            case OP_NEWTABLE: CompileNewtable(); break;
            case OP_SELF:     CompileSelf(); break;
            case OP_ADD:      CompileBinop("lll_addrr"); break;
            case OP_SUB:      CompileBinop("lll_subrr"); break;
            case OP_MUL:      CompileBinop("lll_mulrr"); break;
            case OP_MOD:      CompileBinop("lll_modrr"); break;
            case OP_POW:      CompileBinop("lll_powrr"); break;
            case OP_DIV:      CompileBinop("lll_divrr"); break;
            case OP_IDIV:     CompileBinop("lll_idivrr"); break;
            case OP_BAND:     CompileBinop("lll_bandrr"); break;
            case OP_BOR:      CompileBinop("lll_borrr"); break;
            case OP_BXOR:     CompileBinop("lll_bxorrr"); break;
            case OP_SHL:      CompileBinop("lll_shlrr"); break;
            case OP_SHR:      CompileBinop("lll_shrrr"); break;
            case OP_UNM:      CompileUnop("lll_unm"); break;
            case OP_BNOT:     CompileUnop("lll_bnot"); break;
            case OP_NOT:      CompileUnop("lll_not"); break;
            case OP_LEN:      CompileUnop("luaV_objlen"); break;
            case OP_CONCAT:   CompileConcat(); break;
            case OP_JMP:      CompileJmp(); break;
            case OP_EQ:       CompileCmp("luaV_equalobj"); break;
            case OP_LT:       CompileCmp("luaV_lessthan"); break;
            case OP_LE:       CompileCmp("luaV_lessequal"); break;
            case OP_TEST:     CompileTest(); break;
            case OP_TESTSET:  CompileTestset(); break;
            case OP_CALL:     CompileCall(); break;
            case OP_TAILCALL: CompileCall(); break;
            case OP_RETURN:   CompileReturn(); break;
            case OP_FORLOOP:  CompileForloop(); break;
            case OP_FORPREP:  CompileForprep(); break;
            case OP_TFORCALL: CompileTforcall(); break;
            case OP_TFORLOOP: CompileTforloop(); break;
            case OP_SETLIST:  CompileSetlist(); break;
            case OP_CLOSURE:  CompileClosure(); break;
            case OP_VARARG:   CompileVararg(); break;
            case OP_EXTRAARG: /* ignored */ break;
        }
        if (!blocks_[curr_]->getTerminator())
            builder_.CreateBr(blocks_[curr_ + 1]);
    }
}

bool Compiler::VerifyModule() {
    llvm::raw_string_ostream error_os(error_);
    bool err = llvm::verifyModule(*module_, &error_os);
    if (err)
        module_->dump();
    return !err;
}

bool Compiler::CreateEngine() {
    auto module = module_.get();
    auto engine = llvm::EngineBuilder(module_.release())
            .setErrorStr(&error_)
            .setOptLevel(llvm::CodeGenOpt::None)
            .setEngineKind(llvm::EngineKind::JIT)
            .setUseMCJIT(true)
            .create();
    if (engine) {
        engine->finalizeObject();
        engine_.reset(new Engine(engine, module));
        return true;
    } else {
        return false;
    }
}

void Compiler::CompileMove() {
    UpdateStack();
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    auto rb = GetValueR(GETARG_B(instr_), "rb");
    SetRegister(ra, rb);
}

void Compiler::CompileLoadk(bool extraarg) {
    UpdateStack();
    int kposition = extraarg ? GETARG_Ax(lclosure_->p->code[curr_ + 1])
                             : GETARG_Bx(instr_);
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    auto k = GetValueK(kposition, "k");
    SetRegister(ra, k);
}

void Compiler::CompileLoadbool() {
    UpdateStack();
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    SetField(ra, MakeInt(GETARG_B(instr_)), offsetof(TValue, value_), "value");
    SetField(ra, MakeInt(LUA_TBOOLEAN), offsetof(TValue, tt_), "tag");
    if (GETARG_C(instr_))
        builder_.CreateBr(blocks_[curr_ + 2]);
}

void Compiler::CompileLoadnil() {
    UpdateStack();
    int start = GETARG_A(instr_);
    int end = start + GETARG_B(instr_);
    for (int i = start; i <= end; ++i) {
        auto r = GetValueR(i, "r");
        SetField(r, MakeInt(LUA_TNIL), offsetof(TValue, tt_), "tag");
    }
}

void Compiler::CompileGetupval() {
    UpdateStack();
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    auto upval = GetUpval(GETARG_B(instr_));
    SetRegister(ra, upval);
}

void Compiler::CompileGettabup() {
    UpdateStack();
    auto args = {
        values_.state,
        GetUpval(GETARG_B(instr_)),
        GetValueRK(GETARG_C(instr_), "rkc"),
        GetValueR(GETARG_A(instr_), "ra")
    };
    CreateCall("LLLGetTable", args);
}

void Compiler::CompileGettable() {
    UpdateStack();
    auto args = {
        values_.state,
        GetValueR(GETARG_B(instr_), "rb"),
        GetValueRK(GETARG_C(instr_), "rkc"),
        GetValueR(GETARG_A(instr_), "ra")
    };
    CreateCall("LLLGetTable", args);
}

void Compiler::CompileSettabup() {
    UpdateStack();
    auto args = {
        values_.state,
        GetUpval(GETARG_A(instr_)), 
        GetValueRK(GETARG_B(instr_), "rkb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    CreateCall("LLLSetTable", args);
}

void Compiler::CompileSetupval() {
    UpdateStack();
    auto upvals = LoadField(values_.closure, rt_->GetType("UpVal"),
            offsetof(LClosure, upvals), "upvals");
    auto upval = builder_.CreateGEP(upvals, MakeInt(GETARG_B(instr_)), "upval");
    auto v = LoadField(upval, rt_->GetType("TValue"), offsetof(UpVal, v), "v");
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    SetRegister(v, ra);
    auto args = {values_.state, upval};
    CreateCall("lll_upvalbarrier", args);
}

void Compiler::CompileSettable() {
    UpdateStack();
    auto args = {
        values_.state,
        GetValueR(GETARG_A(instr_), "ra"),
        GetValueRK(GETARG_B(instr_), "rkb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    CreateCall("LLLSetTable", args);
}

void Compiler::CompileNewtable() {
    UpdateStack();
    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    int c = GETARG_C(instr_);
    auto args = {values_.state, GetValueR(a, "ra")};
    auto table = CreateCall("lll_newtable", args);
    if (b != 0 || c != 0) {
        args = {
            values_.state,
            table,
            MakeInt(luaO_fb2int(b)),
            MakeInt(luaO_fb2int(c))
        };
        CreateCall("luaH_resize", args);
    }
    CompileCheckcg(GetValueR(a + 1, "ra1"));
}

void Compiler::CompileSelf() {
    UpdateStack();
    auto args = {
        values_.state,
        GetValueR(GETARG_A(instr_), "ra"),
        GetValueR(GETARG_B(instr_), "rb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    CreateCall("LLLSelf", args);
}

void Compiler::CompileBinop(const std::string& function) {
    UpdateStack();
    auto args = {
        values_.state,
        GetValueR(GETARG_A(instr_), "ra"),
        GetValueRK(GETARG_B(instr_), "rkb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    CreateCall(function, args);
}

void Compiler::CompileUnop(const std::string& function) {
    UpdateStack();
    auto args = {
        values_.state,
        GetValueR(GETARG_A(instr_), "ra"),
        GetValueRK(GETARG_B(instr_), "rkb")
    };
    CreateCall(function, args);
}

void Compiler::CompileConcat() {
    UpdateStack();
    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    int c = GETARG_C(instr_);
    SetTop(c + 1);
    auto args = {values_.state, MakeInt(c - b + 1)};
    CreateCall("luaV_concat", args);

    UpdateStack();
    auto ra = GetValueR(a, "ra");
    auto rb = GetValueR(b, "rb");
    SetRegister(ra, rb);

    if (a >= b)
        CompileCheckcg(GetValueR(a + 1, "ra_next"));
    else
        CompileCheckcg(rb);

    ReloadTop();
}

void Compiler::CompileJmp() {
    builder_.CreateBr(blocks_[curr_ + GETARG_sBx(instr_) + 1]);
}

void Compiler::CompileCmp(const std::string& function) {
    UpdateStack();
    auto args = {
        values_.state,
        GetValueRK(GETARG_B(instr_), "rkb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    auto result = CreateCall(function, args, "result");
    auto a = MakeInt(GETARG_A(instr_));
    auto cmp = builder_.CreateICmpNE(result, a, "cmp");
    builder_.CreateCondBr(cmp, blocks_[curr_ + 2], blocks_[curr_ + 1]);
}

void Compiler::CompileTest() {
    UpdateStack();
    auto args = {MakeInt(GETARG_C(instr_)), GetValueR(GETARG_A(instr_), "ra")};
    auto result = ToBool(CreateCall("lll_test", args, "result"));
    builder_.CreateCondBr(result, blocks_[curr_ + 2], blocks_[curr_ + 1]);
}

void Compiler::CompileTestset() {
    UpdateStack();
    auto rb = GetValueR(GETARG_B(instr_), "rb");
    auto args = {MakeInt(GETARG_C(instr_)), rb};
    auto result = ToBool(CreateCall("lll_test", args, "result"));
    auto setblock = CreateSubBlock("set");
    builder_.CreateCondBr(result, blocks_[curr_ + 2], setblock);
    builder_.SetInsertPoint(setblock);
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    SetRegister(ra, rb);
    builder_.CreateBr(blocks_[curr_ + 1]);
}

void Compiler::CompileCall() {
    UpdateStack();
    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    if (b != 0)
        SetTop(a + b);
    auto args = {
        values_.state,
        GetValueR(a, "ra"),
        MakeInt(GETARG_C(instr_) - 1)
    };
    CreateCall("luaD_callnoyield", args);
}

void Compiler::CompileReturn() {
    UpdateStack();
        //if (cl->p->sizep > 0) luaF_close(L, base);
    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    llvm::Value* nresults = nullptr;
    if (b == 0) {
        nresults = TopDiff(GETARG_A(instr_));
    } else if (b == 1) { 
        nresults = MakeInt(0);
    } else {
        nresults = MakeInt(b - 1);
        SetTop(a + b - 1);
    }
    builder_.CreateRet(nresults);
}

void Compiler::CompileForloop() {
    UpdateStack();
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    auto jump = ToBool(CreateCall("lll_forloop", {ra}, "jump"));
    auto jumpblock = blocks_[curr_ + 1 + GETARG_sBx(instr_)];
    builder_.CreateCondBr(jump, jumpblock, blocks_[curr_ + 1]);
}

void Compiler::CompileForprep() {
    UpdateStack();
    auto args = {values_.state, GetValueR(GETARG_A(instr_), "ra")};
    CreateCall("lll_forprep", args);
    builder_.CreateBr(blocks_[curr_ + 1 + GETARG_sBx(instr_)]);
}

void Compiler::CompileTforcall() {
    UpdateStack();
    int a = GETARG_A(instr_);
    int cb = a + 3;
    SetRegister(GetValueR(cb + 2, "cb2"), GetValueR(a + 2, "ra2"));
    SetRegister(GetValueR(cb + 1, "cb1"), GetValueR(a + 1, "ra1"));
    SetRegister(GetValueR(cb, "cb"), GetValueR(a, "ra"));
    SetTop(cb + 3);
    auto args = {
        values_.state,
        GetValueR(cb, "cb"),
        MakeInt(GETARG_C(instr_))
    };
    CreateCall("luaD_callnoyield", args);
    ReloadTop();
}

void Compiler::CompileTforloop() {
    UpdateStack();
    int a = GETARG_A(instr_);
    auto ra1 = GetValueR(a + 1, "ra1");
    auto tag = LoadField(ra1, MakeIntT(sizeof(int)), offsetof(TValue, tt_),
            "tag");
    auto notnil = builder_.CreateICmpNE(tag, MakeInt(LUA_TNIL), "notnil");
    auto continueblock = CreateSubBlock("continue");
    builder_.CreateCondBr(notnil, continueblock, blocks_[curr_ + 1]);

    builder_.SetInsertPoint(continueblock);
    auto ra = GetValueR(a, "ra");
    SetRegister(ra, ra1);
    builder_.CreateBr(blocks_[curr_ + 1 + GETARG_sBx(instr_)]);
}

void Compiler::CompileSetlist() {
    UpdateStack();

    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    int c = GETARG_C(instr_);
    if (c == 0)
        c = GETARG_Ax(lclosure_->p->code[curr_ + 1]);

    auto n = (b != 0 ? MakeInt(b) : TopDiff(a + 1));
    auto fields = MakeInt((c - 1) * LFIELDS_PER_FLUSH);

    auto args = {values_.state, GetValueR(a, "ra"), fields, n};
    CreateCall("lll_setlist", args);
    ReloadTop();
}

void Compiler::CompileClosure() {
    UpdateStack();
    auto args = {
        values_.state,
        values_.closure,
        GetValueR(0, "base"),
        GetValueR(GETARG_A(instr_), "ra"),
        MakeInt(GETARG_Bx(instr_))
    };
    CreateCall("lll_closure", args);
    CompileCheckcg(GetValueR(GETARG_A(instr_) + 1, "ra1"));
}

void Compiler::CompileVararg() {
#if 0
        int b = GETARG_B(instr_) - 1;

        int b = GETARG_B(i) - 1;
        int j;
        int n = cast_int(base - ci->func) - cl->p->numparams - 1;
        if (b < 0) {  /* B == 0? */
          b = n;  /* get all var. arguments */
          Protect(luaD_checkstack(L, n));
          ra = RA(i);  /* previous call may change the stack */
          L->top = ra + n;
        }
        for (j = 0; j < b; j++) {
          if (j < n) {
            setobjs2s(L, ra + j, base - n + j);
          }
          else {
            setnilvalue(ra + j);
          }
        }
#endif
}

void Compiler::CompileCheckcg(llvm::Value* reg) {
    auto args = {values_.state, values_.ci, reg};
    CreateCall("lll_checkcg", args);
}

llvm::Type* Compiler::MakeIntT(int nbytes) {
    return llvm::IntegerType::get(context_, 8 * nbytes);
}

llvm::Value* Compiler::MakeInt(int value) {
    return llvm::ConstantInt::get(MakeIntT(sizeof(int)), value);
}

llvm::Value* Compiler::ToBool(llvm::Value* value) {
    return builder_.CreateICmpNE(value, MakeInt(0), value->getName());
}

llvm::Value* Compiler::GetFieldPtr(llvm::Value* strukt, llvm::Type* fieldtype,
        size_t offset, const std::string& name) {
    auto memt = llvm::PointerType::get(MakeIntT(1), 0);
    auto mem = builder_.CreateBitCast(strukt, memt, strukt->getName() + "_mem");
    auto element = builder_.CreateGEP(mem, MakeInt(offset), name + "_mem");
    auto ptrtype = llvm::PointerType::get(fieldtype, 0);
    return builder_.CreateBitCast(element, ptrtype, name + "_ptr");
}

llvm::Value* Compiler::LoadField(llvm::Value* strukt, llvm::Type* fieldtype,
        size_t offset, const std::string& name) {
    auto ptr = GetFieldPtr(strukt, fieldtype, offset, name);
    return builder_.CreateLoad(ptr, name);
}

void Compiler::SetField(llvm::Value* strukt, llvm::Value* fieldvalue,
        size_t offset, const std::string& fieldname) {
    auto ptr = GetFieldPtr(strukt, fieldvalue->getType(), offset,fieldname);
    builder_.CreateStore(fieldvalue, ptr);
}

llvm::Value* Compiler::GetValueR(int arg, const std::string& name) {
    return builder_.CreateGEP(values_.func, MakeInt(1 + arg), name);
}

llvm::Value* Compiler::GetValueK(int arg, const std::string& name) {
    auto k = lclosure_->p->k + arg;
    auto intptr = llvm::ConstantInt::get(MakeIntT(sizeof(void*)), (uintptr_t)k);
    return builder_.CreateIntToPtr(intptr, rt_->GetType("TValue"), name);
}

llvm::Value* Compiler::GetValueRK(int arg, const std::string& name) {
    return ISK(arg) ? GetValueK(INDEXK(arg), name) : GetValueR(arg, name);
}

llvm::Value* Compiler::GetUpval(int n) {
    auto upvals = LoadField(values_.closure, rt_->GetType("UpVal"),
            offsetof(LClosure, upvals), "upvals");
    auto upval = builder_.CreateGEP(upvals, MakeInt(n), "upval");
    return LoadField(upval, rt_->GetType("TValue"), offsetof(UpVal, v), "val");
}

llvm::Value* Compiler::CreateCall(const std::string& name,
        std::initializer_list<llvm::Value*> args, const std::string& retname) {
    auto f = rt_->GetFunction(module_.get(), name);
    return builder_.CreateCall(f, args, retname);
}

void Compiler::SetRegister(llvm::Value* reg, llvm::Value* value) {
    auto indices = {MakeInt(0), MakeInt(0)};
    auto value_memptr = builder_.CreateGEP(value, indices, "value_memptr");
    auto value_mem = builder_.CreateLoad(value_memptr, "value_mem"); 
    auto reg_memptr = builder_.CreateGEP(reg, indices, "reg_memptr");
    builder_.CreateStore(value_mem, reg_memptr);
}

void Compiler::ReloadTop() {
    auto top = LoadField(values_.ci, rt_->GetType("TValue"),
            offsetof(CallInfo, top), "top");
    SetField(values_.state, top, offsetof(lua_State, top), "top");
}

void Compiler::SetTop(int reg) {
    auto top = GetValueR(reg, "top");
    SetField(values_.state, top, offsetof(lua_State, top), "top");
}

llvm::Value* Compiler::TopDiff(int n) {
    auto top = LoadField(values_.state, rt_->GetType("TValue"),
            offsetof(lua_State, top), "top");
    auto r = GetValueR(n, "r");
    auto diff = builder_.CreatePtrDiff(top, r, "diff");
    return builder_.CreateIntCast(diff, MakeIntT(sizeof(int)), false, "idiff");
}

void Compiler::UpdateStack() {
    values_.func = LoadField(values_.ci, rt_->GetType("TValue"),
            offsetof(CallInfo, func), "func");
}

llvm::BasicBlock* Compiler::CreateSubBlock(const std::string& suffix) {
    return llvm::BasicBlock::Create(context_,
            blocks_[curr_]->getName() + suffix, function_, blocks_[curr_]);
}

void Compiler::DebugPrint(const std::string& message) {
    auto function = module_->getFunction("printf");
    if (!function) {
        auto rettype = llvm::Type::getVoidTy(context_);
        auto paramtype = llvm::PointerType::get(MakeIntT(1), 0);
        auto functype = llvm::FunctionType::get(rettype, {paramtype}, true);
        function = llvm::Function::Create(functype,
                llvm::Function::ExternalLinkage, "printf", module_.get());
    }
    auto args = {builder_.CreateGlobalStringPtr(message + "\n")};
    builder_.CreateCall(function, args);
}

}

