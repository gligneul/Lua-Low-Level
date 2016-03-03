/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllcompilerstate.h
** Holds all compilation information and auxiliary functions
*/

#ifndef LLLCOMPILERSTATE_H
#define LLLCOMPILERSTATE_H

#include <vector>

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "lllruntime.h"

extern "C" {
#include "llimits.h"

struct Proto;
struct lua_State;
}

namespace lll {

class CompilerState {
public:
    CompilerState(lua_State* L, Proto* proto);

    // Makes a llvm int value
    llvm::Value* MakeInt(int value, llvm::Type* type = nullptr);

    // Converts an int to boolean
    llvm::Value* ToBool(llvm::Value* value);

    // Injects a pointer from host to jit
    llvm::Value* InjectPointer(llvm::Type* type, void* ptr);

    // Obtains the pointer to the field at $offset
    llvm::Value* GetFieldPtr(llvm::Value* strukt, llvm::Type* fieldtype,
            size_t offset, const std::string& name);

    // Loads the field at $offset
    llvm::Value* LoadField(llvm::Value* strukt, llvm::Type* fieldtype,
            size_t offset, const std::string& name);

    // Sets a field at $offset
    void SetField(llvm::Value* strukt, llvm::Value* fieldvalue, size_t offset,
            const std::string& fieldname);

    // Obtains the register $n at stack
    llvm::Value* GetValueR(int n, const std::string& name);

    // Obtains the constant $n at k-table
    llvm::Value* GetValueK(int n, const std::string& name);

    // Verifies if $n is a register of constant and returns it
    llvm::Value* GetValueRK(int n, const std::string& name);

    // Obtains the upvalue $n
    llvm::Value* GetUpval(int n);

    // Create a function call
    llvm::Value* CreateCall(const std::string& name,
            std::initializer_list<llvm::Value*> args,
            const std::string& retname = "");

    // Sets a register with a value
    void SetRegister(llvm::Value* reg, llvm::Value* value);

    // Updates base value
    void UpdateStack();

    // Sets L->top = ci->top
    void ReloadTop();

    // Assigns the top as the $n register
    void SetTop(int n);

    // Returns the ptrdiff from register $n to top
    llvm::Value* TopDiff(int n);

    // Creates a sub-block with $suffix
    llvm::BasicBlock* CreateSubBlock(const std::string& suffix,
            llvm::BasicBlock* preview = nullptr);

    // Prints a message inside the jitted function
    template<typename... Arguments>
    void DebugPrint(const std::string& format, Arguments... args) {
        auto function = module_->getFunction("printf");
        if (!function) {
            auto rettype = llvm::Type::getVoidTy(context_);
            auto paramtype = llvm::PointerType::get(rt_.MakeIntT(1), 0);
            auto functype = llvm::FunctionType::get(rettype, {paramtype}, true);
            function = llvm::Function::Create(functype,
                    llvm::Function::ExternalLinkage, "printf", module_.get());
        }
        auto callargs = {builder_.CreateGlobalStringPtr(format+ "\n"), args...};
        builder_.CreateCall(function, callargs);
    }

    lua_State* L_;
    Proto* proto_;
    llvm::LLVMContext& context_;
    Runtime& rt_;
    std::unique_ptr<llvm::Module> module_;
    llvm::Function* function_;
    std::vector<llvm::BasicBlock*> blocks_;
    llvm::IRBuilder<> builder_;
    struct {
        llvm::Value* state;
        llvm::Value* closure;
        llvm::Value* ci;
        llvm::Value* k;
        llvm::Value* base;
        llvm::Value* bnumber;
        llvm::Value* cnumber;
    } values_;
    int curr_;
    Instruction instr_;

private:
    // Creates the main function
    llvm::Function* CreateMainFunction();

    // Creates one basic block for each instruction
    void CreateBlocks();
};

}

#endif

