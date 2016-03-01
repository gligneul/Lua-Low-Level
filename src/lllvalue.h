/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllvalue.h
** Abstraction for Lua's tagged value.
** This wraps registers and constants, allowing optmizations.
*/

#ifndef LLLVALUE_H
#define LLLVALUE_H

#include <string>

namespace llvm {class Value;}

extern "C" {
struct lua_TValue;
}

namespace lll {

class CompilerState;

class Value {
public:
    // Constant conversions while gathering values information
    enum ConversionType {
        NO_CONVERSION,
        STRING_TO_FLOAT
    };

    // Constructor that receives a opcode argument
    Value(CompilerState& cs, int arg, const std::string& name,
            ConversionType conversion = NO_CONVERSION);

    // Constructor that receives a non-const tvalue
    Value(CompilerState& cs, llvm::Value* v);

    // Obtains the tag of the value
    llvm::Value* GetTag();

    // Obtains the TValue
    llvm::Value* GetTValue();

    // Copy the contents of $value to this
    void SetValue(Value& value);

    // Integer access methods
    llvm::Value* IsInteger();
    llvm::Value* GetInteger();
    void SetInteger(llvm::Value* value);

    // Float access methods
    llvm::Value* GetFloat();
    void SetFloat(llvm::Value* value);

    // String access methods
    llvm::Value* GetTString();

    // Table access methods
    llvm::Value* IsTable();
    llvm::Value* GetTable();

private:
    CompilerState& cs_;
    ConversionType conversion_;
    bool isk_;
    union {
        llvm::Value* r;
        struct lua_TValue* k;
    } u_;
};

}

#endif

