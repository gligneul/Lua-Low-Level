/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllcompiler.cpp
*/

#include <sstream>

#include <llvm/ADT/StringRef.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>

#include "lllcompiler.h"
#include "lllengine.h"
#include "lllruntime.h"

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

Compiler::Compiler(LClosure* lclosure) :
    lclosure_(lclosure),
    context_(llvm::getGlobalContext()),
    rt_(Runtime::Instance()),
    engine_(nullptr),
    module_(new llvm::Module("lll_module", context_)),
    function_(CreateMainFunction()),
    blocks_(lclosure->p->sizecode, nullptr),
    builder_(context_),
    curr_(0) {
    static bool init = true;
    if (init) {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();
        init = false;
    }
}

bool Compiler::Compile() {
    CreateBlocks();
    CompileInstructions();
    return VerifyModule() && CreateEngine();
}

const std::string& Compiler::GetErrorMessage() {
    return error_;
}

Engine* Compiler::GetEngine() {
    return engine_.release();
}

llvm::Function* Compiler::CreateMainFunction() {
    auto ret = llvm::IntegerType::get(context_, sizeof(int));
    std::vector<llvm::Type*> params = {
        rt_->GetType("lua_State"),
        rt_->GetType("LClosure")
    };
    auto type = llvm::FunctionType::get(ret, params, false);
    return llvm::Function::Create(type, llvm::Function::ExternalLinkage,
            "main", module_.get());
}

void Compiler::CreateBlocks() {
    auto& args = function_->getArgumentList();
    values_.state = &args.front();
    values_.state->setName("state");
    values_.closure = &args.back();
    values_.closure->setName("closure");

    auto entry = llvm::BasicBlock::Create(context_, "entry", function_);
    builder_.SetInsertPoint(entry);
    values_.ci = LoadField(values_.state, rt_->GetType("CallInfo"),
            offsetof(lua_State, ci), "ci");

    for (size_t i = 0; i < blocks_.size(); ++i) {
        auto instruction = luaP_opnames[GET_OPCODE(lclosure_->p->code[i])];
        std::stringstream name;
        name << "block." << i << "." << instruction;
        blocks_[i] = llvm::BasicBlock::Create(context_, name.str(), function_);
    }

    builder_.CreateBr(blocks_[0]);
}

Instruction Compiler::GetInstruction() {
    return lclosure_->p->code[curr_];
}

