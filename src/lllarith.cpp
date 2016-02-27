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

extern "C" {
#include "lprefix.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lvm.h"
}

namespace lll {

Arith::Arith(CompilerState& cs) :
    cs_(cs),
    ra_(cs_.GetValueR(GETARG_A(cs_.instr_), "ra")),
    b_(cs, GETARG_B(cs_.instr_), "rb", Value::STRING_TO_FLOAT),
    c_(cs, GETARG_C(cs_.instr_), "rc", Value::STRING_TO_FLOAT) {
}

std::vector<Arith::CompilationStep> Arith::GetSteps() {
    return {
        &Arith::Compute,
        &Arith::ComputeTaggedMethod
    };
}

llvm::BasicBlock* Arith::Compute(llvm::BasicBlock* entry) {
    // The optimal solution is complex. The pseudo-code bellow tries to explain
    // how the operation happens:
    // switch (b.tag)
    //   case INT:
    //     switch (c.tag)
    //       case INT:
    //         intOp()
    //       case FLOAT:
    //         floatOp()
    //       default:
    //         if (tonumber(c))
    //           floatOp()
    //         else
    //           tmOp()
    //   case FLOAT:
    //     switch (c.tag)
    //       case INT:
    //         floatOp()
    //       case FLOAT:
    //         floatOp()
    //       default:
    //         if (tonumber(c))
    //           floatOp()
    //         else
    //           tmOp()
    //   default:
    //     if (tonumber(b))
    //       switch (c.tag)
    //         case INT:
    //           floatOp()
    //         case FLOAT:
    //           floatOp()
    //         default:
    //           if (tonumber(c))
    //             floatOp()
    //           else
    //             tmOp()
    //     else
    //       tmpOp() 

    // Create one block for each case (eg. int/int, int/float, int/conv...)
    llvm::BasicBlock* lastblock = nullptr;
    #define CREATE_BLOCK(name) \
        auto name = lastblock = cs_.CreateSubBlock(#name, lastblock)

    CREATE_BLOCK(bint);
    CREATE_BLOCK(bflt);
    CREATE_BLOCK(bstr);
    CREATE_BLOCK(bint_cflt);
    CREATE_BLOCK(bint_cstr);
    CREATE_BLOCK(bflt_cint);
    CREATE_BLOCK(bflt_cflt);
    CREATE_BLOCK(bflt_cstr);
    CREATE_BLOCK(bstr_cint);
    CREATE_BLOCK(bstr_cflt);
    CREATE_BLOCK(bstr_cstr);
    CREATE_BLOCK(int_op);
    CREATE_BLOCK(flt_op);
    CREATE_BLOCK(tm_op);
    auto end = cs_.blocks_[cs_.curr_ + 1];

    // Operands information
    auto btag = b_.GetTag();
    auto ctag = c_.GetTag();
    auto bnumber = cs_.values_.bnumber;
    auto cnumber = cs_.values_.cnumber;

    // Make the switches
    SwitchTag(b_, btag, bnumber, entry, bint, bflt, bstr, tm_op);
    SwitchTag(c_, ctag, cnumber, bint, int_op, bint_cflt, bint_cstr, tm_op);
    SwitchTag(c_, ctag, cnumber, bflt, bflt_cint, bflt_cflt, bflt_cstr, tm_op);
    SwitchTag(c_, ctag, cnumber, bstr, bstr_cint, bstr_cflt, bstr_cstr, tm_op);

    // Int operation
    cs_.builder_.SetInsertPoint(int_op);
    if (HasIntegerOp()) {
        auto result = PerformIntOp(b_.GetInteger(), c_.GetInteger());
        cs_.SetField(ra_, result, offsetof(TValue, value_), "value");
        auto inttag = cs_.MakeInt(LUA_TNUMINT);
        cs_.SetField(ra_, inttag, offsetof(TValue, tt_), "tag");
        cs_.builder_.CreateBr(end);
    }

    // Float operation
    cs_.builder_.SetInsertPoint(flt_op);
    auto floatt = cs_.rt_.GetType("lua_Number");
    int nincoming = 8 + (int)!HasIntegerOp();
    auto nb = cs_.builder_.CreatePHI(floatt, nincoming, "nb");
    auto nc = cs_.builder_.CreatePHI(floatt, nincoming, "nc");
    auto result = PerformFloatOp(nb, nc);
    cs_.SetField(ra_, result, offsetof(TValue, value_), "value");
    auto flttag = cs_.MakeInt(LUA_TNUMFLT);
    cs_.SetField(ra_, flttag, offsetof(TValue, tt_), "tag");
    cs_.builder_.CreateBr(end);

    // Set phi's incomings
    #define ITOF(v) \
        cs_.builder_.CreateSIToFP(v, floatt, v->getName() + "_flt")

    #define LOAD(v) \
        cs_.builder_.CreateLoad(v, v->getName() + "_loaded")

    #define SET_INCOMING(block, bvalue, cvalue) { \
        cs_.builder_.SetInsertPoint(block); \
        nb->addIncoming((bvalue), (block)); \
        nc->addIncoming((cvalue), (block)); \
        cs_.builder_.CreateBr(flt_op); }

    if (!HasIntegerOp()) {
        SET_INCOMING(int_op, ITOF(b_.GetInteger()), ITOF(c_.GetInteger()));
    }
    SET_INCOMING(bint_cflt, ITOF(b_.GetInteger()), c_.GetFloat());
    SET_INCOMING(bint_cstr, ITOF(b_.GetInteger()), LOAD(cnumber));
    SET_INCOMING(bflt_cint, b_.GetFloat(), ITOF(c_.GetInteger()));
    SET_INCOMING(bflt_cflt, b_.GetFloat(), c_.GetFloat());
    SET_INCOMING(bflt_cstr, b_.GetFloat(), LOAD(cnumber));
    SET_INCOMING(bstr_cint, LOAD(bnumber), ITOF(c_.GetInteger()));
    SET_INCOMING(bstr_cflt, LOAD(bnumber), c_.GetFloat());
    SET_INCOMING(bstr_cstr, LOAD(bnumber), LOAD(cnumber));

    return tm_op;
}

llvm::BasicBlock* Arith::ComputeTaggedMethod(llvm::BasicBlock* entry) {
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

bool Arith::HasIntegerOp() {
    switch (GET_OPCODE(cs_.instr_)) {
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_MOD: case OP_IDIV:
            return true;
        default:
            break;
    }
    return false;
}

void Arith::SwitchTag(Value& value, llvm::Value* tag, llvm::Value* convptr,
        llvm::BasicBlock* entry, llvm::BasicBlock* intop,
        llvm::BasicBlock* floatop, llvm::BasicBlock* convop,
        llvm::BasicBlock* tmop) {
    auto isfloat = cs_.CreateSubBlock("is_flt", entry);
    auto converted = cs_.CreateSubBlock("converted", isfloat);

    cs_.builder_.SetInsertPoint(entry);
    auto is_tag_int = cs_.builder_.CreateICmpEQ(tag, cs_.MakeInt(LUA_TNUMINT),
            "is.tag.int");
    cs_.builder_.CreateCondBr(is_tag_int, intop, isfloat);

    cs_.builder_.SetInsertPoint(isfloat);
    auto is_tag_float = cs_.builder_.CreateICmpEQ(tag, cs_.MakeInt(LUA_TNUMFLT),
            "is.tag.float");
    cs_.builder_.CreateCondBr(is_tag_float, floatop, converted);

    cs_.builder_.SetInsertPoint(converted);
    auto is_value_converted = cs_.CreateCall("LLLToNumber",
            {value.GetTValue(), convptr}, "is.value.converted");
    cs_.builder_.CreateCondBr(is_value_converted, convop, tmop);
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
        default: break;
    }
    assert(false);
    return -1;
}

}

