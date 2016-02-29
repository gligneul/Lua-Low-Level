/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** llllogical.cpp
*/

#include "llllogical.h"

extern "C" {
#include "lprefix.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lvm.h"
}

namespace lll {

Logical::Logical(CompilerState& cs) :
    Opcode(cs),
    ra_(cs_.GetValueR(GETARG_A(cs_.instr_), "ra")),
    b_(cs, GETARG_B(cs_.instr_), "rb", Value::STRING_TO_FLOAT),
    c_(cs, GETARG_C(cs_.instr_), "rc", Value::STRING_TO_FLOAT) {
    assert(GET_OPCODE(cs_.instr_) == OP_BAND ||
           GET_OPCODE(cs_.instr_) == OP_BOR ||
           GET_OPCODE(cs_.instr_) == OP_BXOR ||
           GET_OPCODE(cs_.instr_) == OP_SHL ||
           GET_OPCODE(cs_.instr_) == OP_SHR);
}

std::vector<Logical::CompilationStep> Logical::GetSteps() {
    return {
        &Logical::ComputeInteger,
        &Logical::ComputeTaggedMethod
    };
}

llvm::BasicBlock* Logical::ComputeInteger(llvm::BasicBlock* entry) {
    auto checkc = cs_.CreateSubBlock("checkc", entry);
    auto compute = cs_.CreateSubBlock("compute", checkc);
    auto failed = cs_.CreateSubBlock("failed", compute);
    auto end = cs_.blocks_[cs_.curr_ + 1];

    auto inttag = cs_.MakeInt(LUA_TNUMINT);
    auto b_is_int = cs_.builder_.CreateICmpEQ(b_.GetTag(), inttag);
    cs_.builder_.CreateCondBr(b_is_int, checkc, failed);

    cs_.builder_.SetInsertPoint(checkc);
    auto c_is_int = cs_.builder_.CreateICmpEQ(c_.GetTag(), inttag);
    cs_.builder_.CreateCondBr(c_is_int, compute, failed);

    cs_.builder_.SetInsertPoint(compute);
    auto result = PerformIntOp(b_.GetInteger(), c_.GetInteger());
    cs_.SetField(ra_, result, offsetof(TValue, value_), "value");
    cs_.SetField(ra_, inttag, offsetof(TValue, tt_), "tag");
    cs_.builder_.CreateBr(end);

    return failed;
}

llvm::BasicBlock* Logical::ComputeTaggedMethod(llvm::BasicBlock* entry) {
    auto args = {
        cs_.values_.state,
        b_.GetTValue(),
        c_.GetTValue(),
        ra_,
        cs_.MakeInt(GetMethodTag())
    };
    cs_.CreateCall("luaT_trybinTM", args);
    return entry;
}

llvm::Value* Logical::PerformIntOp(llvm::Value* lhs, llvm::Value* rhs) {
    switch (GET_OPCODE(cs_.instr_)) {
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

int Logical::GetMethodTag() {
    switch (GET_OPCODE(cs_.instr_)) {
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

