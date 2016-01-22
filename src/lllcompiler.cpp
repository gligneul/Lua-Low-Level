/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllcompiler.cpp
*/

#include <llvm/ADT/StringRef.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>

#include "lllcompiler.h"

extern "C" {
#include "lprefix.h"
#include "lfunc.h"
#include "lgc.h"
#include "lopcodes.h"
#include "luaconf.h"
#include "lvm.h"
#include "ltable.h"
}

namespace lll {

Compiler::Compiler(lua_State* L, LClosure* lclosure) :
    lua_state_(L),
    lclosure_(lclosure),
    engine_(nullptr) {
    (void)lua_state_;
    (void)lclosure_;
}

bool Compiler::Compile() {
    return true;
}

const std::string& Compiler::GetErrorMessage() {
    return error_;
}

Engine* Compiler::GetEngine() {
    return engine_;
}

}

