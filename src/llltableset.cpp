/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** llltableset.cpp
*/

#include "lllcompilerstate.h"
#include "lllruntime.h"
#include "llltableset.h"
#include "lllvalue.h"

extern "C" {
#include "lprefix.h"
#include "lgc.h"
#include "llimits.h"
#include "lobject.h"
#include "lstate.h"
#include "ltm.h"
}

namespace lll {

TableSet::TableSet(CompilerState& cs, Stack& stack, Value& table, Value& key,
        Value& value) :
    Opcode(cs, stack),
    table_(table),
    key_(key),
    value_(value),
    tablevalue_(nullptr),
    slot_(nullptr),
    switchtag_(cs_.CreateSubBlock("switchtag")),
    getint_(cs_.CreateSubBlock("getint", switchtag_)),
    getshrstr_(cs_.CreateSubBlock("getshrstr", getint_)),
    getlngstr_(cs_.CreateSubBlock("getlngstr", getshrstr_)),
    getnil_(cs_.CreateSubBlock("getnil", getlngstr_)),
    getany_(cs_.CreateSubBlock("getany", getnil_)),
    callgcbarrier_(cs_.CreateSubBlock("callgcbarrier", getany_)),
    fastset_(cs_.CreateSubBlock("fastset", callgcbarrier_)),
    finishset_(cs_.CreateSubBlock("finishset", fastset_)) {
}

void TableSet::Compile() {
    CheckTable();
    SwithTag();
    PerformGet();
    CallGCBarrier();
    FastSet();
    FinishSet();
}

void TableSet::CheckTable() {
    cs_.B_.SetInsertPoint(entry_);
    cs_.B_.CreateCondBr(table_.HasTag(ctb(LUA_TTABLE)), switchtag_, finishset_);
    auto ttvalue = static_cast<llvm::PointerType*>(cs_.rt_.GetType("TValue"));
    auto nulltvalue = llvm::ConstantPointerNull::get(ttvalue);
    oldvals_.push_back({nulltvalue, entry_});
}

void TableSet::SwithTag() {
    cs_.B_.SetInsertPoint(switchtag_);
    tablevalue_ = table_.GetTable();
    auto s = cs_.B_.CreateSwitch(key_.GetTag(), getany_, 4);
    auto AddCase = [&](int v, llvm::BasicBlock* block) {
        s->addCase(static_cast<llvm::ConstantInt*>(cs_.MakeInt(v)), block);
    };
    AddCase(LUA_TNUMINT, getint_);
    AddCase(ctb(LUA_TSHRSTR), getshrstr_);
    AddCase(ctb(LUA_TLNGSTR), getlngstr_);
    AddCase(LUA_TNIL, getnil_);
}

void TableSet::PerformGet() {
    PerformGetCase(getint_, &Value::GetInteger, "int");
    PerformGetCase(getshrstr_, &Value::GetTString, "shortstr");
    PerformGetCase(getlngstr_, &Value::GetTString, "str");
    PerformGetCase(getany_, &Value::GetTValue, "");

    // A nil key will always return a nil value
    cs_.B_.SetInsertPoint(getnil_);
    auto ttvalue = cs_.rt_.GetType("TValue");
    auto nilobj = cs_.InjectPointer(ttvalue,
            const_cast<TValue*>(luaO_nilobject));
    cs_.B_.CreateBr(finishset_);
    oldvals_.push_back({nilobj, getnil_});
}

void TableSet::CallGCBarrier() {
    auto checktableblack = cs_.CreateSubBlock("checktableblack",
            callgcbarrier_);
    auto checkvaluewhite = cs_.CreateSubBlock("checkvaluewhite",
            checktableblack);
    auto callbarrierback = cs_.CreateSubBlock("callbarrierback",
            checkvaluewhite);

    // iscollectable(v)?
    cs_.B_.SetInsertPoint(callgcbarrier_);
    slot_ = CreatePHI(cs_.rt_.GetType("TValue"), slots_, "slot");
    auto collectablebit = cs_.MakeInt(BIT_ISCOLLECTABLE);
    auto iscoll = cs_.ToBool(cs_.B_.CreateAnd(value_.GetTag(), collectablebit));
    cs_.B_.CreateCondBr(iscoll, checktableblack, fastset_);

    // isblack(table)?
    cs_.B_.SetInsertPoint(checktableblack);
    auto tlu_byte = cs_.rt_.MakeIntT(sizeof(lu_byte));
    auto markedoffset = offsetof(GCObject, marked);
    auto tablemarked = cs_.LoadField(tablevalue_, tlu_byte, markedoffset,
            "tablemarked");
    auto blackbit = cs_.MakeInt(bitmask(BLACKBIT), tlu_byte);
    auto isblack = cs_.ToBool(cs_.B_.CreateAnd(tablemarked, blackbit));
    cs_.B_.CreateCondBr(isblack, checkvaluewhite, fastset_);

    // iswhite(gcvalue(v)))?
    cs_.B_.SetInsertPoint(checkvaluewhite);
    auto gcvalue = value_.GetGCValue();
    auto valuemarked = cs_.LoadField(gcvalue, tlu_byte, markedoffset,
            "valuemarked");
    auto whitebits = cs_.MakeInt(WHITEBITS, tlu_byte);
    auto iswhite = cs_.ToBool(cs_.B_.CreateAnd(valuemarked, whitebits));
    cs_.B_.CreateCondBr(iswhite, callbarrierback, fastset_);

    // luaC_barrierback_(L,p)
    cs_.B_.SetInsertPoint(callbarrierback);
    auto args = {cs_.values_.state, tablevalue_};
    cs_.CreateCall("luaC_barrierback_", args);
    cs_.B_.CreateBr(fastset_);
}

void TableSet::FastSet() {
    cs_.B_.SetInsertPoint(fastset_);
    RTRegister slot(cs_, slot_);
    slot.Assign(value_);
    cs_.B_.CreateBr(exit_);
}

void TableSet::FinishSet() {
    cs_.B_.SetInsertPoint(finishset_);
    auto ttvalue = cs_.rt_.GetType("TValue");
    auto oldvalphi = CreatePHI(ttvalue, oldvals_, "oldval");
    auto args = {
        cs_.values_.state,
        table_.GetTValue(),
        key_.GetTValue(),
        value_.GetTValue(),
        oldvalphi
    };
    cs_.CreateCall("luaV_finishset", args);
    stack_.Update();
    cs_.B_.CreateBr(exit_);
}

void TableSet::PerformGetCase(llvm::BasicBlock* block, GetMethod getmethod,
        const char* suffix) {
    cs_.B_.SetInsertPoint(block);
    auto tableget = std::string("luaH_get") + suffix;
    auto args = {tablevalue_, (key_.*getmethod)()};
    auto result = cs_.CreateCall(tableget, args, "result");
    auto tag = cs_.LoadField(result, cs_.rt_.MakeIntT(sizeof(int)),
            offsetof(TValue, tt_), "result.tag");
    auto isnil = cs_.B_.CreateICmpEQ(tag, cs_.MakeInt(LUA_TNIL));
    cs_.B_.CreateCondBr(isnil, finishset_, callgcbarrier_);
    oldvals_.push_back({result, block});
    slots_.push_back({result, block});
}

}

