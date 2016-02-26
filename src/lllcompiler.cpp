/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllcompiler.cpp
*/

#include <llvm/ADT/StringRef.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>

#include "lllarith.h"
#include "lllcompiler.h"
#include "lllengine.h"
#include "lllruntime.h"
#include "lllvararg.h"

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

Compiler::Compiler(Proto* proto) :
    cs_(proto),
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
    CompileInstructions();
    return VerifyModule() && CreateEngine();
}

const std::string& Compiler::GetErrorMessage() {
    return error_;
}

Engine* Compiler::GetEngine() {
    return engine_.release();
}

void Compiler::CompileInstructions() {
    for (cs_.curr_ = 0; cs_.curr_ < cs_.proto_->sizecode; ++cs_.curr_) {
        cs_.builder_.SetInsertPoint(cs_.blocks_[cs_.curr_]);
        cs_.instr_ = cs_.proto_->code[cs_.curr_];
        cs_.UpdateStack();
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
            case OP_DIV: case OP_IDIV: case OP_BAND: case OP_BOR: case OP_BXOR: 
            case OP_SHL: case OP_SHR:
                              Arith::Compile(cs_); break;
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
            case OP_VARARG:   Vararg::Compile(cs_); break;
            case OP_EXTRAARG: /* ignored */ break;
        }
        if (!cs_.blocks_[cs_.curr_]->getTerminator())
            cs_.builder_.CreateBr(cs_.blocks_[cs_.curr_ + 1]);
    }
}

bool Compiler::VerifyModule() {
    llvm::raw_string_ostream error_os(error_);
    bool err = llvm::verifyModule(*cs_.module_, &error_os);
    if (err)
        cs_.module_->dump();
    return !err;
}

bool Compiler::CreateEngine() {
    auto module = cs_.module_.get();
    auto engine = llvm::EngineBuilder(cs_.module_.release())
            .setErrorStr(&error_)
            .setOptLevel(OPT_LEVEL)
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
    auto ra = cs_.GetValueR(GETARG_A(cs_.instr_), "ra");
    auto rb = cs_.GetValueR(GETARG_B(cs_.instr_), "rb");
    cs_.SetRegister(ra, rb);
}

void Compiler::CompileLoadk(bool extraarg) {
    int kposition = extraarg ? GETARG_Ax(cs_.proto_->code[cs_.curr_ + 1])
                             : GETARG_Bx(cs_.instr_);
    auto ra = cs_.GetValueR(GETARG_A(cs_.instr_), "ra");
    auto k = cs_.GetValueK(kposition, "k");
    cs_.SetRegister(ra, k);
}

void Compiler::CompileLoadbool() {
    auto ra = cs_.GetValueR(GETARG_A(cs_.instr_), "ra");
    auto value = cs_.MakeInt(GETARG_B(cs_.instr_));
    auto tag = cs_.MakeInt(LUA_TBOOLEAN);
    cs_.SetField(ra, value, offsetof(TValue, value_), "value");
    cs_.SetField(ra, tag, offsetof(TValue, tt_), "tag");
    if (GETARG_C(cs_.instr_))
        cs_.builder_.CreateBr(cs_.blocks_[cs_.curr_ + 2]);
}

void Compiler::CompileLoadnil() {
    int start = GETARG_A(cs_.instr_);
    int end = start + GETARG_B(cs_.instr_);
    for (int i = start; i <= end; ++i) {
        auto r = cs_.GetValueR(i, "r");
        cs_.SetField(r, cs_.MakeInt(LUA_TNIL), offsetof(TValue, tt_), "tag");
    }
}

void Compiler::CompileGetupval() {
    auto ra = cs_.GetValueR(GETARG_A(cs_.instr_), "ra");
    auto upval = cs_.GetUpval(GETARG_B(cs_.instr_));
    cs_.SetRegister(ra, upval);
}

