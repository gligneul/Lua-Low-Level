/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** llltableget.cpp
*/

#include "lllcompilerstate.h"
#include "lllruntime.h"
#include "llltableget.h"
#include "lllvalue.h"

extern "C" {
#include "lprefix.h"
#include "llimits.h"
#include "lobject.h"
#include "lstate.h"
#include "ltm.h"
}

namespace lll {

TableGet::TableGet(CompilerState& cs, Value& table, Value& key, Value& dest) :
    Opcode(cs),
    table_(table),
    key_(key),
    dest_(dest),
    tablevalue_(nullptr),
    entry_(cs_.blocks_[cs_.curr_]),
    switchtag_(cs_.CreateSubBlock("switchtag")),
    getint_(cs_.CreateSubBlock("getint", switchtag_)),
    getshrstr_(cs_.CreateSubBlock("getshrstr", getint_)),
    getlngstr_(cs_.CreateSubBlock("getlngstr", getshrstr_)),
    getany_(cs_.CreateSubBlock("getany", getlngstr_)),
    saveresult_(cs_.CreateSubBlock("saveresult", getany_)),
    searchtm_(cs_.CreateSubBlock("searchtm", saveresult_)),
    finishget_(cs_.CreateSubBlock("finshget", searchtm_)),
    end_(cs_.blocks_[cs_.curr_ + 1]) {
}

std::vector<TableGet::CompilationStep> TableGet::GetSteps() {
    return {
        &TableGet::CheckTable,
        &TableGet::SwithTag,
        &TableGet::PerformGet,
        &TableGet::SearchForTM,
        &TableGet::SaveResult,
        &TableGet::FinishGet
    };
}

llvm::BasicBlock* TableGet::CheckTable(llvm::BasicBlock* entry) {
    B_.SetInsertPoint(entry_);
    auto tabletag = cs_.MakeInt(ctb(LUA_TTABLE));
    auto is_table = B_.CreateICmpEQ(table_.GetTag(), tabletag, "is.table");
    B_.CreateCondBr(is_table, switchtag_, finishget_);
    auto nulltvalue = llvm::ConstantPointerNull::get(
            static_cast<llvm::PointerType*>(cs_.rt_.GetType("TValue")));
    tms_.push_back({nulltvalue, entry_});
    return entry;
}

llvm::BasicBlock* TableGet::SwithTag(llvm::BasicBlock* entry) {
    B_.SetInsertPoint(switchtag_);
    tablevalue_ = table_.GetTable();
    auto s = B_.CreateSwitch(key_.GetTag(), getany_, 4);
    auto AddCase = [&](int v, llvm::BasicBlock* block) {
        s->addCase(static_cast<llvm::ConstantInt*>(cs_.MakeInt(v)), block);
    };
    AddCase(LUA_TNUMINT, getint_);
    AddCase(ctb(LUA_TSHRSTR), getshrstr_);
    AddCase(ctb(LUA_TLNGSTR), getlngstr_);
    AddCase(LUA_TNIL, searchtm_);
    return entry;
}

llvm::BasicBlock* TableGet::PerformGet(llvm::BasicBlock* entry) {
    PerformGetCase(getint_, &Value::GetInteger, "int");
    PerformGetCase(getshrstr_, &Value::GetTString, "shortstr");
    PerformGetCase(getlngstr_, &Value::GetTString, "str");
    PerformGetCase(getany_, &Value::GetTValue, "");
    return entry;
}

llvm::BasicBlock* TableGet::SearchForTM(llvm::BasicBlock* entry) {
    auto checkflags = cs_.CreateSubBlock("checkflags", searchtm_);
    auto callgettm = cs_.CreateSubBlock("callgettm", checkflags);
    auto tmnotfound = cs_.CreateSubBlock("tmnotfound", callgettm);

    // Has metatable?
    B_.SetInsertPoint(searchtm_);
    auto tablet = cs_.rt_.GetType("Table");
    auto metatable = cs_.LoadField(tablevalue_, tablet,
            offsetof(Table, metatable), "metatable");
    auto ismtnull = B_.CreateIsNull(metatable, "is.mt.null");
    B_.CreateCondBr(ismtnull, tmnotfound, checkflags);

    // Has tagged method?
    B_.SetInsertPoint(checkflags);
    auto flags = cs_.LoadField(metatable, cs_.rt_.MakeIntT(sizeof(int)),
            offsetof(Table, flags), "metatableflags");
    auto flagenable = B_.CreateAnd(flags, cs_.MakeInt(1u << TM_INDEX));
    auto hastm = B_.CreateICmpEQ(flagenable, cs_.MakeInt(0), "has.tm");
    B_.CreateCondBr(hastm, callgettm, tmnotfound);
    
    // Call luaT_gettm
    B_.SetInsertPoint(callgettm);
    auto tstringt = cs_.rt_.GetType("TString");
    auto tmname = cs_.InjectPointer(tstringt, G(cs_.L_)->tmname[TM_INDEX]);
    auto args = {metatable, cs_.MakeInt(TM_INDEX), tmname};
    auto tm = cs_.CreateCall("luaT_gettm", args, "tm");
    auto istmnull = B_.CreateIsNull(tm, "is.tm.null");
    B_.CreateCondBr(istmnull, tmnotfound, finishget_);
    tms_.push_back({tm, callgettm});

    // Tagged method not found, result is nil
    B_.SetInsertPoint(tmnotfound);
    cs_.SetField(dest_.GetTValue(), cs_.MakeInt(LUA_TNIL),
            offsetof(TValue, tt_), "desttag");
    B_.CreateBr(end_);

    return entry;
}

llvm::BasicBlock* TableGet::SaveResult(llvm::BasicBlock* entry) {
    B_.SetInsertPoint(saveresult_);
    auto tvaluet = cs_.rt_.GetType("TValue");
    auto resultphi = B_.CreatePHI(tvaluet, results_.size(), "resultphi");
    for (auto& r : results_)
        resultphi->addIncoming(r.first, r.second);
    cs_.SetRegister(dest_.GetTValue(), resultphi);
    B_.CreateBr(end_);
    return entry;
}

llvm::BasicBlock* TableGet::FinishGet(llvm::BasicBlock* entry) {
    B_.SetInsertPoint(finishget_);
    auto tvaluet = cs_.rt_.GetType("TValue");
    auto tmphi = B_.CreatePHI(tvaluet, tms_.size(), "tmphi");
    for (auto& tm : tms_)
        tmphi->addIncoming(tm.first, tm.second);
    auto args = {
        cs_.values_.state,
        table_.GetTValue(),
        key_.GetTValue(),
        dest_.GetTValue(),
        static_cast<llvm::Value*>(tmphi)
    };
    cs_.CreateCall("luaV_finishget", args);
    B_.CreateBr(end_);
    return entry;
}

void TableGet::PerformGetCase(llvm::BasicBlock* block, GetMethod getmethod,
        const char* suffix) {
    B_.SetInsertPoint(block);
    auto tableget = std::string("luaH_get") + suffix;
    auto args = {tablevalue_, (key_.*getmethod)()};
    auto result = cs_.CreateCall(tableget, args, "result");
    auto tag = cs_.LoadField(result, cs_.rt_.MakeIntT(sizeof(int)),
            offsetof(TValue, tt_), "result.tag");
    auto isnil = B_.CreateICmpEQ(tag, cs_.MakeInt(LUA_TNIL));
    B_.CreateCondBr(isnil, searchtm_, saveresult_);
    results_.push_back({result, block});
}

}

