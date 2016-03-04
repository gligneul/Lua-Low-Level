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
    ra_(cs, GETARG_A(cs_.instr_), "ra"),
    rb_(cs, GETARG_B(cs_.instr_), "rb", Value::STRING_TO_FLOAT),
    rc_(cs, GETARG_C(cs_.instr_), "rc", Value::STRING_TO_FLOAT),
    trytm_(cs.CreateSubBlock("trytm")) {
    
    assert(GET_OPCODE(cs.instr_) == OP_BAND ||
           GET_OPCODE(cs.instr_) == OP_BOR ||
           GET_OPCODE(cs.instr_) == OP_BXOR ||
           GET_OPCODE(cs.instr_) == OP_SHL ||
           GET_OPCODE(cs.instr_) == OP_SHR);
}

std::vector<Logical::CompilationStep> Logical::GetSteps() {
    return {
        &Logical::ComputeInteger,
        &Logical::ComputeTaggedMethod
    };
}

void Logical::ComputeInteger() {
    auto checkrc = cs_.CreateSubBlock("checkc", entry_);
    auto compute = cs_.CreateSubBlock("compute", checkrc);

    B_.SetInsertPoint(entry_);
    B_.CreateCondBr(rb_.IsInteger(), checkrc, trytm_);

    B_.SetInsertPoint(checkrc);
    B_.CreateCondBr(rc_.IsInteger(), compute, trytm_);

    B_.SetInsertPoint(compute);
    ra_.SetInteger(PerformIntOp(rb_.GetInteger(), rc_.GetInteger()));
    B_.CreateBr(exit_);
}

void Logical::ComputeTaggedMethod() {
    B_.SetInsertPoint(trytm_);
    auto args = {
        cs_.values_.state,
        rb_.GetTValue(),
        rc_.GetTValue(),
        ra_.GetTValue(),
        cs_.MakeInt(GetMethodTag())
    };
    cs_.CreateCall("luaT_trybinTM", args);
    cs_.UpdateStack();
    B_.CreateBr(exit_);
}

llvm::Value* Logical::PerformIntOp(llvm::Value* a, llvm::Value* b) {
    auto name = "result";
    switch (GET_OPCODE(cs_.instr_)) {
        case OP_BAND:
            return B_.CreateAnd(a, b, name);
        case OP_BOR:
            return B_.CreateOr(a, b, name);
        case OP_BXOR:
            return B_.CreateXor(a, b, name);
        case OP_SHL:
            return cs_.CreateCall("luaV_shiftl", {a, b}, name);
        case OP_SHR:
            return cs_.CreateCall("luaV_shiftl", {a, B_.CreateNeg(b)}, name);
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