void Compiler::CompileInstructions() {
    for (curr_ = 0; curr_ < lclosure_->p->sizecode; ++curr_) {
        builder_.SetInsertPoint(blocks_[curr_]);
        switch(GET_OPCODE(GetInstruction())) {
            case OP_RETURN:   CompileReturn(); break;
            default: break;
            #if 0
            case OP_MOVE:     compile_move(cs); break;
            case OP_LOADK:    compile_loadk(cs); break;
            case OP_LOADKX:   compile_loadk(cs); break;
            case OP_LOADBOOL: compile_loadbool(cs); break;
            case OP_LOADNIL:  compile_loadnil(cs); break;
            case OP_GETUPVAL: /* TODO */ break;
            case OP_GETTABUP: compile_gettabup(cs); break;
            case OP_GETTABLE: compile_gettable(cs); break;
            case OP_SETTABUP: compile_settabup(cs); break;
            case OP_SETUPVAL: /* TODO */ break;
            case OP_SETTABLE: compile_settable(cs); break;
            case OP_NEWTABLE: compile_newtable(cs); break;
            case OP_SELF:     /* TODO */ break;
            case OP_ADD:      compile_binop(cs, luaJ_addrr); break;
            case OP_SUB:      compile_binop(cs, luaJ_subrr); break;
            case OP_MUL:      compile_binop(cs, luaJ_mulrr); break;
            case OP_MOD:      compile_binop(cs, luaJ_modrr); break;
            case OP_POW:      compile_binop(cs, luaJ_powrr); break;
            case OP_DIV:      compile_binop(cs, luaJ_divrr); break;
            case OP_IDIV:     compile_binop(cs, luaJ_idivrr); break;
            case OP_BAND:     compile_binop(cs, luaJ_bandrr); break;
            case OP_BOR:      compile_binop(cs, luaJ_borrr); break;
            case OP_BXOR:     compile_binop(cs, luaJ_bxorrr); break;
            case OP_SHL:      compile_binop(cs, luaJ_shlrr); break;
            case OP_SHR:      compile_binop(cs, luaJ_shrrr); break;
            case OP_UNM:      compile_unop(cs, luaJ_unm); break;
            case OP_BNOT:     compile_unop(cs, luaJ_bnot); break;
            case OP_NOT:      compile_unop(cs, luaJ_not); break;
            case OP_LEN:      compile_unop(cs, luaV_objlen); break;
            case OP_CONCAT:   compile_concat(cs); break;
            case OP_JMP:      compile_jmp(cs); break;
            case OP_EQ:       compile_cmp(cs, luaV_equalobj); break;
            case OP_LT:       compile_cmp(cs, luaV_lessthan); break;
            case OP_LE:       compile_cmp(cs, luaV_lessequal); break;
            case OP_TEST:     compile_test(cs); break;
            case OP_TESTSET:  compile_testset(cs); break;
            case OP_CALL:     compile_call(cs); break;
            case OP_TAILCALL: compile_call(cs); break;
            case OP_RETURN:   compile_return(cs); break;
            case OP_FORLOOP:  /* TODO */ break;
            case OP_FORPREP:  /* TODO */ break;
            case OP_TFORCALL: /* TODO */ break;
            case OP_TFORLOOP: /* TODO */ break;
            case OP_SETLIST:  /* TODO */ break;
            case OP_CLOSURE:  /* TODO */ break;
            case OP_VARARG:   /* TODO */ break;
            case OP_EXTRAARG: /* ignored */ break;
            #endif
        }
        if (!blocks_[curr_]->getTerminator())
            builder_.CreateBr(blocks_[curr_ + 1]);
    }
}

void Compiler::CompileReturn() {
    builder_.CreateRet(CreateInt(0));
}

bool Compiler::VerifyModule() {
    llvm::raw_string_ostream error_os(error_);
    bool err = llvm::verifyModule(*module_, &error_os);
    if (err)
        module_->dump();
    return !err;
}

bool Compiler::CreateEngine() {
    auto module = module_.get();
    auto engine = llvm::EngineBuilder(module_.release())
            .setErrorStr(&error_)
            .setOptLevel(llvm::CodeGenOpt::Aggressive)
            .setEngineKind(llvm::EngineKind::JIT)
            .setUseMCJIT(true)
            .create();
    if (engine) {
        engine->finalizeObject();
        engine_.reset(new Engine(engine, module));
        return true;
    } else {
        return false;
    }
}

llvm::Value* Compiler::CreateInt(int value) {
    return llvm::ConstantInt::get(
            llvm::IntegerType::get(context_, sizeof(int)), value);
}

llvm::Value* Compiler::GetFieldPtr(llvm::Value* strukt, llvm::Type* fieldtype,
        size_t offset, const std::string& name) {
    auto memt = llvm::PointerType::get(llvm::IntegerType::get(context_, 8), 0);
    auto mem = builder_.CreateBitCast(strukt, memt, strukt->getName() + "_mem");
    auto element = builder_.CreateGEP(mem, CreateInt(offset), name + "_mem");
    return builder_.CreateBitCast(element, fieldtype, name + "_ptr");
}

llvm::Value* Compiler::LoadField(llvm::Value* strukt, llvm::Type* fieldtype,
        size_t offset, const std::string& name) {
    auto ptr = GetFieldPtr(strukt, fieldtype, offset, name);
    return builder_.CreateLoad(ptr, name);
}

}

