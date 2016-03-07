/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** llllogical.cpp
*/

#include "lllcompilerstate.h"
#include "llllogical.h"
#include "lllvalue.h"

extern "C" {
#include "lprefix.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lvm.h"
}

namespace lll {

Logical::Logical(CompilerState& cs, Stack& stack) :
    Opcode(cs),
    ra_(stack.GetR(GETARG_A(cs.instr_))),
    rkb_(stack.GetRK(GETARG_B(cs.instr_))),
    rkc_(stack.GetRK(GETARG_C(cs.instr_))),
    trytm_(cs.CreateSubBlock("trytm")) {
    
    assert(GET_OPCODE(cs.instr_) == OP_BAND ||
           GET_OPCODE(cs.instr_) == OP_BOR ||
           GET_OPCODE(cs.instr_) == OP_BXOR ||
           GET_OPCODE(cs.instr_) == OP_SHL ||
           GET_OPCODE(cs.instr_) == OP_SHR);
}

void Logical::Compile() {
    ComputeInteger();
    ComputeTaggedMethod();
}

void Logical::ComputeInteger() {
    auto checkrc = cs_.CreateSubBlock("checkc", entry_);
    auto compute = cs_.CreateSubBlock("compute", checkrc);

    cs_.B_.SetInsertPoint(entry_);
    cs_.B_.CreateCondBr(rkb_.HasTag(LUA_TNUMINT), checkrc, trytm_);

    cs_.B_.SetInsertPoint(checkrc);
    cs_.B_.CreateCondBr(rkc_.HasTag(LUA_TNUMINT), compute, trytm_);

    cs_.B_.SetInsertPoint(compute);
    ra_.SetInteger(PerformIntOp(rkb_.GetInteger(), rkc_.GetInteger()));
    cs_.B_.CreateBr(exit_);
}

void Logical::ComputeTaggedMethod() {
    cs_.B_.SetInsertPoint(trytm_);
    auto args = {
        cs_.values_.state,
        rkb_.GetTValue(),
        rkc_.GetTValue(),
        ra_.GetTValue(),
        cs_.MakeInt(GetMethodTag())
    };
    cs_.CreateCall("luaT_trybinTM", args);
    cs_.UpdateStack();
    cs_.B_.CreateBr(exit_);
}

llvm::Value* Logical::PerformIntOp(llvm::Value* a, llvm::Value* b) {
    auto name = "result";
    switch (GET_OPCODE(cs_.instr_)) {
        case OP_BAND:
            return cs_.B_.CreateAnd(a, b, name);
        case OP_BOR:
            return cs_.B_.CreateOr(a, b, name);
        case OP_BXOR:
            return cs_.B_.CreateXor(a, b, name);
        case OP_SHL:
            return cs_.CreateCall("luaV_shiftl", {a, b}, name);
        case OP_SHR:
            return cs_.CreateCall("luaV_shiftl", {a, cs_.B_.CreateNeg(b)}, name);
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

