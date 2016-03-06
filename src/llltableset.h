/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** llltableset.h
** 
** Implements the luaV_settable function
*/

#ifndef LLLTABLESET_H
#define LLLTABLESET_H

#include "lllopcode.h"

namespace lll {

class Value;

class TableSet : public Opcode {
public:
    // Constructor
    TableSet(CompilerState& cs, Value& table, Value& key, Value& value);

    // Compiles the opcode
    void Compile();

private:
    // Compilation steps
    void CheckTable();
    void SwithTag();
    void PerformGet();
    void CallGCBarrier();
    void FastSet();
    void FinishSet();

    // Call of a specific luaH_get*
    typedef llvm::Value* (Value::*GetMethod)();
    void PerformGetCase(llvm::BasicBlock* block, GetMethod getmethod,
            const char* suffix);

    Value& table_;
    Value& key_;
    Value& value_;
    llvm::Value* tablevalue_;
    llvm::Value* slot_;
    IncomingList slots_;
    IncomingList oldvals_;
    llvm::BasicBlock* switchtag_;
    llvm::BasicBlock* getint_;
    llvm::BasicBlock* getshrstr_;
    llvm::BasicBlock* getlngstr_;
    llvm::BasicBlock* getnil_;
    llvm::BasicBlock* getany_;
    llvm::BasicBlock* callgcbarrier_;
    llvm::BasicBlock* fastset_;
    llvm::BasicBlock* finishset_;
};

}

#endif