void Compiler::CompileGettabup() {
    auto args = {
        cs_.values_.state,
        cs_.GetUpval(GETARG_B(cs_.instr_)),
        cs_.GetValueRK(GETARG_C(cs_.instr_), "rkc"),
        cs_.GetValueR(GETARG_A(cs_.instr_), "ra")
    };
    cs_.CreateCall("LLLGetTable", args);
}

void Compiler::CompileGettable() {
    auto args = {
        cs_.values_.state,
        cs_.GetValueR(GETARG_B(cs_.instr_), "rb"),
        cs_.GetValueRK(GETARG_C(cs_.instr_), "rkc"),
        cs_.GetValueR(GETARG_A(cs_.instr_), "ra")
    };
    cs_.CreateCall("LLLGetTable", args);
}

void Compiler::CompileSettabup() {
    auto args = {
        cs_.values_.state,
        cs_.GetUpval(GETARG_A(cs_.instr_)), 
        cs_.GetValueRK(GETARG_B(cs_.instr_), "rkb"),
        cs_.GetValueRK(GETARG_C(cs_.instr_), "rkc")
    };
    cs_.CreateCall("LLLSetTable", args);
}

void Compiler::CompileSetupval() {
    auto upvals = cs_.GetFieldPtr(cs_.values_.closure, cs_.rt_.GetType("UpVal"),
            offsetof(LClosure, upvals), "upvals");
    auto upvalptr = cs_.builder_.CreateGEP(upvals,
            cs_.MakeInt(GETARG_B(cs_.instr_)), "upvalptr");
    auto upval = cs_.builder_.CreateLoad(upvalptr);
    auto v = cs_.LoadField(upval, cs_.rt_.GetType("TValue"), offsetof(UpVal, v),
            "v");
    auto ra = cs_.GetValueR(GETARG_A(cs_.instr_), "ra");
    cs_.SetRegister(v, ra);
    cs_.CreateCall("lll_upvalbarrier", {cs_.values_.state, upval});
}

void Compiler::CompileSettable() {
    auto args = {
        cs_.values_.state,
        cs_.GetValueR(GETARG_A(cs_.instr_), "ra"),
        cs_.GetValueRK(GETARG_B(cs_.instr_), "rkb"),
        cs_.GetValueRK(GETARG_C(cs_.instr_), "rkc")
    };
    cs_.CreateCall("LLLSetTable", args);
}

void Compiler::CompileNewtable() {
    int a = GETARG_A(cs_.instr_);
    int b = GETARG_B(cs_.instr_);
    int c = GETARG_C(cs_.instr_);
    auto args = {cs_.values_.state, cs_.GetValueR(a, "ra")};
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
    CompileCheckcg(cs_.GetValueR(a + 1, "ra1"));
}

void Compiler::CompileSelf() {
    auto args = {
        cs_.values_.state,
        cs_.GetValueR(GETARG_A(cs_.instr_), "ra"),
        cs_.GetValueR(GETARG_B(cs_.instr_), "rb"),
        cs_.GetValueRK(GETARG_C(cs_.instr_), "rkc")
    };
    cs_.CreateCall("LLLSelf", args);
}

void Compiler::CompileBinop(const std::string& function) {
    auto args = {
        cs_.values_.state,
        cs_.GetValueR(GETARG_A(cs_.instr_), "ra"),
        cs_.GetValueRK(GETARG_B(cs_.instr_), "rkb"),
        cs_.GetValueRK(GETARG_C(cs_.instr_), "rkc")
    };
    cs_.CreateCall(function, args);
}

void Compiler::CompileUnop(const std::string& function) {
    auto args = {
        cs_.values_.state,
        cs_.GetValueR(GETARG_A(cs_.instr_), "ra"),
        cs_.GetValueRK(GETARG_B(cs_.instr_), "rkb")
    };
    cs_.CreateCall(function, args);
}

