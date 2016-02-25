/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** runtime.h
** Singleton that manages functions/types used by compiled the function
*/

#ifndef LLLRUNTIME_H
#define LLLRUNTIME_H

#include <map>

#include <llvm/IR/LLVMContext.h>

#define STRINGFY(a) #a
#define STRINGFY2(a) STRINGFY(a)

namespace lll {

class Runtime {
public:
    // Gets the unique instance
    // Creates it when called for the first time
    static Runtime* Instance();

    // Obtains the type declaration
    llvm::Type* GetType(const std::string& name);

    // Obtains the function declaration
    llvm::Function* GetFunction(llvm::Module* module, const std::string& name);

    // Makes a llvm int type
    llvm::Type* MakeIntT(int nbytes);

private:
    Runtime();
    void InitTypes();
    void InitFunctions();

    // Adds a named struct type with $size bytes
    void AddStructType(const std::string& name, size_t size);

    // Adds a function that can be compiled
    void AddFunction(const std::string& name, llvm::FunctionType* type,
                     void* address);

    static Runtime* instance_;
    llvm::LLVMContext& context_;
    std::map<std::string, llvm::Type*> types_;
    std::map<std::string, llvm::FunctionType*> functions_;
};

}

#endif

