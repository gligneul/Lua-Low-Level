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
    llvm::BasicBlock* CheckTable(llvm::BasicBlock* entry);
    llvm::BasicBlock* SwithTag(llvm::BasicBlock* entry);
    llvm::BasicBlock* PerformGet(llvm::BasicBlock* entry);
    llvm::BasicBlock* SearchForTM(llvm::BasicBlock* entry);
    llvm::BasicBlock* SaveResult(llvm::BasicBlock* entry);
    llvm::BasicBlock* FinishGet(llvm::BasicBlock* entry);

    // Call of a specific luaH_get*
    typedef llvm::Value* (Value::*GetMethod)();
    void PerformGetCase(llvm::BasicBlock* block, GetMethod getmethod,
            const char* suffix);

    Value& table_;
    Value& key_;
    Value& dest_;
    llvm::Value* tablevalue_;
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> results_;
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> tms_;
    llvm::BasicBlock* entry_;
    llvm::BasicBlock* switchtag_;
    llvm::BasicBlock* getint_;
    llvm::BasicBlock* getshrstr_;
    llvm::BasicBlock* getlngstr_;
    llvm::BasicBlock* getany_;
    llvm::BasicBlock* saveresult_;
    llvm::BasicBlock* searchtm_;
    llvm::BasicBlock* finishget_;
    llvm::BasicBlock* end_;
};

}

#endif

