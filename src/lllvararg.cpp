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
    nmoves_(nullptr) {
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

llvm::BasicBlock* Vararg::ComputeAvailableArgs(llvm::BasicBlock* entry) {
    auto func = cs_.LoadField(cs_.values_.ci, cs_.rt_.GetType("TValue"),
            offsetof(CallInfo, func), "func");
    auto vadiff = cs_.builder_.CreatePtrDiff(cs_.values_.base, func, "vadiff");
    auto vasize = cs_.builder_.CreateIntCast(vadiff,
            cs_.rt_.MakeIntT(sizeof(int)), false, "vasize");
    auto numparams1 = cs_.MakeInt(cs_.proto_->numparams + 1);
    auto n = cs_.builder_.CreateSub(vasize, numparams1, "n");

    // available = max(n, 0)
    auto nge0 = cs_.builder_.CreateICmpSGE(n, cs_.MakeInt(0), "n.ge.0");
    auto nge0int = cs_.builder_.CreateIntCast(nge0,
            cs_.rt_.MakeIntT(sizeof(int)), false, "n.ge.0_int");
    available_ = cs_.builder_.CreateMul(nge0int, n, "available");

    return entry;
}

llvm::BasicBlock* Vararg::ComputeRequiredArgs(llvm::BasicBlock* entry) {
    int b = GETARG_B(cs_.instr_);
    required_ = cs_.MakeInt(b - 1);
    if (b == 0) {
        required_ = available_;
        cs_.CreateCall("lll_checkstack", {cs_.values_.state, available_});
        cs_.UpdateStack();
        auto top = GetRegisterFromA(available_);
        cs_.SetField(cs_.values_.state, top, offsetof(lua_State, top), "top");
    }
    return entry;
}

llvm::BasicBlock* Vararg::ComputeNMoves(llvm::BasicBlock* entry) {
    auto requiredmin = cs_.CreateSubBlock("requiredmin", entry);
    auto availablemin = cs_.CreateSubBlock("availablemin", requiredmin);
    auto computenmoves = cs_.CreateSubBlock("computenmoves", availablemin);

    auto required_lt_available = cs_.builder_.CreateICmpSLT(required_, 
            available_, "required.lt.available");
    cs_.builder_.CreateCondBr(required_lt_available, requiredmin, availablemin);

    cs_.builder_.SetInsertPoint(requiredmin);
    cs_.builder_.CreateBr(computenmoves);

    cs_.builder_.SetInsertPoint(availablemin);
    cs_.builder_.CreateBr(computenmoves);

    cs_.builder_.SetInsertPoint(computenmoves);
    auto nmoves = cs_.builder_.CreatePHI(cs_.rt_.MakeIntT(sizeof(int)), 2,
            "nmoves");
    nmoves->addIncoming(required_, requiredmin);
    nmoves->addIncoming(available_, availablemin);
    nmoves_ = nmoves;

    return computenmoves;
}

llvm::BasicBlock* Vararg::MoveAvailable(llvm::BasicBlock* entry) {
    auto check = cs_.CreateSubBlock("move.check", entry);
    auto move = cs_.CreateSubBlock("move", check);
    auto end = cs_.CreateSubBlock("move.end", move);

    cs_.builder_.CreateBr(check);

    cs_.builder_.SetInsertPoint(check);
    auto i = cs_.builder_.CreatePHI(cs_.rt_.MakeIntT(sizeof(int)), 2, "i");
    i->addIncoming(cs_.MakeInt(0), entry);
    i->addIncoming(cs_.builder_.CreateAdd(i, cs_.MakeInt(1)), move);
    auto i_lt_nmoves = cs_.builder_.CreateICmpSLT(i, nmoves_, "i.lt.nmoves");
    cs_.builder_.CreateCondBr(i_lt_nmoves, move, end);

    cs_.builder_.SetInsertPoint(move);
    auto vidx = cs_.builder_.CreateSub(i, available_, "valueidx");
    auto v = cs_.builder_.CreateGEP(cs_.values_.base, vidx, "value");
    auto r = GetRegisterFromA(i);
    cs_.SetRegister(r, v);
    cs_.builder_.CreateBr(check);

    return end;
}

llvm::BasicBlock* Vararg::FillRequired(llvm::BasicBlock* entry) {
    auto check = cs_.CreateSubBlock("fill.check", entry);
    auto fill = cs_.CreateSubBlock("fill", check);
    auto end = cs_.CreateSubBlock("fill.end", check);

    cs_.builder_.CreateBr(check);

    cs_.builder_.SetInsertPoint(check);
    auto j = cs_.builder_.CreatePHI(cs_.rt_.MakeIntT(sizeof(int)), 2, "j");
    j->addIncoming(nmoves_, entry);
    j->addIncoming(cs_.builder_.CreateAdd(j, cs_.MakeInt(1)), fill);
    auto j_lt_req = cs_.builder_.CreateICmpSLT(j, required_, "j.lt.required");
    cs_.builder_.CreateCondBr(j_lt_req, fill, end);

    cs_.builder_.SetInsertPoint(fill);
    auto r = GetRegisterFromA(j);
    cs_.SetField(r, cs_.MakeInt(LUA_TNIL), offsetof(TValue, tt_), "tag");
    cs_.builder_.CreateBr(check);

    return end;
}

llvm::Value* Vararg::GetRegisterFromA(llvm::Value* offset) {
    auto a = cs_.MakeInt(GETARG_A(cs_.instr_));
    auto idx = cs_.builder_.CreateAdd(a, offset, "idx");
    return cs_.builder_.CreateGEP(cs_.values_.base, idx, "register");
}

}

