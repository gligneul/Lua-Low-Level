/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllvalue.h
** Abstraction for Lua's TValue.
*/

#ifndef LLLVALUE_H
#define LLLVALUE_H

#include <string>

namespace llvm {
class Value;
}

extern "C" {
struct lua_TValue;
}

namespace lll {

class CompilerState;

// The top abstraction for constant, registers and upvalues
class Value {
public:
    // Creates a Register or a Constant, depending on arg
    static std::unique_ptr<Value> CreateRK(CompilerState& cs, int arg,
            const std::string& name = "");

    // Creates a Constant
    static std::unique_ptr<Constant> CreateK(CompilerState& cs, int arg);

    // Creates a Register
    static std::unique_ptr<Register> CreateR(CompilerState& cs, int arg,
            const std::string& name = "");
    
    // Creates a Upvalue
    static std::unique_ptr<Upvalue> CreateUpval(CompilerState& cs, int arg);

    // Manipulates the tag of the value
    virtual llvm::Value* GetTag() = 0;
    virtual llvm::Value* HasTag(int tag) = 0;

    // Obtains the TValue (this should only be used when calling functions)
    virtual llvm::Value* GetTValue() = 0;

    // Access methods
    virtual llvm::Value* GetBoolean() = 0;
    virtual llvm::Value* GetInteger() = 0;
    virtual llvm::Value* GetFloat() = 0;
    virtual llvm::Value* GetTString() = 0;
    virtual llvm::Value* GetGCValue() = 0;
};

// Implementation of a constant TValue
class Constant : public Value {
public:
    // Constructor
    Constant(CompilerState& cs, int arg);

    // Manipulates the tag of the value
    llvm::Value* GetTag();
    llvm::Value* HasTag(int tag);

    // Obtains the TValue (this should only be used when calling functions)
    llvm::Value* GetTValue();

    // Access methods
    llvm::Value* GetBoolean();
    llvm::Value* GetInteger();
    llvm::Value* GetFloat();
    llvm::Value* GetTString();
    llvm::Value* GetGCValue();

private:
    CompilerState& cs_;
    struct lua_TValue* tvalue_;
};

// Represents a mutable TValue, can be either a upvalue or a register
class MutableValue : public Value {
public:
    // Constructor
    MutableValue(CompilerState& cs, llvm::Value* tvalue);

    // Manipulates the tag of the value
    llvm::Value* GetTag();
    llvm::Value* HasTag(int tag);
    void SetTag(int tag);
    void SetTag(llvm::Value* tag);

    // Obtains the TValue (this should only be used when calling functions)
    llvm::Value* GetTValue();

    // Copy the contents of $value to this
    void Assign(Value& value);

    // Boolean access methods
    llvm::Value* GetBoolean();
    void SetBoolean(llvm::Value* bvalue);

    // Integer access methods
    llvm::Value* GetInteger();
    void SetInteger(llvm::Value* ivalue);

    // Float access methods
    llvm::Value* GetFloat();
    void SetFloat(llvm::Value* fvalue);

    // Other values access methods
    llvm::Value* GetValue(
    llvm::Value* GetTString();
    llvm::Value* GetTable();
    llvm::Value* GetGCValue();

protected:
    enum Fields {
        VALUE = 0,
        TAG = 1
    };

    // Obtains the pointer to a field
    llvm::Value* GetField(Field field);

    // Obtains the pointer to a value and cast it to $type
    llvm::Value* GetValue(const std::string& typestr, const std::string& name);
    llvm::Value* GetValuePtr(const std::string& typestr,
            const std::string& name);

    CompilerState& cs_;
    llvm::Value* tvalue_;
};

// Represents a register of lua stack
class Register : public MutableValue {
public:
    // Constructor
    Register(CompilerState& cs, int arg, const std::string& name);
};

// Represents an upvalue
class Upvalue : public MutableValue {
public:
    // Constructor
    Upvalue(CompilerState& cs, int arg);

private:
    static llvm::Value* ComputeTValue(CompilerState& cs, int arg);
};

}

#endif

