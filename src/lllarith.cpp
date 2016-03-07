/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllarith.cpp
** Compiles the arithmetics opcodes
*/

#include "lllarith.h"
#include "lllcompilerstate.h"
#include "lllvalue.h"

extern "C" {
#include "lprefix.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lvm.h"
}

namespace lll {

Arith::Arith(CompilerState& cs, Stack& stack) :
    Opcode(cs),
    ra_(stack.GetR(GETARG_A(cs.instr_))),
    rkb_(stack.GetRK(GETARG_B(cs.instr_))),
    rkc_(stack.GetRK(GETARG_C(cs.instr_))),
    intop_(cs.CreateSubBlock("intop")),
    floatop_(cs.CreateSubBlock("floatop", intop_)),
    tmop_(cs.CreateSubBlock("tmop", floatop_)) {
}

void Arith::Compile() {
    SwitchTags();
    ComputeInt();
    ComputeFloat();
    ComputeTaggedMethod();
}

void Arith::SwitchTags() {
    // Creates a bblock for each possible combination of tags
    auto bint = cs_.CreateSubBlock("bint");
    auto bflt = cs_.CreateSubBlock("bflt", bint);
    auto bstr = cs_.CreateSubBlock("bstr", bflt);
    auto bint_cflt = cs_.CreateSubBlock("bint_cflt", bstr);
    auto bint_cstr = cs_.CreateSubBlock("bint_cstr", bint_cflt);
    auto bflt_cint = cs_.CreateSubBlock("bflt_cint", bint_cstr);
    auto bflt_cflt = cs_.CreateSubBlock("bflt_cflt", bflt_cint);
    auto bflt_cstr = cs_.CreateSubBlock("bflt_cstr", bflt_cflt);
    auto bstr_cint = cs_.CreateSubBlock("bstr_cint", bflt_cstr);
    auto bstr_cflt = cs_.CreateSubBlock("bstr_cflt", bstr_cint);
    auto bstr_cstr = cs_.CreateSubBlock("bstr_cstr", bstr_cflt);

    // Operands information
    cs_.B_.SetInsertPoint(entry_);
    auto btag = rkb_.GetTag();
    auto ctag = rkc_.GetTag();
    auto bnumber = cs_.values_.bnumber;
    auto cnumber = cs_.values_.cnumber;

    // Make the switches
    SwitchTagCase(rkb_, btag, bnumber, entry_, bint, bflt, bstr, true);
    SwitchTagCase(rkc_, ctag, cnumber, bint, intop_, bint_cflt, bint_cstr,
            true);
    SwitchTagCase(rkc_, ctag, cnumber, bflt, bflt_cint, bflt_cflt, bflt_cstr,
            false);
    SwitchTagCase(rkc_, ctag, cnumber, bstr, bstr_cint, bstr_cflt, bstr_cstr,
            false);

    // Set nb and nc incomings
    auto floatt = cs_.rt_.GetType("lua_Number");
    #define ITOF(v) \
        cs_.B_.CreateSIToFP(v, floatt, v->getName() + "_flt")

    #define LOAD(v) \
        cs_.B_.CreateLoad(v, v->getName() + "_loaded")

    #define SET_INCOMING(block, bvalue, cvalue) { \
        cs_.B_.SetInsertPoint(block); \
        nbinc_.push_back({(bvalue), (block)}); \
        ncinc_.push_back({(cvalue), (block)}); \
        cs_.B_.CreateBr(floatop_); }

    if (!HasIntegerOp()) {
        SET_INCOMING(intop_, ITOF(rkb_.GetInteger()), ITOF(rkc_.GetInteger()));
    }
    SET_INCOMING(bint_cflt, ITOF(rkb_.GetInteger()), rkc_.GetFloat());
    SET_INCOMING(bint_cstr, ITOF(rkb_.GetInteger()), LOAD(cnumber));
    SET_INCOMING(bflt_cint, rkb_.GetFloat(), ITOF(rkc_.GetInteger()));
    SET_INCOMING(bflt_cflt, rkb_.GetFloat(), rkc_.GetFloat());
    SET_INCOMING(bflt_cstr, rkb_.GetFloat(), LOAD(cnumber));
    SET_INCOMING(bstr_cint, LOAD(bnumber), ITOF(rkc_.GetInteger()));
    SET_INCOMING(bstr_cflt, LOAD(bnumber), rkc_.GetFloat());
    SET_INCOMING(bstr_cstr, LOAD(bnumber), LOAD(cnumber));
}

