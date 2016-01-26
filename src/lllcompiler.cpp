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
            case OP_FORLOOP:  /* TODO */ break;
            case OP_FORPREP:  /* TODO */ break;
            case OP_TFORCALL: /* TODO */ break;
            case OP_TFORLOOP: /* TODO */ break;
            case OP_SETLIST:  /* TODO */ break;
            case OP_CLOSURE:  /* TODO */ break;
            case OP_VARARG:   /* TODO */ break;
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
    builder_.CreateCall(GetFunction("luaV_gettable"), args);
}

void Compiler::CompileGettable() {
    UpdateStack();
    auto args = {
        values_.state,
        GetValueR(GETARG_B(instr_), "rb"),
        GetValueRK(GETARG_C(instr_), "rkc"),
        GetValueR(GETARG_A(instr_), "ra")
    };
    builder_.CreateCall(GetFunction("luaV_gettable"), args);
}

void Compiler::CompileSettabup() {
    UpdateStack();
    auto args = {
        values_.state,
        GetUpval(GETARG_A(instr_)), 
        GetValueRK(GETARG_B(instr_), "rkb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    builder_.CreateCall(GetFunction("luaV_settable"), args);
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
    builder_.CreateCall(GetFunction("lll_upvalbarrier"), args);
}

void Compiler::CompileSettable() {
    UpdateStack();
    auto args = {
        values_.state,
        GetValueR(GETARG_A(instr_), "ra"),
        GetValueRK(GETARG_B(instr_), "rkb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    builder_.CreateCall(GetFunction("luaV_settable"), args);
}

void Compiler::CompileNewtable() {
    UpdateStack();
    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    int c = GETARG_C(instr_);
    auto args = {values_.state, GetValueR(a, "ra")};
    auto table = builder_.CreateCall(GetFunction("lll_newtable"), args);
    if (b != 0 || c != 0) {
        args = {values_.state, table, MakeInt(b), MakeInt(c)};
        builder_.CreateCall(GetFunction("luaH_resize"), args);
    }
    CompileCheckcg(GetValueR(a + 1, "next"));
}

void Compiler::CompileSelf() {
    UpdateStack();
    auto rapp = GetValueR(GETARG_A(instr_) + 1, "rapp");
    auto rb = GetValueR(GETARG_B(instr_), "rb");
    SetRegister(rapp, rb);
    auto args = {
        values_.state,
        rb,
        GetValueRK(GETARG_C(instr_), "rkc"),
        GetValueR(GETARG_A(instr_), "ra")
    };
    builder_.CreateCall(GetFunction("luaV_gettable"), args);
}

void Compiler::CompileBinop(const std::string& function) {
    UpdateStack();
    auto args = {
        values_.state,
        GetValueR(GETARG_A(instr_), "ra"),
        GetValueRK(GETARG_B(instr_), "rkb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    builder_.CreateCall(GetFunction(function), args);
}

void Compiler::CompileUnop(const std::string& function) {
    UpdateStack();
    auto args = {
        values_.state,
        GetValueR(GETARG_A(instr_), "ra"),
        GetValueRK(GETARG_B(instr_), "rkb")
    };
    builder_.CreateCall(GetFunction(function), args);
}

void Compiler::CompileConcat() {
    UpdateStack();
    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    int c = GETARG_C(instr_);
    SetTop(c + 1);
    auto args = {values_.state, MakeInt(c - b + 1)};
    builder_.CreateCall(GetFunction("luaV_concat"), args);

    UpdateStack();
    auto ra = GetValueR(a, "ra");
    auto rb = GetValueR(b, "rb");
    SetRegister(ra, rb);

    if (a >= b)
        CompileCheckcg(GetValueR(a + 1, "ra_next"));
    else
        CompileCheckcg(rb);

    auto oldtop = LoadField(values_.ci, rt_->GetType("TValue"),
            offsetof(CallInfo, top), "oldtop");
    SetField(values_.state, oldtop, offsetof(lua_State, top), "top");
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
    auto result = builder_.CreateCall(GetFunction(function), args, "result");
    auto a = MakeInt(GETARG_A(instr_));
    auto cmp = builder_.CreateICmpNE(result, a, "cmp");
    builder_.CreateCondBr(cmp, blocks_[curr_ + 2], blocks_[curr_ + 1]);
}

void Compiler::CompileTest() {
    UpdateStack();
    auto args = {MakeInt(GETARG_C(instr_)), GetValueR(GETARG_A(instr_), "ra")};
    auto result = builder_.CreateCall(GetFunction("lll_test"), args, "result");
    auto test = builder_.CreateICmpNE(result, MakeInt(0), "test");
    builder_.CreateCondBr(test, blocks_[curr_ + 2], blocks_[curr_ + 1]);
}

void Compiler::CompileTestset() {
    UpdateStack();
    auto rb = GetValueR(GETARG_B(instr_), "rb");
    auto args = {MakeInt(GETARG_C(instr_)), rb};
    auto result = builder_.CreateCall(GetFunction("lll_test"), args, "result");
    auto test = builder_.CreateICmpNE(result, MakeInt(0), "test");
    auto setblock = llvm::BasicBlock::Create(context_,
            blocks_[curr_]->getName() + ".set", function_, blocks_[curr_]);
    builder_.CreateCondBr(test, blocks_[curr_ + 2], setblock);
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
        MakeInt(GETARG_C(instr_) - 1),
        MakeInt(0)
    };
    builder_.CreateCall(GetFunction("luaD_call"), args);
}

void Compiler::CompileReturn() {
    UpdateStack();
    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    llvm::Value* nresults = nullptr;
    if (b == 0) {
        auto ra = GetValueR(GETARG_A(instr_), "ra");
        auto top = LoadField(values_.state, rt_->GetType("TValue"),
                offsetof(lua_State, top), "top");
        auto diff = builder_.CreatePtrDiff(top, ra, "diff");
        nresults = builder_.CreateIntCast(diff, MakeIntT(sizeof(int)), false);
    } else if (b == 1) { 
        nresults = MakeInt(0);
    } else {
        nresults = MakeInt(b - 1);
        SetTop(a + b - 1);
    }
    builder_.CreateRet(nresults);
}

void Compiler::CompileCheckcg(llvm::Value* reg) {
    auto args = {values_.state, values_.ci, reg};
    builder_.CreateCall(GetFunction("lll_checkcg"), args);
}

llvm::Type* Compiler::MakeIntT(int nbytes) {
    return llvm::IntegerType::get(context_, 8 * nbytes);
}

llvm::Value* Compiler::MakeInt(int value) {
    return llvm::ConstantInt::get(MakeIntT(sizeof(int)), value);
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

llvm::Function* Compiler::GetFunction(const std::string& name) {
    return rt_->GetFunction(module_.get(), name);
}

void Compiler::SetRegister(llvm::Value* reg, llvm::Value* value) {
    auto indices = {MakeInt(0), MakeInt(0)};
    auto value_memptr = builder_.CreateGEP(value, indices, "value_memptr");
    auto value_mem = builder_.CreateLoad(value_memptr, "value_mem"); 
    auto reg_memptr = builder_.CreateGEP(reg, indices, "reg_memptr");
    builder_.CreateStore(value_mem, reg_memptr);
}

void Compiler::SetTop(int reg) {
    auto top = GetValueR(reg, "top");
    SetField(values_.state, top, offsetof(lua_State, top), "top");
}

void Compiler::UpdateStack() {
    values_.func = LoadField(values_.ci, rt_->GetType("TValue"),
            offsetof(CallInfo, func), "func");
}

}

