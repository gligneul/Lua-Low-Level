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

    // Constructor
    Value(CompilerState& cs, int arg, const std::string& name,
            ConversionType conversion = NO_CONVERSION);

    // Obtains the tag of the value
    llvm::Value* GetTag();

    // Obtains the integer value
    llvm::Value* GetInteger();

    // Obtains the float value
    llvm::Value* GetFloat();

    // Obtains the tstring value
    llvm::Value* GetTString();

    // Obtains the table value
    llvm::Value* GetTable();

    // Obtains the TValue
    llvm::Value* GetTValue();

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