void Compiler::CompileConcat() {
    int a = GETARG_A(cs_.instr_);
    int b = GETARG_B(cs_.instr_);
    int c = GETARG_C(cs_.instr_);
    cs_.SetTop(c + 1);
    auto args = {cs_.values_.state, cs_.MakeInt(c - b + 1)};
    cs_.CreateCall("luaV_concat", args);

    auto ra = cs_.GetValueR(a, "ra");
    auto rb = cs_.GetValueR(b, "rb");
    cs_.SetRegister(ra, rb);

    if (a >= b)
        CompileCheckcg(cs_.GetValueR(a + 1, "ra_next"));
    else
        CompileCheckcg(rb);

    cs_.ReloadTop();
}

void Compiler::CompileJmp() {
    cs_.builder_.CreateBr(cs_.blocks_[cs_.curr_ + GETARG_sBx(cs_.instr_) + 1]);
}

void Compiler::CompileCmp(const std::string& function) {
    auto args = {
        cs_.values_.state,
        cs_.GetValueRK(GETARG_B(cs_.instr_), "rkb"),
        cs_.GetValueRK(GETARG_C(cs_.instr_), "rkc")
    };
    auto result = cs_.CreateCall(function, args, "result");
    auto a = cs_.MakeInt(GETARG_A(cs_.instr_));
    auto cmp = cs_.builder_.CreateICmpNE(result, a, "cmp");
    auto nextblock = cs_.blocks_[cs_.curr_ + 2];
    auto jmpblock = cs_.blocks_[cs_.curr_ + 1];
    cs_.builder_.CreateCondBr(cmp, nextblock, jmpblock);
}

void Compiler::CompileTest() {
    auto args = {
        cs_.MakeInt(GETARG_C(cs_.instr_)),
        cs_.GetValueR(GETARG_A(cs_.instr_), "ra")
    };
    auto test = cs_.ToBool(cs_.CreateCall("lll_test", args, "test"));
    auto nextblock = cs_.blocks_[cs_.curr_ + 2];
    auto jmpblock = cs_.blocks_[cs_.curr_ + 1];
    cs_.builder_.CreateCondBr(test, nextblock, jmpblock);
}

void Compiler::CompileTestset() {
    auto rb = cs_.GetValueR(GETARG_B(cs_.instr_), "rb");
    auto args = {cs_.MakeInt(GETARG_C(cs_.instr_)), rb};
    auto result = cs_.ToBool(cs_.CreateCall("lll_test", args, "result"));
    auto setblock = cs_.CreateSubBlock("set");
    cs_.builder_.CreateCondBr(result, cs_.blocks_[cs_.curr_ + 2], setblock);
    cs_.builder_.SetInsertPoint(setblock);
    auto ra = cs_.GetValueR(GETARG_A(cs_.instr_), "ra");
    cs_.SetRegister(ra, rb);
    cs_.builder_.CreateBr(cs_.blocks_[cs_.curr_ + 1]);
}

void Compiler::CompileCall() {
    int a = GETARG_A(cs_.instr_);
    int b = GETARG_B(cs_.instr_);
    if (b != 0)
        cs_.SetTop(a + b);
    auto args = {
        cs_.values_.state,
        cs_.GetValueR(a, "ra"),
        cs_.MakeInt(GETARG_C(cs_.instr_) - 1)
    };
    cs_.CreateCall("luaD_callnoyield", args);
}

void Compiler::CompileTailcall() {
    // Tailcall returns a negative value that signals the call must be performed
    if (cs_.proto_->sizep > 0)
        cs_.CreateCall("luaF_close", {cs_.values_.state, cs_.values_.base});
    int a = GETARG_A(cs_.instr_);
    int b = GETARG_B(cs_.instr_);
    if (b != 0)
        cs_.SetTop(a + b);
    auto diff = cs_.TopDiff(a);
    auto ret = cs_.builder_.CreateNeg(diff, "ret");
    cs_.builder_.CreateRet(ret);
}

void Compiler::CompileReturn() {
    if (cs_.proto_->sizep > 0)
        cs_.CreateCall("luaF_close", {cs_.values_.state, cs_.values_.base});
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
    cs_.builder_.CreateRet(nresults);
}

