/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllvararg.cpp
*/

#include "lllvararg.h"

extern "C" {
#include "lprefix.h"
#include "lfunc.h"
#include "lopcodes.h"
#include "lstate.h"
}

namespace lll {

Vararg::Vararg(CompilerState& cs) :
    Opcode(cs),
    available_(nullptr),
    required_(nullptr),
    nmoves_(nullptr),
    movecheck_(cs.CreateSubBlock("movecheck", entry_)),
    move_(cs.CreateSubBlock("move", movecheck_)),
    fillcheck_(cs.CreateSubBlock("fillcheck", move_)),
    fill_(cs.CreateSubBlock("fill", fillcheck_)) {
}

std::vector<Vararg::CompilationStep> Vararg::GetSteps() {
    return {
        &Vararg::ComputeAvailableArgs,
        &Vararg::ComputeRequiredArgs,
        &Vararg::ComputeNMoves,
        &Vararg::MoveAvailable,
        &Vararg::FillRequired
    };
}

void Vararg::ComputeAvailableArgs() {
    B_.SetInsertPoint(entry_);
    auto func = cs_.LoadField(cs_.values_.ci, cs_.rt_.GetType("TValue"),
            offsetof(CallInfo, func), "func");
    auto vadiff = B_.CreatePtrDiff(cs_.values_.base, func, "vadiff");
    auto tint = cs_.rt_.MakeIntT(sizeof(int));
    auto vasize = B_.CreateIntCast(vadiff, tint, false, "vasize");
    auto numparams1 = cs_.MakeInt(cs_.proto_->numparams + 1);
    auto n = B_.CreateSub(vasize, numparams1, "n");
    auto n_ge_0 = B_.CreateICmpSGE(n, cs_.MakeInt(0), "n.ge.0");
    available_ = B_.CreateSelect(n_ge_0, n, cs_.MakeInt(0));
}

void Vararg::ComputeRequiredArgs() {
    int b = GETARG_B(cs_.instr_);
    required_ = cs_.MakeInt(b - 1);
    if (b == 0) {
        required_ = available_;
        cs_.CreateCall("lll_checkstack", {cs_.values_.state, available_});
        cs_.UpdateStack();
        auto top = GetRegisterFromA(available_);
        cs_.SetField(cs_.values_.state, top, offsetof(lua_State, top), "top");
    }
}

void Vararg::ComputeNMoves() {
    auto required_lt_available = B_.CreateICmpSLT(required_, available_,
            "required.lt.available");
    nmoves_ = B_.CreateSelect(required_lt_available, required_, available_);
    B_.CreateBr(movecheck_);
}

void Vararg::MoveAvailable() {
    B_.SetInsertPoint(movecheck_);
    auto i = B_.CreatePHI(cs_.rt_.MakeIntT(sizeof(int)), 2, "i");
    i->addIncoming(cs_.MakeInt(0), entry_);
    i->addIncoming(B_.CreateAdd(i, cs_.MakeInt(1)), move_);
    auto i_lt_nmoves = B_.CreateICmpSLT(i, nmoves_, "i.lt.nmoves");
    B_.CreateCondBr(i_lt_nmoves, move_, fillcheck_);

    B_.SetInsertPoint(move_);
    auto vidx = B_.CreateSub(i, available_, "valueidx");
    auto v = B_.CreateGEP(cs_.values_.base, vidx, "value");
    auto r = GetRegisterFromA(i);
    cs_.SetRegister(r, v);
    B_.CreateBr(movecheck_);
}

void Vararg::FillRequired() {
    B_.SetInsertPoint(fillcheck_);
    auto j = B_.CreatePHI(cs_.rt_.MakeIntT(sizeof(int)), 2, "j");
    j->addIncoming(nmoves_, movecheck_);
    j->addIncoming(B_.CreateAdd(j, cs_.MakeInt(1)), fill_);
    auto j_lt_req = B_.CreateICmpSLT(j, required_, "j.lt.required");
    B_.CreateCondBr(j_lt_req, fill_, exit_);

    B_.SetInsertPoint(fill_);
    auto r = GetRegisterFromA(j);
    cs_.SetField(r, cs_.MakeInt(LUA_TNIL), offsetof(TValue, tt_), "tag");
    B_.CreateBr(fillcheck_);
}

llvm::Value* Vararg::GetRegisterFromA(llvm::Value* offset) {
    auto a = cs_.MakeInt(GETARG_A(cs_.instr_));
    auto idx = B_.CreateAdd(a, offset, "idx");
    return B_.CreateGEP(cs_.values_.base, idx, "register");
}

}

