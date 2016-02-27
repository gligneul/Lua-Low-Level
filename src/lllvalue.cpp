/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllvalue.cpp
*/

#include "lllcompilerstate.h"
#include "lllvalue.h"

extern "C" {
#include "lprefix.h"
#include "lopcodes.h"
#include "lvm.h"
}

namespace lll {

Value::Value(CompilerState& cs, int arg, const std::string& name,
        ConversionType conversion) :
    cs_(cs),
    conversion_(conversion),
    isk_(ISK(arg)) {
    if (isk_)
        u_.k = cs_.proto_->k + INDEXK(arg);
    else
        u_.r = cs_.GetValueR(arg, name);
}

llvm::Value* Value::GetTag() {
    if (isk_) {
        TValue _;
        int tag;
        if (conversion_ == STRING_TO_FLOAT
            && cvt2num(u_.k)
            && luaO_str2num(svalue(u_.k), &_) == vslen(u_.k) + 1) {
            tag = LUA_TNUMFLT;
        } else {
            tag = ttype(u_.k);
        }
        return cs_.MakeInt(tag);
    } else {
        return cs_.LoadField(u_.r, cs_.rt_.MakeIntT(sizeof(int)),
                offsetof(TValue, tt_), "tag");
    }
}

llvm::Value* Value::GetInteger() {
    auto intt = cs_.rt_.GetType("lua_Integer");
    if (isk_) {
        return llvm::ConstantInt::get(intt, ivalue(u_.k));
    } else {
        return cs_.LoadField(u_.r, intt, offsetof(TValue, value_), "ivalue");
    }
}

llvm::Value* Value::GetFloat() {
    auto floatt = cs_.rt_.GetType("lua_Number");
    if (isk_) {
        TValue converted;
        lua_Number n;
        if (conversion_ == STRING_TO_FLOAT
            && cvt2num(u_.k)) {
            luaO_str2num(svalue(u_.k), &converted);
            n = fltvalue(&converted);
        } else {
            n = fltvalue(u_.k);
        }
        return llvm::ConstantFP::get(floatt, n);
    } else {
        return cs_.LoadField(u_.r, floatt, offsetof(TValue, value_), "nvalue");
    }
}

llvm::Value* Value::GetTValue() {
    if (isk_) {
        auto intptrt = cs_.rt_.MakeIntT(sizeof(void*));
        auto intptr = llvm::ConstantInt::get(intptrt, (uintptr_t)(u_.k));
        return cs_.builder_.CreateIntToPtr(intptr, cs_.rt_.GetType("TValue"));
    } else {
        return u_.r;
    }
}

}

