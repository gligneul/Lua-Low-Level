/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllcompiler.cpp
*/

#include <llvm/ADT/StringRef.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/Verifier.h>
#include <llvm/PassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>

#define LLL_USE_MCJIT
#ifdef LLL_USE_MCJIT
#include <llvm/ExecutionEngine/MCJIT.h>
#else
#include <llvm/ExecutionEngine/JIT.h>
#endif

#include "lllarith.h"
#include "lllcompiler.h"
#include "lllengine.h"
#include "llllogical.h"
#include "lllruntime.h"
#include "llltableget.h"
#include "llltableset.h"
#include "lllvararg.h"

extern "C" {
#include "lprefix.h"
#include "lfunc.h"
#include "lgc.h"
#include "lopcodes.h"
#include "ltable.h"
#include "luaconf.h"
#include "lvm.h"
}

static const llvm::CodeGenOpt::Level OPT_LEVEL =
        //llvm::CodeGenOpt::None;
        //llvm::CodeGenOpt::Default;
        llvm::CodeGenOpt::Aggressive;

namespace lll {

Compiler::Compiler(lua_State* L, Proto* proto) :
    cs_(L, proto),
    stack_(cs_),
    engine_(nullptr) {
    static bool init = true;
    if (init) {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();
        init = false;
    }
}

bool Compiler::Compile() {
    return CompileInstructions() &&
           VerifyModule() &&
           OptimizeModule() &&
           CreateEngine();
}

const std::string& Compiler::GetErrorMessage() {
    return error_;
}

Engine* Compiler::GetEngine() {
    return engine_.release();
}

bool Compiler::CompileInstructions() {
    cs_.InitEntryBlock();
    stack_.InitValues();
    cs_.B_.CreateBr(cs_.blocks_[0]);

    for (cs_.curr_ = 0; cs_.curr_ < cs_.proto_->sizecode; ++cs_.curr_) {
        cs_.B_.SetInsertPoint(cs_.blocks_[cs_.curr_]);
        cs_.instr_ = cs_.proto_->code[cs_.curr_];
        //cs_.DebugPrint(luaP_opnames[GET_OPCODE(cs_.instr_)]);
        switch (GET_OPCODE(cs_.instr_)) {
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
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_MOD: case OP_POW:  
            case OP_DIV: case OP_IDIV:
                              Arith(cs_, stack_).Compile(); break;
            case OP_BAND: case OP_BOR: case OP_BXOR: case OP_SHL: case OP_SHR:
                              Logical(cs_, stack_).Compile(); break;
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
            case OP_TAILCALL: CompileTailcall(); break;
            case OP_RETURN:   CompileReturn(); break;
            case OP_FORLOOP:  CompileForloop(); break;
            case OP_FORPREP:  CompileForprep(); break;
            case OP_TFORCALL: CompileTforcall(); break;
            case OP_TFORLOOP: CompileTforloop(); break;
            case OP_SETLIST:  CompileSetlist(); break;
            case OP_CLOSURE:  CompileClosure(); break;
            case OP_VARARG:   Vararg(cs_, stack_).Compile(); break;
            case OP_EXTRAARG: /* ignored */ break;
        }
        if (!cs_.blocks_[cs_.curr_]->getTerminator())
            cs_.B_.CreateBr(cs_.blocks_[cs_.curr_ + 1]);
    }
    return true;
}

bool Compiler::VerifyModule() {
    llvm::raw_string_ostream error_os(error_);
    bool err = llvm::verifyModule(*cs_.module_, &error_os);
    if (err) {
        cs_.module_->dump();
    }
    return !err;
}

bool Compiler::OptimizeModule() {
    llvm::FunctionPassManager fpm(cs_.module_.get());
    fpm.add(llvm::createGVNPass()); // required by SCCP Pass
    fpm.add(llvm::createPromoteMemoryToRegisterPass());
    fpm.add(llvm::createSCCPPass());
    fpm.add(llvm::createAggressiveDCEPass());
    fpm.run(*cs_.function_);
    return true;
}

bool Compiler::CreateEngine() {
    auto module = cs_.module_.get();
    auto engine = llvm::EngineBuilder(cs_.module_.release())
            .setErrorStr(&error_)
            .setOptLevel(OPT_LEVEL)
            .setEngineKind(llvm::EngineKind::JIT)
#ifdef LLL_USE_MCJIT
            .setUseMCJIT(true)
#else
            .setUseMCJIT(false)
#endif
            .create();

    if (engine) {
        engine->finalizeObject();
        engine_.reset(new Engine(engine, module, cs_.function_));
        return true;
    } else {
        return false;
    }
}

void Compiler::CompileMove() {
    auto& ra = stack_.GetR(GETARG_A(cs_.instr_));
    auto& rb = stack_.GetR(GETARG_B(cs_.instr_));
    ra.Assign(rb);
}

void Compiler::CompileLoadk(bool extraarg) {
    auto& ra = stack_.GetR(GETARG_A(cs_.instr_));
    int karg = extraarg ? GETARG_Ax(cs_.proto_->code[cs_.curr_ + 1])
                        : GETARG_Bx(cs_.instr_);
    auto& k = stack_.GetK(karg);
    ra.Assign(k);
}

void Compiler::CompileLoadbool() {
    auto& ra = stack_.GetR(GETARG_A(cs_.instr_));
    ra.SetBoolean(cs_.MakeInt(GETARG_B(cs_.instr_)));
    if (GETARG_C(cs_.instr_))
        cs_.B_.CreateBr(cs_.blocks_[cs_.curr_ + 2]);
}

void Compiler::CompileLoadnil() {
    int start = GETARG_A(cs_.instr_);
    int end = start + GETARG_B(cs_.instr_);
    for (int i = start; i <= end; ++i) {
        auto& r = stack_.GetR(i);
        r.SetTagK(LUA_TNIL);
    }
}

void Compiler::CompileGetupval() {
    auto& ra = stack_.GetR(GETARG_A(cs_.instr_));
    auto& upval = stack_.GetUp(GETARG_B(cs_.instr_));
    ra.Assign(upval);
}

void Compiler::CompileGettabup() {
    auto& table = stack_.GetUp(GETARG_B(cs_.instr_));
    auto& key = stack_.GetRK(GETARG_C(cs_.instr_));
    auto& dest = stack_.GetR(GETARG_A(cs_.instr_));
    TableGet(cs_, stack_, table, key, dest).Compile();
}

void Compiler::CompileGettable() {
    auto& table = stack_.GetR(GETARG_B(cs_.instr_));
    auto& key = stack_.GetRK(GETARG_C(cs_.instr_));
    auto& dest = stack_.GetR(GETARG_A(cs_.instr_));
    TableGet(cs_, stack_, table, key, dest).Compile();
}

void Compiler::CompileSettabup() {
    auto& table = stack_.GetUp(GETARG_A(cs_.instr_));
    auto& key = stack_.GetRK(GETARG_B(cs_.instr_));
    auto& value = stack_.GetRK(GETARG_C(cs_.instr_));
    TableSet(cs_, stack_, table, key, value).Compile();
}

void Compiler::CompileSetupval() {
    auto& upval = stack_.GetUp(GETARG_B(cs_.instr_));
    auto& ra = stack_.GetR(GETARG_A(cs_.instr_));
    upval.Assign(ra);
    cs_.CreateCall("lll_upvalbarrier", {cs_.values_.state, upval.GetUpVal()});
}

void Compiler::CompileSettable() {
    auto& table = stack_.GetR(GETARG_A(cs_.instr_));
    auto& key = stack_.GetRK(GETARG_B(cs_.instr_));
    auto& value = stack_.GetRK(GETARG_C(cs_.instr_));
    TableSet(cs_, stack_, table, key, value).Compile();
}

void Compiler::CompileNewtable() {
    int a = GETARG_A(cs_.instr_);
    int b = GETARG_B(cs_.instr_);
    int c = GETARG_C(cs_.instr_);
    auto& ra = stack_.GetR(a);
    auto args = {cs_.values_.state, ra.GetTValue()};
    auto table = cs_.CreateCall("lll_newtable", args);
    if (b != 0 || c != 0) {
        args = {
            cs_.values_.state,
            table,
            cs_.MakeInt(luaO_fb2int(b)),
            cs_.MakeInt(luaO_fb2int(c))
        };
        cs_.CreateCall("luaH_resize", args);
    }
    auto& ra1 = stack_.GetR(a + 1);
    CompileCheckcg(ra1.GetTValue());
}

void Compiler::CompileSelf() {
    auto& table = stack_.GetR(GETARG_B(cs_.instr_));
    auto& key = stack_.GetRK(GETARG_C(cs_.instr_));
    auto& methodslot = stack_.GetR(GETARG_A(cs_.instr_));
    auto& selfslot = stack_.GetR(GETARG_A(cs_.instr_) + 1);
    selfslot.Assign(table);
    TableGet(cs_, stack_, table, key, methodslot).Compile();
}

void Compiler::CompileUnop(const std::string& function) {
    auto& ra = stack_.GetR(GETARG_A(cs_.instr_));
    auto& rkb = stack_.GetRK(GETARG_B(cs_.instr_));
    auto args = {cs_.values_.state, ra.GetTValue(), rkb.GetTValue()};
    cs_.CreateCall(function, args);
    stack_.Update();
}

void Compiler::CompileConcat() {
    int a = GETARG_A(cs_.instr_);
    int b = GETARG_B(cs_.instr_);
    int c = GETARG_C(cs_.instr_);

    cs_.SetTop(c + 1);
    auto args = {cs_.values_.state, cs_.MakeInt(c - b + 1)};
    cs_.CreateCall("luaV_concat", args);
    stack_.Update();

    auto& ra = stack_.GetR(a);
    auto& rb = stack_.GetR(b);
    ra.Assign(rb);

    if (a >= b) {
        auto& ra1 = stack_.GetR(a + 1);
        CompileCheckcg(ra1.GetTValue());
    } else {
        CompileCheckcg(rb.GetTValue());
    }

    cs_.ReloadTop();
}

void Compiler::CompileJmp() {
    int a = GETARG_A(cs_.instr_);
    if (a != 0) {
        auto& r = stack_.GetR(a - 1);
        cs_.CreateCall("luaF_close", {cs_.values_.state, r.GetTValue()});
    }
    cs_.B_.CreateBr(cs_.blocks_[cs_.curr_ + GETARG_sBx(cs_.instr_) + 1]);
}

void Compiler::CompileCmp(const std::string& function) {
    auto& rkb = stack_.GetRK(GETARG_B(cs_.instr_));
    auto& rkc = stack_.GetRK(GETARG_C(cs_.instr_));
    auto args = {cs_.values_.state, rkb.GetTValue(), rkc.GetTValue()};
    auto result = cs_.CreateCall(function, args, "result");
    stack_.Update();

    auto a = cs_.MakeInt(GETARG_A(cs_.instr_));
    auto cmp = cs_.B_.CreateICmpNE(result, a, "cmp");
    auto nextblock = cs_.blocks_[cs_.curr_ + 2];
    auto jmpblock = cs_.blocks_[cs_.curr_ + 1];
    cs_.B_.CreateCondBr(cmp, nextblock, jmpblock);
}

void Compiler::CompileTest() {
    auto checkbool = cs_.CreateSubBlock("checkbool");
    auto checkfalse = cs_.CreateSubBlock("checkfalse", checkbool);
    auto success = cs_.blocks_[cs_.curr_ + 2];
    auto fail = cs_.blocks_[cs_.curr_ + 1];

    auto& r = stack_.GetR(GETARG_A(cs_.instr_));
    if (GETARG_C(cs_.instr_)) {
        auto isnil = r.HasTag(LUA_TNIL);
        cs_.B_.CreateCondBr(isnil, success, checkbool);

        cs_.B_.SetInsertPoint(checkbool);
        auto isbool = r.HasTag(LUA_TBOOLEAN);
        cs_.B_.CreateCondBr(isbool, checkfalse, fail);

        cs_.B_.SetInsertPoint(checkfalse);
        auto bvalue = r.GetBoolean();
        auto isfalse = cs_.B_.CreateICmpEQ(bvalue, cs_.MakeInt(0));
        cs_.B_.CreateCondBr(isfalse, success, fail);
    } else {
        auto isnil = r.HasTag(LUA_TNIL);
        cs_.B_.CreateCondBr(isnil, fail, checkbool);

        cs_.B_.SetInsertPoint(checkbool);
        auto isbool = r.HasTag(LUA_TBOOLEAN);
        cs_.B_.CreateCondBr(isbool, checkfalse, success);

        cs_.B_.SetInsertPoint(checkfalse);
        auto bvalue = r.GetBoolean();
        auto isfalse = cs_.B_.CreateICmpEQ(bvalue, cs_.MakeInt(0));
        cs_.B_.CreateCondBr(isfalse, fail, success);
    }
}

void Compiler::CompileTestset() {
    auto checkbool = cs_.CreateSubBlock("checkbool");
    auto checkfalse = cs_.CreateSubBlock("checkfalse", checkbool);
    auto fail = cs_.CreateSubBlock("set", checkfalse);
    auto success = cs_.blocks_[cs_.curr_ + 2];

    auto& r = stack_.GetR(GETARG_B(cs_.instr_));
    if (GETARG_C(cs_.instr_)) {
        auto isnil = r.HasTag(LUA_TNIL);
        cs_.B_.CreateCondBr(isnil, success, checkbool);

        cs_.B_.SetInsertPoint(checkbool);
        auto isbool = r.HasTag(LUA_TBOOLEAN);
        cs_.B_.CreateCondBr(isbool, checkfalse, fail);

        cs_.B_.SetInsertPoint(checkfalse);
        auto bvalue = r.GetBoolean();
        auto isfalse = cs_.B_.CreateICmpEQ(bvalue, cs_.MakeInt(0));
        cs_.B_.CreateCondBr(isfalse, success, fail);
    } else {
        auto isnil = r.HasTag(LUA_TNIL);
        cs_.B_.CreateCondBr(isnil, fail, checkbool);

        cs_.B_.SetInsertPoint(checkbool);
        auto isbool = r.HasTag(LUA_TBOOLEAN);
        cs_.B_.CreateCondBr(isbool, checkfalse, success);

        cs_.B_.SetInsertPoint(checkfalse);
        auto bvalue = r.GetBoolean();
        auto isfalse = cs_.B_.CreateICmpEQ(bvalue, cs_.MakeInt(0));
        cs_.B_.CreateCondBr(isfalse, fail, success);
    }

    cs_.B_.SetInsertPoint(fail);
    auto& ra = stack_.GetR(GETARG_A(cs_.instr_));
    ra.Assign(r);
    cs_.B_.CreateBr(cs_.blocks_[cs_.curr_ + 1]);
}

void Compiler::CompileCall() {
    int a = GETARG_A(cs_.instr_);
    int b = GETARG_B(cs_.instr_);
    if (b != 0)
        cs_.SetTop(a + b);
    auto& ra = stack_.GetR(a);
    auto args = {
        cs_.values_.state,
        ra.GetTValue(),
        cs_.MakeInt(GETARG_C(cs_.instr_) - 1)
    };
    cs_.CreateCall("luaD_callnoyield", args);
    stack_.Update();
}

void Compiler::CompileTailcall() {
    // Tailcall returns a negative value that signals the call must be performed
    if (cs_.proto_->sizep > 0)
        cs_.CreateCall("luaF_close", {cs_.values_.state, cs_.GetBase()});
    int a = GETARG_A(cs_.instr_);
    int b = GETARG_B(cs_.instr_);
    if (b != 0)
        cs_.SetTop(a + b);
    auto diff = cs_.TopDiff(a);
    auto ret = cs_.B_.CreateNeg(diff, "ret");
    cs_.B_.CreateRet(ret);
}

void Compiler::CompileReturn() {
    if (cs_.proto_->sizep > 0)
        cs_.CreateCall("luaF_close", {cs_.values_.state, cs_.GetBase()});
    int a = GETARG_A(cs_.instr_);
    int b = GETARG_B(cs_.instr_);
    llvm::Value* nresults = nullptr;
    if (b == 0) {
        nresults = cs_.TopDiff(GETARG_A(cs_.instr_));
    } else if (b == 1) { 
        nresults = cs_.MakeInt(0);
    } else {
        nresults = cs_.MakeInt(b - 1);
        cs_.SetTop(a + b - 1);
    }
    cs_.B_.CreateRet(nresults);
}

void Compiler::CompileForloop() {
    auto& ra = stack_.GetR(GETARG_A(cs_.instr_));
    auto args = {ra.GetTValue()};
    auto jump = cs_.ToBool(cs_.CreateCall("lll_forloop", args, "jump"));
    auto jumpblock = cs_.blocks_[cs_.curr_ + 1 + GETARG_sBx(cs_.instr_)];
    cs_.B_.CreateCondBr(jump, jumpblock, cs_.blocks_[cs_.curr_ + 1]);
}

void Compiler::CompileForprep() {
    auto& ra = stack_.GetR(GETARG_A(cs_.instr_));
    auto args = {cs_.values_.state, ra.GetTValue()};
    cs_.CreateCall("lll_forprep", args);
    cs_.B_.CreateBr(cs_.blocks_[cs_.curr_ + 1 + GETARG_sBx(cs_.instr_)]);
}

void Compiler::CompileTforcall() {
    int a = GETARG_A(cs_.instr_);
    int cb = a + 3;
    auto& rcb = stack_.GetR(cb);
    rcb.Assign(stack_.GetR(a));
    stack_.GetR(cb + 1).Assign(stack_.GetR(a + 1));
    stack_.GetR(cb + 2).Assign(stack_.GetR(a + 2));
    cs_.SetTop(cb + 3);
    auto args = {
        cs_.values_.state,
        rcb.GetTValue(),
        cs_.MakeInt(GETARG_C(cs_.instr_))
    };
    cs_.CreateCall("luaD_callnoyield", args);
    stack_.Update();
    cs_.ReloadTop();
}

void Compiler::CompileTforloop() {
    int a = GETARG_A(cs_.instr_);
    auto& ra1 = stack_.GetR(a + 1);
    auto isnil = ra1.HasTag(LUA_TNIL);
    auto end = cs_.blocks_[cs_.curr_ + 1];
    auto cont = cs_.CreateSubBlock("continue");
    cs_.B_.CreateCondBr(isnil, end, cont);

    cs_.B_.SetInsertPoint(cont);
    auto& ra = stack_.GetR(a);
    ra.Assign(ra1);
    cs_.B_.CreateBr(cs_.blocks_[cs_.curr_ + 1 + GETARG_sBx(cs_.instr_)]);
}

void Compiler::CompileSetlist() {
    int a = GETARG_A(cs_.instr_);
    int b = GETARG_B(cs_.instr_);
    int c = GETARG_C(cs_.instr_);
    if (c == 0)
        c = GETARG_Ax(cs_.proto_->code[cs_.curr_ + 1]);

    auto n = (b != 0 ? cs_.MakeInt(b) : cs_.TopDiff(a + 1));
    auto fields = cs_.MakeInt((c - 1) * LFIELDS_PER_FLUSH);

    auto& ra = stack_.GetR(a);
    auto args = {cs_.values_.state, ra.GetTValue(), fields, n};
    cs_.CreateCall("lll_setlist", args);
    cs_.ReloadTop();
}

void Compiler::CompileClosure() {
    int a = GETARG_A(cs_.instr_);
    auto& ra = stack_.GetR(a);
    auto args = {
        cs_.values_.state,
        cs_.values_.closure,
        cs_.GetBase(),
        ra.GetTValue(),
        cs_.MakeInt(GETARG_Bx(cs_.instr_))
    };
    cs_.CreateCall("lll_closure", args);
    auto& ra1 = stack_.GetR(a + 1);
    CompileCheckcg(ra1.GetTValue());
}

void Compiler::CompileCheckcg(llvm::Value* reg) {
    auto args = {cs_.values_.state, cs_.values_.ci, reg};
    cs_.CreateCall("lll_checkcg", args);
}

}