void Arith::ComputeInt() {
    if (HasIntegerOp()) {
        cs_.B_.SetInsertPoint(intop_);
        ra_.SetInteger(PerformIntOp(rkb_.GetInteger(), rkc_.GetInteger()));
        cs_.B_.CreateBr(exit_);
    }
}

void Arith::ComputeFloat() {
    cs_.B_.SetInsertPoint(floatop_);
    auto floatt = cs_.rt_.GetType("lua_Number");
    auto nb = CreatePHI(floatt, nbinc_, "nb");
    auto nc = CreatePHI(floatt, ncinc_, "nc");
    ra_.SetFloat(PerformFloatOp(nb, nc));
    cs_.B_.CreateBr(exit_);
}

void Arith::ComputeTaggedMethod() {
    cs_.B_.SetInsertPoint(tmop_);
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

bool Arith::HasIntegerOp() {
    switch (GET_OPCODE(cs_.instr_)) {
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_MOD: case OP_IDIV:
            return true;
        default:
            break;
    }
    return false;
}

void Arith::SwitchTagCase(Value& value, llvm::Value* tag, llvm::Value* convptr,
        llvm::BasicBlock* entry, llvm::BasicBlock* intop,
        llvm::BasicBlock* floatop, llvm::BasicBlock* convop, bool intfirst) {
    auto checkothertag = cs_.CreateSubBlock("checkothertag", entry);
    auto convert = cs_.CreateSubBlock("convert", checkothertag);

    auto firsttag = cs_.MakeInt(intfirst ? LUA_TNUMINT : LUA_TNUMFLT);
    auto secondtag = cs_.MakeInt(intfirst ? LUA_TNUMFLT : LUA_TNUMINT);
    auto firstop = intfirst ? intop : floatop;
    auto secondop = intfirst ? floatop : intop;

    cs_.B_.SetInsertPoint(entry);
    auto hasfirsttag = cs_.B_.CreateICmpEQ(tag, firsttag);
    cs_.B_.CreateCondBr(hasfirsttag, firstop, checkothertag);

    cs_.B_.SetInsertPoint(checkothertag);
    auto hassecondtag = cs_.B_.CreateICmpEQ(tag, secondtag);
    cs_.B_.CreateCondBr(hassecondtag, secondop, convert);

    cs_.B_.SetInsertPoint(convert);
    auto args = {value.GetTValue(), convptr};
    auto converted = cs_.CreateCall("LLLToNumber", args, "converted");
    cs_.B_.CreateCondBr(converted, convop, tmop_);
}

llvm::Value* Arith::PerformIntOp(llvm::Value* lhs, llvm::Value* rhs) {
    auto name = "result";
    switch (GET_OPCODE(cs_.instr_)) {
        case OP_ADD:
            return cs_.B_.CreateAdd(lhs, rhs, name);
        case OP_SUB:
            return cs_.B_.CreateSub(lhs, rhs, name);
        case OP_MUL:
            return cs_.B_.CreateMul(lhs, rhs, name);
        case OP_MOD:
            return cs_.CreateCall("luaV_mod", {cs_.values_.state, lhs, rhs},
                    name);
        case OP_IDIV:
            return cs_.CreateCall("luaV_div", {cs_.values_.state, lhs, rhs},
                    name);
        default:
            break;
    }
    assert(false);
    return nullptr;
}

llvm::Value* Arith::PerformFloatOp(llvm::Value* lhs, llvm::Value* rhs) {
    auto name = "result";
    switch (GET_OPCODE(cs_.instr_)) {
        case OP_ADD:
            return cs_.B_.CreateFAdd(lhs, rhs, name);
        case OP_SUB:
            return cs_.B_.CreateFSub(lhs, rhs, name);
        case OP_MUL:
            return cs_.B_.CreateFMul(lhs, rhs, name);
        case OP_MOD:
            return cs_.CreateCall("LLLNumMod", {lhs, rhs}, name);
        case OP_POW:
            return cs_.CreateCall(STRINGFY2(l_mathop(pow)), {lhs, rhs}, name);
        case OP_DIV:
            return cs_.B_.CreateFDiv(lhs, rhs, name);
        case OP_IDIV:
            return cs_.CreateCall(STRINGFY2(l_mathop(floor)),
                    {cs_.B_.CreateFDiv(lhs, rhs, name)}, "floor");
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
        default: break;
    }
    assert(false);
    return -1;
}

}

