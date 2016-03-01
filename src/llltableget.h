/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** llltableget.h
** Gets an element from $table with the $key and stores it at $dest.
** This class will check for compile-time-known tags and call the appropriate
** luaH_get* function.
*/

#ifndef LLLTABLEGET_H
#define LLLTABLEGET_H

namespace llvm {
class BasicBlock;
}

#include "lllopcode.h"

namespace lll {

class Value;
class CompilerState;

class TableGet : public Opcode<TableGet> {
public:
    // Constructor
    TableGet(CompilerState& cs, Value& table, Value& key, Value& dest);

    // Returns the list of steps
    std::vector<CompilationStep> GetSteps();

private:
    // Compilation steps
    void CheckTable();
    void SwithTag();
    void PerformGet();
    void SearchForTM();
    void SaveResult();
    void FinishGet();

    // Call of a specific luaH_get*
    typedef llvm::Value* (Value::*GetMethod)();
    void PerformGetCase(llvm::BasicBlock* block, GetMethod getmethod,
            const char* suffix);

    Value& table_;
    Value& key_;
    Value& dest_;
    llvm::Value* tablevalue_;
    IncomingList results_;
    IncomingList tms_;
    llvm::BasicBlock* switchtag_;
    llvm::BasicBlock* getint_;
    llvm::BasicBlock* getshrstr_;
    llvm::BasicBlock* getlngstr_;
    llvm::BasicBlock* getany_;
    llvm::BasicBlock* saveresult_;
    llvm::BasicBlock* searchtm_;
    llvm::BasicBlock* finishget_;
};

}

#endif

