/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllopcode.h
** Superclass for opcodes compilations
*/

#ifndef LLLOPCODE_H
#define LLLOPCODE_H

#include <string>
#include <vector>

namespace llvm {
class BasicBlock;
class Value;
class Type;
}

namespace lll {

class CompilerState;

class Opcode {
public:
    // Default contructor
    Opcode(CompilerState& cs);

    // Compiles the opcode
    virtual void Compile() = 0;

protected:
    // List of incomming values and blocks
    typedef std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>
            IncomingList;

    // Creates a Phi value and adds it's incoming values
    llvm::Value* CreatePHI(llvm::Type* type, const IncomingList& incoming,
            const std::string& name);

    CompilerState& cs_;
    llvm::BasicBlock* entry_;
    llvm::BasicBlock* exit_;
};

}

#endif

