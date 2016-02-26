/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllarith.h
** Compiles the arithmetics opcodes
*/

#include "lllarith.h"

extern "C" {
#include "lprefix.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lvm.h"
}

namespace lll {

Arith::Arith(CompilerState& cs) :
    cs_(cs),
    ra_(GetRegister(GETARG_A(cs_.instr_), "ra")),
    rb_(GetRegister(GETARG_B(cs_.instr_), "rb")),
    rc_(GetRegister(GETARG_C(cs_.instr_), "rc")) {
}

std::vector<Arith::CompilationStep> Arith::GetSteps() {
    return {
        &Arith::ComputeInteger,
        &Arith::ComputeFloat,
        &Arith::ComputeTaggedMethod
    };
}

llvm::BasicBlock* Arith::ComputeInteger(llvm::BasicBlock* entry) {
    if (!HasIntegerOp())
        return entry;

    int b = GETARG_B(cs_.instr_);
    int c = GETARG_C(cs_.instr_);
    if (!CanPerformIntegerOp(b) || !CanPerformIntegerOp(c))
        return entry;

    auto checkc = cs_.CreateSubBlock("int.checkc", entry);
    auto compute = cs_.CreateSubBlock("int.compute", checkc);
    auto failed = cs_.CreateSubBlock("int.failed", compute);
    auto end = cs_.blocks_[cs_.curr_ + 1];

    cs_.builder_.CreateCondBr(IsInteger(b, rb_), checkc, failed);

    cs_.builder_.SetInsertPoint(checkc);
    cs_.builder_.CreateCondBr(IsInteger(c, rc_), compute, failed);

    cs_.builder_.SetInsertPoint(compute);
    auto ra = cs_.GetValueR(GETARG_A(cs_.instr_), "ra");
    auto ib = LoadInteger(b, rb_);
    auto ic = LoadInteger(c, rc_);
    auto result = PerformIntOp(ib, ic);
    cs_.SetField(ra, result, offsetof(TValue, value_), "value");
    auto tag = cs_.MakeInt(LUA_TNUMINT);
    cs_.SetField(ra, tag, offsetof(TValue, tt_), "tag");
    cs_.builder_.CreateBr(end);

    return failed;
}

llvm::BasicBlock* Arith::ComputeFloat(llvm::BasicBlock* entry) {
    if (!HasFloatOp())
        return entry;

    int b = GETARG_B(cs_.instr_);
    int c = GETARG_C(cs_.instr_);
    if (!CanPerformFloatOp(b) || !CanPerformFloatOp(c))
        return entry;

    auto checkc = cs_.CreateSubBlock("float.checkc", entry);
    auto compute = cs_.CreateSubBlock("float.compute", checkc);
    auto failed = cs_.CreateSubBlock("float.failed", compute);
    auto end = cs_.blocks_[cs_.curr_ + 1];

    auto fb = ConvertToFloat(b, rb_);
    cs_.builder_.CreateCondBr(fb.first, checkc, failed);

    cs_.builder_.SetInsertPoint(checkc);
    auto fc = ConvertToFloat(c, rc_);
    cs_.builder_.CreateCondBr(fc.first, compute, failed);

    cs_.builder_.SetInsertPoint(compute);
    auto ra = cs_.GetValueR(GETARG_A(cs_.instr_), "ra");
    auto result = PerformFloatOp(fb.second, fc.second);
    cs_.SetField(ra, result, offsetof(TValue, value_), "value");
    auto tag = cs_.MakeInt(LUA_TNUMFLT);
    cs_.SetField(ra, tag, offsetof(TValue, tt_), "tag");
    cs_.builder_.CreateBr(end);

    return failed;
}

llvm::BasicBlock* Arith::ComputeTaggedMethod(llvm::BasicBlock* entry) {
    auto args = {
        cs_.values_.state,
        rb_ ? rb_ : cs_.GetValueRK(GETARG_B(cs_.instr_), "kb"),
        rc_ ? rc_ : cs_.GetValueRK(GETARG_C(cs_.instr_), "kc"),
        ra_,
        cs_.MakeInt(GetMethodTag())
    };
    cs_.CreateCall("luaT_trybinTM", args);
    return entry;
}

llvm::Value* Arith::GetRegister(int id, const char* name) {
    if (ISK(id))
        return nullptr;
    else
        return cs_.GetValueR(id, name);
}

bool Arith::HasIntegerOp() {
    switch (GET_OPCODE(cs_.instr_)) {
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_MOD: case OP_IDIV:
        case OP_BAND: case OP_BOR: case OP_BXOR: case OP_SHL: case OP_SHR:
            return true;
        default:
            break;
    }
    return false;
}

bool Arith::HasFloatOp() {
    switch (GET_OPCODE(cs_.instr_)) {
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_POW:
        case OP_MOD: case OP_IDIV:
            return true;
        default:
            break;
    }
    return false;
}

bool Arith::CanPerformIntegerOp(int v) {
    if (ISK(v)) {
        auto k = cs_.proto_->k + INDEXK(v);
        return ttisinteger(k);
    } else {
        return true;
    }
}

bool Arith::CanPerformFloatOp(int v) {
    if (ISK(v)) {
        auto k = cs_.proto_->k + INDEXK(v);
        TValue _;
        return ttisnumber(k) ||
               (cvt2num(k) && luaO_str2num(svalue(k), &_) == vslen(k) + 1);
    } else {
        return true;
    }
}

llvm::Value* Arith::IsInteger(int v, llvm::Value* reg) {
    if (ISK(v)) {
        return llvm::ConstantInt::getTrue(cs_.context_);
    } else {
        auto tag = cs_.LoadField(reg, cs_.rt_.MakeIntT(sizeof(int)),
                offsetof(TValue, tt_), "tag");
        return cs_.builder_.CreateICmpEQ(tag, cs_.MakeInt(LUA_TNUMINT));
    }
}

llvm::Value* Arith::LoadInteger(int v, llvm::Value* reg) {
    auto intt = cs_.rt_.GetType("lua_Integer");
    if (ISK(v)) {
        auto k = cs_.proto_->k + INDEXK(v);
        return llvm::ConstantInt::get(intt, ivalue(k));
    } else {
        return cs_.LoadField(reg, intt, offsetof(TValue, value_), "value");
    }
}

std::pair<llvm::Value*, llvm::Value*> Arith::ConvertToFloat(int v,
        llvm::Value* reg) {
    auto floatt = cs_.rt_.GetType("lua_Number");
    if (ISK(v)) {
        auto k = cs_.proto_->k + INDEXK(v);
        lua_Number n;
        if (ttisnumber(k)) {
            n = nvalue(k);
        } else {
            // Must be a string
            TValue converted;
            luaO_str2num(svalue(k), &converted);
            n = nvalue(&converted);
        }
        return {
            llvm::ConstantInt::getTrue(cs_.context_),
            llvm::ConstantFP::get(floatt, n)
        };
    } else {
        auto n = cs_.values_.luanumber;
        auto isfloat = cs_.CreateCall("LLLToNumber", {reg, n}, "is.float");
        auto boolt = llvm::Type::getInt1Ty(cs_.context_);
        return {
            cs_.builder_.CreateIntCast(isfloat, boolt, false),
            cs_.builder_.CreateLoad(n, "n")
        };
    }
}

llvm::Value* Arith::PerformIntOp(llvm::Value* lhs, llvm::Value* rhs) {
    switch (GET_OPCODE(cs_.instr_)) {
        case OP_ADD:
            return cs_.builder_.CreateAdd(lhs, rhs, "result");
        case OP_SUB:
            return cs_.builder_.CreateSub(lhs, rhs, "result");
        case OP_MUL:
            return cs_.builder_.CreateMul(lhs, rhs, "result");
        case OP_MOD:
            return cs_.CreateCall("luaV_mod", {cs_.values_.state, lhs, rhs},
                    "result");
        case OP_IDIV:
            return cs_.CreateCall("luaV_div", {cs_.values_.state, lhs, rhs},
                    "result");
        case OP_BAND:
            return cs_.builder_.CreateAnd(lhs, rhs, "result");
        case OP_BOR:
            return cs_.builder_.CreateOr(lhs, rhs, "result");
        case OP_BXOR:
            return cs_.builder_.CreateXor(lhs, rhs, "result");
        case OP_SHL:
            return cs_.CreateCall("luaV_shiftl", {lhs, rhs}, "result");
        case OP_SHR:
            return cs_.CreateCall("luaV_shiftl",
                    {lhs, cs_.builder_.CreateNeg(rhs, "neg.ic")}, "result");
        default:
            break;
    }
    assert(false);
    return nullptr;
}

llvm::Value* Arith::PerformFloatOp(llvm::Value* lhs, llvm::Value* rhs) {
    switch (GET_OPCODE(cs_.instr_)) {
        case OP_ADD:
            return cs_.builder_.CreateFAdd(lhs, rhs, "result");
        case OP_SUB:
            return cs_.builder_.CreateFSub(lhs, rhs, "result");
        case OP_MUL:
            return cs_.builder_.CreateFMul(lhs, rhs, "result");
        case OP_MOD:
            return cs_.CreateCall("LLLNumMod", {lhs, rhs}, "result");
        case OP_POW:
            return cs_.CreateCall(STRINGFY2(l_mathop(pow)), {lhs, rhs},
                    "result");
        case OP_DIV:
            return cs_.builder_.CreateFDiv(lhs, rhs, "result");
        case OP_IDIV:
            return cs_.CreateCall(STRINGFY2(l_mathop(floor)),
                    {cs_.builder_.CreateFDiv(lhs, rhs, "result")}, "floor");
        default:
            break;
    }
    assert(false);
    return nullptr;
}

int Arith::GetMethodTag() {
    switch (GET_OPCODE(cs_.instr_)) {
        case OP_ADD:    return TM_ADD;
        case OP_SUB:    return TM_SUB;
        case OP_MUL:    return TM_MUL;
        case OP_MOD:    return TM_MOD;
        case OP_POW:    return TM_POW;
        case OP_DIV:    return TM_DIV;
        case OP_IDIV:   return TM_IDIV;
        case OP_BAND:   return TM_BAND;
        case OP_BOR:    return TM_BOR;
        case OP_BXOR:   return TM_BXOR;
        case OP_SHL:    return TM_SHL;
        case OP_SHR:    return TM_SHR;
        default: break;
    }
    assert(false);
    return -1;
}

}

