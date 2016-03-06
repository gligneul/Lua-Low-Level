/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllvalue.cpp
*/

#include "lllvalue.h"
#include "lllcompilerstate.h"

extern "C" {
#include "lprefix.h"
#include "lobject.h"
}

std::unique_ptr<Value> Value::CreateRK(CompilerState& cs, int arg,
        const std::string& name) {
    if (ISK(arg))
        return CreateK(cs, INDEXK(arg));
    else
        return CreateR(cs, arg, name);
}

std::unique_ptr<Constant> Value::CreateK(CompilerState& cs, int arg) {
    return new Constant(cs, arg);
}

std::unique_ptr<Register> Value::CreateR(CompilerState& cs, int arg,
        const std::string& name = "") {
    return new Register(cs, arg, name);
}

std::unique_ptr<Upvalue> Value::CreateUpval(CompilerState& cs, int arg) {
    return new Upvalue(cs, arg);
}

Constant::Constant(CompilerState& cs, int arg) :
    cs_(cs),
    tvalue_(cs.proto_->k + arg) {
}

llvm::Value* Constant::GetTag() {
    return cs_.MakeInt(rttype(tvalue_));
}

llvm::Value* Constant::HasTag(int tag) {
    auto tbool = llvm::Type::getInt1Ty(cs_.context_);
    return cs_.MakeInt(rttype(tvalue_) == tag, tbool);
}

llvm::Value* Constant::GetTValue() {
    auto ttvalue = cs_.rt_.GetType("TValue");
    return cs_.InjectPointer(ttvalue, tvalue_);
}

llvm::Value* Constant::GetBoolean() {
    return cs_.MakeInt(bvalue(tvalue_));
}

llvm::Value* Constant::GetInteger() {
    auto tluainteger = cs_.rt_.GetType("lua_Integer");
    return cs_.MakeInt(ivalue(tvalue_), tluainteger);
}

llvm::Value* Constant::GetFloat() {
    auto tluanumber = cs_.rt_.GetType("lua_Number");
    return llvm::ConstantFP::get(tluanumber, nvalue(tvalue_));
}

llvm::Value* Constant::GetTString() {
    auto ttstring = cs_.rt_.GetType("TString");
    return cs_.InjectPointer(ttstring, tsvalue(tvalue_));
}

llvm::Value* Constant::GetGCValue() {
    auto tgcobject = cs_.rt_.GetType("GCObject");
    return cs_.InjectPointer(tgcobject, gcvalue(tvalue_));
}

MutableValue::MutableValue(CompilerState& cs, llvm::Value* tvalue) :
    cs_(cs),
    tvalue_(tvalue) {
}

llvm::Value* MutableValue::GetTag() {
    return cs_.builder_.CreateLoad(GetField(TAG), tvalue_->getName() + ".tag");
}

llvm::Value* MutableValue::HasTag(int tag) {
    return cs_.builder_.CreateICmpEQ(GetTag(), cs_.MakeInt(tag));
}

void MutableValue::SetTag(int tag) {
    SetTag(cs_.MakeInt(tag));
}

void MutableValue::SetTag(llvm::Value* tag) {
    cs_.builder_.CreateStore(tag, GetFieldPtr(TAG));
}

llvm::Value* MutableValue::GetTValue() {
    return tvalue_;
}

void MutableValue::Assign(Value& value) {
    this->SetTag(value.GetTag());
    cs_.builder_.CreateStore(value.GetInteger(), this->GetField(VALUE));
}

llvm::Value* MutableValue::GetBoolean() {

}

void MutableValue::SetBoolean(llvm::Value* bvalue) {
    SetTag(LUA_TBOOLEAN);
    cs_.builder_.CreateStore(bvalue, GetValuePtr("int", "bvalue"));
}

llvm::Value* MutableValue::GetInteger() {
    auto name = tvalue_->getName() + ".ivalue";
    return cs_.builder_.CreateLoad(GetField(Value), name);
}

void MutableValue::SetInteger(llvm::Value* ivalue) {
    SetTag(LUA_TNUMINT);
    cs_.builder_.CreateStore(ivalue, this->GetField(VALUE));
}

llvm::Value* MutableValue::GetFloat() {
    return GetValue("lua_Number", "nvalue");
}

void MutableValue::SetFloat(llvm::Value* fvalue) {
    SetTag(LUA_TNUMFLT);
    cs_.builder_.CreateStore(fvalue, GetValuePtr("lua_Number", "nvalue"));
}

llvm::Value* MutableValue::GetTString() {
    return GetValue("TString", "str");
}

llvm::Value* MutableValue::GetTable() {
    return GetValue("Table", "table");
}

llvm::Value* MutableValue::GetGCValue() {
    return GetValue("GCObject", "gc");
}

llvm::Value* MutableValue::GetField(Fields field) {
    auto indices = {MakeInt(0), MakeInt((int)field)};
    auto name = tvalue_->GetName() + (field == VALUE ? ".value.ptr"
                                                     : ".tag.ptr");
    return cs_.builder_.CreateGEP(tvalue_, indices, name);
}

llvm::Value* MutableValue::GetValue(const std::string& typestr,
        const std::string& name) {
    auto fullname = tvalue_->getName() + "." + name;
    return cs_.builder_.CreateLoad(GetValuePtr(typestr, fullname), fullname);

llvm::Value* MutableValue::GetValuePtr(const std::string& typestr,
        const std::string& name) {
    auto type = cs_.rt_.LoadType(typestr);
    return cs_.builder_.CreateBitCast(GetField(VALUE), type, name + ".ptr");
}

Register::Register(CompilerState& cs, int arg, const std::string& name) :
    MutableValue(cs, cs.builder_.CreateGEP(cs.GetBase(), cs.MakeInt(arg),
                                           name)) {
}

Upvalue::Upvalue(CompilerState& cs, int arg) :
    MutableValue(cs, ComputeTValue(cs, arg)) {
}

llvm::Value* Upvalue::ComputeTValue(CompilerState& cs, int arg) {
    auto upvals = GetFieldPtr(values_.closure, rt_.GetType("UpVal"),
            offsetof(LClosure, upvals), "closure.upvals");
    auto upvalptr = builder_.CreateGEP(upvals, MakeInt(n), "upval.ptr");
    auto upval = builder_.CreateLoad(upvalptr, "upval");
    return LoadField(upval, rt_.GetType("TValue"), offsetof(UpVal, v),
             "upval.v");
}

