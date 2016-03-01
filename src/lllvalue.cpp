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

static const size_t TAG_OFFSET = offsetof(TValue, tt_);
static const size_t VALUE_OFFSET = offsetof(TValue, value_);

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

Value::Value(CompilerState& cs, llvm::Value* v) :
    cs_(cs),
    conversion_(NO_CONVERSION),
    isk_(false),
    u_{.r=v} {
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
                TAG_OFFSET, "tag");
    }
}

llvm::Value* Value::GetTValue() {
    if (isk_) {
        return cs_.InjectPointer(cs_.rt_.GetType("TValue"), u_.k);
    } else {
        return u_.r;
    }
}

void Value::SetValue(Value& value) {
    cs_.SetRegister(GetTValue(), value.GetTValue());
}

llvm::Value* Value::IsInteger() {
    return cs_.builder_.CreateICmpEQ(GetTag(), cs_.MakeInt(LUA_TNUMINT),
            "is.int");
}

llvm::Value* Value::GetInteger() {
    auto intt = cs_.rt_.GetType("lua_Integer");
    if (isk_) {
        return llvm::ConstantInt::get(intt, ivalue(u_.k));
    } else {
        return cs_.LoadField(u_.r, intt, VALUE_OFFSET, "ivalue");
    }
}

void Value::SetInteger(llvm::Value* value) {
    cs_.SetField(u_.r, value, VALUE_OFFSET, "value");
    cs_.SetField(u_.r, cs_.MakeInt(LUA_TNUMINT), TAG_OFFSET, "tag");
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
        return cs_.LoadField(u_.r, floatt, VALUE_OFFSET, "nvalue");
    }
}

void Value::SetFloat(llvm::Value* value) {
    cs_.SetField(u_.r, value, VALUE_OFFSET, "value");
    cs_.SetField(u_.r, cs_.MakeInt(LUA_TNUMFLT), TAG_OFFSET, "tag");
}

llvm::Value* Value::GetTString() {
    auto stringt = cs_.rt_.GetType("TString");
    if (isk_) {
        return cs_.InjectPointer(stringt, tsvalue(u_.k));
    } else {
        return cs_.LoadField(u_.r, stringt, VALUE_OFFSET, "tstringvalue");
    }
}

llvm::Value* Value::IsTable() {
    return cs_.builder_.CreateICmpEQ(GetTag(), cs_.MakeInt(ctb(LUA_TTABLE)),
            "is.table");
}

llvm::Value* Value::GetTable() {
    auto tablet = cs_.rt_.GetType("Table");
    return cs_.LoadField(GetTValue(), tablet, VALUE_OFFSET, "tablevalue");
}

}

