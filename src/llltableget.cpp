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

TableGet::TableGet(CompilerState& cs, Stack& stack, Value& table, Value& key,
        Register& dest) :
    Opcode(cs, stack),
    table_(table),
    key_(key),
    dest_(dest),
    tablevalue_(nullptr),
    switchtag_(cs_.CreateSubBlock("switchtag")),
    getint_(cs_.CreateSubBlock("getint", switchtag_)),
    getshrstr_(cs_.CreateSubBlock("getshrstr", getint_)),
    getlngstr_(cs_.CreateSubBlock("getlngstr", getshrstr_)),
    getany_(cs_.CreateSubBlock("getany", getlngstr_)),
    saveresult_(cs_.CreateSubBlock("saveresult", getany_)),
    searchtm_(cs_.CreateSubBlock("searchtm", saveresult_)),
    finishget_(cs_.CreateSubBlock("finshget", searchtm_)) {
}

void TableGet::Compile() {
    CheckTable();
    SwithTag();
    PerformGet();
    SearchForTM();
    SaveResult();
    FinishGet();
}

void TableGet::CheckTable() {
    cs_.B_.SetInsertPoint(entry_);
    cs_.B_.CreateCondBr(table_.HasTag(ctb(LUA_TTABLE)), switchtag_, finishget_);
    auto ttvalue = static_cast<llvm::PointerType*>(cs_.rt_.GetType("TValue"));
    auto nulltvalue = llvm::ConstantPointerNull::get(ttvalue);
    tms_.push_back({nulltvalue, entry_});
}

void TableGet::SwithTag() {
#if 1
    cs_.B_.SetInsertPoint(switchtag_);
    tablevalue_ = table_.GetTable();
    auto s = cs_.B_.CreateSwitch(key_.GetTag(), getany_, 4);
    auto AddCase = [&](int v, llvm::BasicBlock* block) {
        s->addCase(static_cast<llvm::ConstantInt*>(cs_.MakeInt(v)), block);
    };
    AddCase(LUA_TNUMINT, getint_);
    AddCase(ctb(LUA_TSHRSTR), getshrstr_);
    AddCase(ctb(LUA_TLNGSTR), getlngstr_);
    AddCase(LUA_TNIL, searchtm_);
#else
    cs_.B_.SetInsertPoint(switchtag_);
    tablevalue_ = table_.GetTable();
    cs_.B_.CreateBr(getany_);
#endif
}

void TableGet::PerformGet() {
    PerformGetCase(getint_, &Value::GetInteger, "int");
    PerformGetCase(getshrstr_, &Value::GetTString, "shortstr");
    PerformGetCase(getlngstr_, &Value::GetTString, "str");
    PerformGetCase(getany_, &Value::GetTValue, "");
}

void TableGet::SearchForTM() {
    auto checkflags = cs_.CreateSubBlock("checkflags", searchtm_);
    auto callgettm = cs_.CreateSubBlock("callgettm", checkflags);
    auto tmnotfound = cs_.CreateSubBlock("tmnotfound", callgettm);

    // Has metatable?
    cs_.B_.SetInsertPoint(searchtm_);
    auto tablet = cs_.rt_.GetType("Table");
    auto metatable = cs_.LoadField(tablevalue_, tablet,
            offsetof(Table, metatable), "metatable");
    auto ismtnull = cs_.B_.CreateIsNull(metatable, "is.mt.null");
    cs_.B_.CreateCondBr(ismtnull, tmnotfound, checkflags);

    // Has tagged method?
    cs_.B_.SetInsertPoint(checkflags);
    auto flags = cs_.LoadField(metatable, cs_.rt_.MakeIntT(sizeof(int)),
            offsetof(Table, flags), "metatableflags");
    auto flagenable = cs_.B_.CreateAnd(flags, cs_.MakeInt(1u << TM_INDEX));
    auto hastm = cs_.B_.CreateICmpEQ(flagenable, cs_.MakeInt(0), "has.tm");
    cs_.B_.CreateCondBr(hastm, callgettm, tmnotfound);
    
    // Call luaT_gettm
    cs_.B_.SetInsertPoint(callgettm);
    auto tstringt = cs_.rt_.GetType("TString");
    auto tmname = cs_.InjectPointer(tstringt, G(cs_.L_)->tmname[TM_INDEX]);
    auto args = {metatable, cs_.MakeInt(TM_INDEX), tmname};
    auto tm = cs_.CreateCall("luaT_gettm", args, "tm");
    auto istmnull = cs_.B_.CreateIsNull(tm, "is.tm.null");
    cs_.B_.CreateCondBr(istmnull, tmnotfound, finishget_);
    tms_.push_back({tm, callgettm});

    // Tagged method not found, result is nil
    cs_.B_.SetInsertPoint(tmnotfound);
    cs_.SetField(dest_.GetTValue(), cs_.MakeInt(LUA_TNIL),
            offsetof(TValue, tt_), "desttag");
    cs_.B_.CreateBr(exit_);
}

void TableGet::SaveResult() {
    cs_.B_.SetInsertPoint(saveresult_);
    auto ttvalue = cs_.rt_.GetType("TValue");
    RTRegister result(cs_, CreatePHI(ttvalue, results_, "resultphi"));
    dest_.Assign(result);
    cs_.B_.CreateBr(exit_);
}

void TableGet::FinishGet() {
    cs_.B_.SetInsertPoint(finishget_);
    auto ttvalue = cs_.rt_.GetType("TValue");
    auto tmphi = CreatePHI(ttvalue, tms_, "tmphi");
    auto args = {
        cs_.values_.state,
        table_.GetTValue(),
        key_.GetTValue(),
        dest_.GetTValue(),
        tmphi
    };
    cs_.CreateCall("luaV_finishget", args);
    stack_.Update();
    cs_.B_.CreateBr(exit_);
}

void TableGet::PerformGetCase(llvm::BasicBlock* block, GetMethod getmethod,
        const char* suffix) {
    cs_.B_.SetInsertPoint(block);
    auto tableget = std::string("luaH_get") + suffix;
    auto args = {tablevalue_, (key_.*getmethod)()};
    auto result = cs_.CreateCall(tableget, args, "result");
    auto tag = cs_.LoadField(result, cs_.rt_.MakeIntT(sizeof(int)),
            offsetof(TValue, tt_), "result.tag");
    auto isnil = cs_.B_.CreateICmpEQ(tag, cs_.MakeInt(LUA_TNIL));
    cs_.B_.CreateCondBr(isnil, searchtm_, saveresult_);
    results_.push_back({result, block});
}

}