void Compiler::CompileForloop() {
    auto ra = cs_.GetValueR(GETARG_A(cs_.instr_), "ra");
    auto jump = cs_.ToBool(cs_.CreateCall("lll_forloop", {ra}, "jump"));
    auto jumpblock = cs_.blocks_[cs_.curr_ + 1 + GETARG_sBx(cs_.instr_)];
    cs_.builder_.CreateCondBr(jump, jumpblock, cs_.blocks_[cs_.curr_ + 1]);
}

void Compiler::CompileForprep() {
    auto args = {cs_.values_.state, cs_.GetValueR(GETARG_A(cs_.instr_), "ra")};
    cs_.CreateCall("lll_forprep", args);
    cs_.builder_.CreateBr(cs_.blocks_[cs_.curr_ + 1 + GETARG_sBx(cs_.instr_)]);
}

void Compiler::CompileTforcall() {
    int a = GETARG_A(cs_.instr_);
    int cb = a + 3;
    cs_.SetRegister(cs_.GetValueR(cb + 2, "cb2"), cs_.GetValueR(a + 2, "ra2"));
    cs_.SetRegister(cs_.GetValueR(cb + 1, "cb1"), cs_.GetValueR(a + 1, "ra1"));
    cs_.SetRegister(cs_.GetValueR(cb, "cb"), cs_.GetValueR(a, "ra"));
    cs_.SetTop(cb + 3);
    auto args = {
        cs_.values_.state,
        cs_.GetValueR(cb, "cb"),
        cs_.MakeInt(GETARG_C(cs_.instr_))
    };
    cs_.CreateCall("luaD_callnoyield", args);
    cs_.ReloadTop();
}

void Compiler::CompileTforloop() {
    int a = GETARG_A(cs_.instr_);
    auto ra1 = cs_.GetValueR(a + 1, "ra1");
    auto tag = cs_.LoadField(ra1, cs_.rt_.MakeIntT(sizeof(int)),
            offsetof(TValue, tt_), "tag");
    auto notnil = cs_.builder_.CreateICmpNE(tag, cs_.MakeInt(LUA_TNIL),
            "notnil");
    auto continueblock = cs_.CreateSubBlock("continue");
    auto jmpblock = cs_.blocks_[cs_.curr_ + 1];
    cs_.builder_.CreateCondBr(notnil, continueblock, jmpblock);

    cs_.builder_.SetInsertPoint(continueblock);
    auto ra = cs_.GetValueR(a, "ra");
    cs_.SetRegister(ra, ra1);
    cs_.builder_.CreateBr(cs_.blocks_[cs_.curr_ + 1 + GETARG_sBx(cs_.instr_)]);
}

void Compiler::CompileSetlist() {

    int a = GETARG_A(cs_.instr_);
    int b = GETARG_B(cs_.instr_);
    int c = GETARG_C(cs_.instr_);
    if (c == 0)
        c = GETARG_Ax(cs_.proto_->code[cs_.curr_ + 1]);

    auto n = (b != 0 ? cs_.MakeInt(b) : cs_.TopDiff(a + 1));
    auto fields = cs_.MakeInt((c - 1) * LFIELDS_PER_FLUSH);

    auto args = {cs_.values_.state, cs_.GetValueR(a, "ra"), fields, n};
    cs_.CreateCall("lll_setlist", args);
    cs_.ReloadTop();
}

void Compiler::CompileClosure() {
    auto args = {
        cs_.values_.state,
        cs_.values_.closure,
        cs_.values_.base,
        cs_.GetValueR(GETARG_A(cs_.instr_), "ra"),
        cs_.MakeInt(GETARG_Bx(cs_.instr_))
    };
    cs_.CreateCall("lll_closure", args);
    CompileCheckcg(cs_.GetValueR(GETARG_A(cs_.instr_) + 1, "ra1"));
}

void Compiler::CompileCheckcg(llvm::Value* reg) {
    auto args = {cs_.values_.state, cs_.values_.ci, reg};
    cs_.CreateCall("lll_checkcg", args);
}

}

