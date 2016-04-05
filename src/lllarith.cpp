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
    Opcode(cs, stack),
    ra_(stack.GetR(GETARG_A(cs.instr_))),
    rkb_(stack.GetRK(GETARG_B(cs.instr_))),
    rkc_(stack.GetRK(GETARG_C(cs.instr_))),
    x_(rkb_),
    y_(rkc_),
    check_y_(cs.CreateSubBlock("check_y")),
    intop_(cs.CreateSubBlock("intop", check_y_)),
    floatop_(cs.CreateSubBlock("floatop", intop_)),
    tmop_(cs.CreateSubBlock("tmop", floatop_)),
    x_int_(nullptr),
    x_float_(nullptr) {
}

void Arith::Compile() {
    CheckXTag();
    CheckYTag();
    ComputeInt();
    ComputeFloat();
    ComputeTaggedMethod();
}

void Arith::CheckXTag() {
    auto check_y_int = cs_.CreateSubBlock("is_y_int");
    auto check_x_float = cs_.CreateSubBlock("is_x_float", check_y_int);
    auto tonumber_x = cs_.CreateSubBlock("tonumber_x", check_x_float);

    cs_.B_.SetInsertPoint(entry_);
    auto xtag = x_.GetTag();
    auto is_x_int = cs_.B_.CreateICmpEQ(xtag, cs_.MakeInt(LUA_TNUMINT));
    cs_.B_.CreateCondBr(is_x_int, check_y_int, check_x_float);

    cs_.B_.SetInsertPoint(check_y_int);
    x_int_ = x_.GetInteger();
    auto floatt = cs_.rt_.GetType("lua_Number");
    auto x_itof = cs_.B_.CreateSIToFP(x_int_, floatt, x_int_->getName() + "_flt");
    x_float_inc_.push_back({x_itof, check_y_int});
    auto is_y_int = y_.HasTag(LUA_TNUMINT);
    cs_.B_.CreateCondBr(is_y_int, intop_, check_y_);

    cs_.B_.SetInsertPoint(check_x_float);
    x_float_inc_.push_back({x_.GetFloat(), check_x_float});
    auto is_x_float = cs_.B_.CreateICmpEQ(xtag, cs_.MakeInt(LUA_TNUMFLT));
    cs_.B_.CreateCondBr(is_x_float, check_y_, tonumber_x);

    cs_.B_.SetInsertPoint(tonumber_x);
    auto args = {x_.GetTValue(), cs_.values_.xnumber};
    auto tonumberret = cs_.CreateCall("luaV_tonumber_", args);
    auto x_converted = cs_.B_.CreateLoad(cs_.values_.xnumber);
    x_float_inc_.push_back({x_converted, tonumber_x});
    auto converted = cs_.ToBool(tonumberret);
    cs_.B_.CreateCondBr(converted, check_y_, tmop_);
}

void Arith::CheckYTag() {
    auto tonumber_y = cs_.CreateSubBlock("tonumber_y", check_y_);

    cs_.B_.SetInsertPoint(check_y_);
    auto floatt = cs_.rt_.GetType("lua_Number");
    x_float_ = CreatePHI(floatt, x_float_inc_, "xfloat");
    y_float_inc_.push_back({y_.GetFloat(), check_y_});
    auto is_y_float = y_.HasTag(LUA_TNUMFLT);
    cs_.B_.CreateCondBr(is_y_float, floatop_, tonumber_y);

    cs_.B_.SetInsertPoint(tonumber_y);
    auto args = {y_.GetTValue(), cs_.values_.ynumber};
    auto tonumberret = cs_.CreateCall("luaV_tonumber_", args);
    auto y_converted = cs_.B_.CreateLoad(cs_.values_.ynumber);
    y_float_inc_.push_back({y_converted, tonumber_y});
    auto converted = cs_.ToBool(tonumberret);
    cs_.B_.CreateCondBr(converted, floatop_, tmop_);
}

void Arith::ComputeInt() {
    cs_.B_.SetInsertPoint(intop_);
    auto y_int = y_.GetInteger();
    if (HasIntegerOp()) {
        ra_.SetInteger(PerformIntOp(x_int_, y_int));
        cs_.B_.CreateBr(exit_);
    } else {
        auto floatt = cs_.rt_.GetType("lua_Number");
        auto x_float = cs_.B_.CreateSIToFP(x_int_, floatt);
        auto y_float = cs_.B_.CreateSIToFP(y_int, floatt);
        ra_.SetFloat(PerformFloatOp(x_float, y_float));
        cs_.B_.CreateBr(exit_);
    }
}

void Arith::ComputeFloat() {
    cs_.B_.SetInsertPoint(floatop_);
    auto floatt = cs_.rt_.GetType("lua_Number");
    auto y_float = CreatePHI(floatt, y_float_inc_, "yfloat");
    ra_.SetFloat(PerformFloatOp(x_float_, y_float));
    cs_.B_.CreateBr(exit_);
}

void Arith::ComputeTaggedMethod() {
    cs_.B_.SetInsertPoint(tmop_);
    auto args = {
        cs_.values_.state,
        x_.GetTValue(),
        y_.GetTValue(),
        ra_.GetTValue(),
        cs_.MakeInt(GetMethodTag())
    };
    cs_.CreateCall("luaT_trybinTM", args);
    stack_.Update();
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

