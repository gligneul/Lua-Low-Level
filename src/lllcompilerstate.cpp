/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllcompilerstate.cpp
*/

#include <sstream>

#include <llvm/IR/Constant.h>

#include "lllcompilerstate.h"

extern "C" {
#include "lprefix.h"
#include "lfunc.h"
#include "lopcodes.h"
#include "lstate.h"
}

namespace lll {

CompilerState::CompilerState(Proto* proto) :
    proto_(proto),
    context_(llvm::getGlobalContext()),
    rt_(*Runtime::Instance()),
    module_(new llvm::Module("lll_module", context_)),
    function_(CreateMainFunction()),
    blocks_(proto_->sizecode, nullptr),
    builder_(context_),
    curr_(0) {
    CreateBlocks();
}

llvm::Function* CompilerState::CreateMainFunction() {
    auto ret = rt_.MakeIntT(sizeof(int));
    auto params = {rt_.GetType("lua_State"), rt_.GetType("LClosure")};
    auto type = llvm::FunctionType::get(ret, params, false);
    std::stringstream name;
    name << "lll" << static_cast<void*>(proto_);
    return llvm::Function::Create(type, llvm::Function::ExternalLinkage,
            name.str(), module_.get());
}

void CompilerState::CreateBlocks() {
    auto& args = function_->getArgumentList();
    values_.state = &args.front();
    values_.state->setName("state");
    values_.closure = &args.back();
    values_.closure->setName("closure");

    auto entry = llvm::BasicBlock::Create(context_, "entry", function_);
    builder_.SetInsertPoint(entry);
    values_.ci = LoadField(values_.state, rt_.GetType("CallInfo"),
            offsetof(lua_State, ci), "ci");
    values_.luanumber = builder_.CreateAlloca(rt_.GetType("lua_Number"), 
            nullptr, "luanumber");

    for (size_t i = 0; i < blocks_.size(); ++i) {
        auto instruction = luaP_opnames[GET_OPCODE(proto_->code[i])];
        std::stringstream name;
        name << "block." << i << "." << instruction;
        blocks_[i] = llvm::BasicBlock::Create(context_, name.str(), function_);
    }

    builder_.CreateBr(blocks_[0]);
}

llvm::Value* CompilerState::MakeInt(int value) {
    return llvm::ConstantInt::get(rt_.MakeIntT(sizeof(int)), value);
}

llvm::Value* CompilerState::ToBool(llvm::Value* value) {
    return builder_.CreateICmpNE(value, MakeInt(0), value->getName());
}

llvm::Value* CompilerState::GetFieldPtr(llvm::Value* strukt,
        llvm::Type* fieldtype, size_t offset, const std::string& name) {
    auto memt = llvm::PointerType::get(rt_.MakeIntT(1), 0);
    auto mem = builder_.CreateBitCast(strukt, memt, strukt->getName() + "_mem");
    auto element = builder_.CreateGEP(mem, MakeInt(offset), name + "_mem");
    auto ptrtype = llvm::PointerType::get(fieldtype, 0);
    return builder_.CreateBitCast(element, ptrtype, name + "_ptr");
}

llvm::Value* CompilerState::LoadField(llvm::Value* strukt,
        llvm::Type* fieldtype, size_t offset, const std::string& name) {
    auto ptr = GetFieldPtr(strukt, fieldtype, offset, name);
    return builder_.CreateLoad(ptr, name);
}

void CompilerState::SetField(llvm::Value* strukt, llvm::Value* fieldvalue,
        size_t offset, const std::string& fieldname) {
    auto ptr = GetFieldPtr(strukt, fieldvalue->getType(), offset,fieldname);
    builder_.CreateStore(fieldvalue, ptr);
}

llvm::Value* CompilerState::GetValueR(int arg, const std::string& name) {
    return builder_.CreateGEP(values_.base, MakeInt(arg), name);
}

llvm::Value* CompilerState::GetValueK(int arg, const std::string& name) {
    auto k = (uintptr_t)(proto_->k + arg);
    auto intptr = llvm::ConstantInt::get(rt_.MakeIntT(sizeof(void*)), k);
    return builder_.CreateIntToPtr(intptr, rt_.GetType("TValue"), name);
}

llvm::Value* CompilerState::GetValueRK(int arg, const std::string& name) {
    return ISK(arg) ? GetValueK(INDEXK(arg), name) : GetValueR(arg, name);
}

llvm::Value* CompilerState::GetUpval(int n) {
    auto upvals = GetFieldPtr(values_.closure, rt_.GetType("UpVal"),
            offsetof(LClosure, upvals), "upvals");
    auto upvalptr = builder_.CreateGEP(upvals, MakeInt(n), "upval");
    auto upval = builder_.CreateLoad(upvalptr, "upval");
    return LoadField(upval, rt_.GetType("TValue"), offsetof(UpVal, v), "val");
}

llvm::Value* CompilerState::CreateCall(const std::string& name,
        std::initializer_list<llvm::Value*> args, const std::string& retname) {
    auto f = rt_.GetFunction(module_.get(), name);
    return builder_.CreateCall(f, args, retname);
}

void CompilerState::SetRegister(llvm::Value* reg, llvm::Value* value) {
    auto indices = {MakeInt(0), MakeInt(0)};
    auto value_memptr = builder_.CreateGEP(value, indices, "value_memptr");
    auto value_mem = builder_.CreateLoad(value_memptr, "value_mem"); 
    auto reg_memptr = builder_.CreateGEP(reg, indices, "reg_memptr");
    builder_.CreateStore(value_mem, reg_memptr);
}

void CompilerState::UpdateStack() {
    values_.base = LoadField(values_.ci, rt_.GetType("TValue"),
            offsetof(CallInfo, u.l.base), "base");
}

void CompilerState::ReloadTop() {
    auto top = LoadField(values_.ci, rt_.GetType("TValue"),
            offsetof(CallInfo, top), "top");
    SetField(values_.state, top, offsetof(lua_State, top), "top");
}

void CompilerState::SetTop(int reg) {
    auto top = GetValueR(reg, "top");
    SetField(values_.state, top, offsetof(lua_State, top), "top");
}

llvm::Value* CompilerState::TopDiff(int n) {
    auto top = LoadField(values_.state, rt_.GetType("TValue"),
            offsetof(lua_State, top), "top");
    auto r = GetValueR(n, "r");
    auto diff = builder_.CreatePtrDiff(top, r, "diff");
    auto inttype = rt_.MakeIntT(sizeof(int));
    return builder_.CreateIntCast(diff, inttype, false, "idiff");
}

llvm::BasicBlock* CompilerState::CreateSubBlock(const std::string& suffix,
            llvm::BasicBlock* preview) {
    if (!preview)
        preview = blocks_[curr_];
    auto name = blocks_[curr_]->getName() + "." +  suffix;
    auto block = llvm::BasicBlock::Create(context_, name, function_, preview);
    block->moveAfter(preview);
    return block;
}

}

