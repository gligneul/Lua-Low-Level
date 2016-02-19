/*
** LLL - Lua Low Level
** September, 2015
** Author: Gabriel de Quadros Ligneul
** Copyright Notice for LLL: see lllcore.h
**
** lllcompiler.cpp
*/

#include <iostream>
#include <sstream>

#include <llvm/ADT/StringRef.h>
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

Compiler::Compiler(Proto* proto) :
    proto_(proto),
    context_(llvm::getGlobalContext()),
    rt_(Runtime::Instance()),
    engine_(nullptr),
    module_(new llvm::Module("lll_module", context_)),
    function_(CreateMainFunction()),
    blocks_(proto_->sizecode, nullptr),
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
    auto ret = MakeIntT(sizeof(int));
    auto params = {rt_->GetType("lua_State"), rt_->GetType("LClosure")};
    auto type = llvm::FunctionType::get(ret, params, false);
    return llvm::Function::Create(type, llvm::Function::ExternalLinkage,
            "lll", module_.get());
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
        auto instruction = luaP_opnames[GET_OPCODE(proto_->code[i])];
        std::stringstream name;
        name << "block." << i << "." << instruction;
        blocks_[i] = llvm::BasicBlock::Create(context_, name.str(), function_);
    }

    builder_.CreateBr(blocks_[0]);
}

void Compiler::CompileInstructions() {
    for (curr_ = 0; curr_ < proto_->sizecode; ++curr_) {
        builder_.SetInsertPoint(blocks_[curr_]);
        instr_ = proto_->code[curr_];
        UpdateStack();
        switch(GET_OPCODE(instr_)) {
            case OP_MOVE:     CompileMove(); break;
            case OP_LOADK:    CompileLoadk(false); break;
            case OP_LOADKX:   CompileLoadk(true); break;
            case OP_LOADBOOL: CompileLoadbool(); break;
            case OP_LOADNIL:  CompileLoadnil(); break;
            case OP_GETUPVAL: CompileGetupval(); break;
            case OP_GETTABUP: CompileGettabup(); break;
            case OP_GETTABLE: CompileGettable(); break;
            case OP_SETTABUP: CompileSettabup(); break;
            case OP_SETUPVAL: CompileSetupval(); break;
            case OP_SETTABLE: CompileSettable(); break;
            case OP_NEWTABLE: CompileNewtable(); break;
            case OP_SELF:     CompileSelf(); break;
            //case OP_ADD:      CompileArithIF("LLLAdd"); break;
            case OP_ADD:      CompileBinop("lll_addrr"); break;
            case OP_SUB:      CompileBinop("lll_subrr"); break;
            case OP_MUL:      CompileBinop("lll_mulrr"); break;
            case OP_MOD:      CompileBinop("lll_modrr"); break;
            case OP_POW:      CompileBinop("lll_powrr"); break;
            case OP_DIV:      CompileBinop("lll_divrr"); break;
            case OP_IDIV:     CompileBinop("lll_idivrr"); break;
            case OP_BAND:     CompileBinop("lll_bandrr"); break;
            case OP_BOR:      CompileBinop("lll_borrr"); break;
            case OP_BXOR:     CompileBinop("lll_bxorrr"); break;
            case OP_SHL:      CompileBinop("lll_shlrr"); break;
            case OP_SHR:      CompileBinop("lll_shrrr"); break;
            case OP_UNM:      CompileUnop("lll_unm"); break;
            case OP_BNOT:     CompileUnop("lll_bnot"); break;
            case OP_NOT:      CompileUnop("lll_not"); break;
            case OP_LEN:      CompileUnop("luaV_objlen"); break;
            case OP_CONCAT:   CompileConcat(); break;
            case OP_JMP:      CompileJmp(); break;
            case OP_EQ:       CompileCmp("luaV_equalobj"); break;
            case OP_LT:       CompileCmp("luaV_lessthan"); break;
            case OP_LE:       CompileCmp("luaV_lessequal"); break;
            case OP_TEST:     CompileTest(); break;
            case OP_TESTSET:  CompileTestset(); break;
            case OP_CALL:     CompileCall(); break;
            case OP_TAILCALL: CompileCall(); break;
            case OP_RETURN:   CompileReturn(); break;
            case OP_FORLOOP:  CompileForloop(); break;
            case OP_FORPREP:  CompileForprep(); break;
            case OP_TFORCALL: CompileTforcall(); break;
            case OP_TFORLOOP: CompileTforloop(); break;
            case OP_SETLIST:  CompileSetlist(); break;
            case OP_CLOSURE:  CompileClosure(); break;
            case OP_VARARG:   CompileVararg(); break;
            case OP_EXTRAARG: /* ignored */ break;
        }
        if (!blocks_[curr_]->getTerminator())
            builder_.CreateBr(blocks_[curr_ + 1]);
    }
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
            .setOptLevel(OPT_LEVEL)
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

void Compiler::CompileMove() {
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    auto rb = GetValueR(GETARG_B(instr_), "rb");
    SetRegister(ra, rb);
}

void Compiler::CompileLoadk(bool extraarg) {
    int kposition = extraarg ? GETARG_Ax(proto_->code[curr_ + 1])
                             : GETARG_Bx(instr_);
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    auto k = GetValueK(kposition, "k");
    SetRegister(ra, k);
}

void Compiler::CompileLoadbool() {
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    SetField(ra, MakeInt(GETARG_B(instr_)), offsetof(TValue, value_), "value");
    SetField(ra, MakeInt(LUA_TBOOLEAN), offsetof(TValue, tt_), "tag");
    if (GETARG_C(instr_))
        builder_.CreateBr(blocks_[curr_ + 2]);
}

void Compiler::CompileLoadnil() {
    int start = GETARG_A(instr_);
    int end = start + GETARG_B(instr_);
    for (int i = start; i <= end; ++i) {
        auto r = GetValueR(i, "r");
        SetField(r, MakeInt(LUA_TNIL), offsetof(TValue, tt_), "tag");
    }
}

void Compiler::CompileGetupval() {
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    auto upval = GetUpval(GETARG_B(instr_));
    SetRegister(ra, upval);
}

void Compiler::CompileGettabup() {
    auto args = {
        values_.state,
        GetUpval(GETARG_B(instr_)),
        GetValueRK(GETARG_C(instr_), "rkc"),
        GetValueR(GETARG_A(instr_), "ra")
    };
    CreateCall("LLLGetTable", args);
}

void Compiler::CompileGettable() {
    auto args = {
        values_.state,
        GetValueR(GETARG_B(instr_), "rb"),
        GetValueRK(GETARG_C(instr_), "rkc"),
        GetValueR(GETARG_A(instr_), "ra")
    };
    CreateCall("LLLGetTable", args);
}

void Compiler::CompileSettabup() {
    auto args = {
        values_.state,
        GetUpval(GETARG_A(instr_)), 
        GetValueRK(GETARG_B(instr_), "rkb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    CreateCall("LLLSetTable", args);
}

void Compiler::CompileSetupval() {
    auto upvals = LoadField(values_.closure, rt_->GetType("UpVal"),
            offsetof(LClosure, upvals), "upvals");
    auto upval = builder_.CreateGEP(upvals, MakeInt(GETARG_B(instr_)), "upval");
    auto v = LoadField(upval, rt_->GetType("TValue"), offsetof(UpVal, v), "v");
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    SetRegister(v, ra);
    auto args = {values_.state, upval};
    CreateCall("lll_upvalbarrier", args);
}

void Compiler::CompileSettable() {
    auto args = {
        values_.state,
        GetValueR(GETARG_A(instr_), "ra"),
        GetValueRK(GETARG_B(instr_), "rkb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    CreateCall("LLLSetTable", args);
}

void Compiler::CompileNewtable() {
    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    int c = GETARG_C(instr_);
    auto args = {values_.state, GetValueR(a, "ra")};
    auto table = CreateCall("lll_newtable", args);
    if (b != 0 || c != 0) {
        args = {
            values_.state,
            table,
            MakeInt(luaO_fb2int(b)),
            MakeInt(luaO_fb2int(c))
        };
        CreateCall("luaH_resize", args);
    }
    CompileCheckcg(GetValueR(a + 1, "ra1"));
}

void Compiler::CompileSelf() {
    auto args = {
        values_.state,
        GetValueR(GETARG_A(instr_), "ra"),
        GetValueR(GETARG_B(instr_), "rb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    CreateCall("LLLSelf", args);
}

void Compiler::CompileArithIF(const std::string& function) {
    int b = GETARG_B(instr_);
    int c = GETARG_C(instr_);
    auto args = {
        values_.state,
        GetValueR(GETARG_A(instr_), "ra"),
        GetValueRIF(b, "rkb"),
        GetValueRIF(c, "rkc")
    };
    CreateCall(function + GetTypeRIF(b) + GetTypeRIF(c), args);
}

void Compiler::CompileBinop(const std::string& function) {
    auto args = {
        values_.state,
        GetValueR(GETARG_A(instr_), "ra"),
        GetValueRK(GETARG_B(instr_), "rkb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    CreateCall(function, args);
}

void Compiler::CompileUnop(const std::string& function) {
    auto args = {
        values_.state,
        GetValueR(GETARG_A(instr_), "ra"),
        GetValueRK(GETARG_B(instr_), "rkb")
    };
    CreateCall(function, args);
}

void Compiler::CompileConcat() {
    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    int c = GETARG_C(instr_);
    SetTop(c + 1);
    auto args = {values_.state, MakeInt(c - b + 1)};
    CreateCall("luaV_concat", args);

    auto ra = GetValueR(a, "ra");
    auto rb = GetValueR(b, "rb");
    SetRegister(ra, rb);

    if (a >= b)
        CompileCheckcg(GetValueR(a + 1, "ra_next"));
    else
        CompileCheckcg(rb);

    ReloadTop();
}

void Compiler::CompileJmp() {
    builder_.CreateBr(blocks_[curr_ + GETARG_sBx(instr_) + 1]);
}

void Compiler::CompileCmp(const std::string& function) {
    auto args = {
        values_.state,
        GetValueRK(GETARG_B(instr_), "rkb"),
        GetValueRK(GETARG_C(instr_), "rkc")
    };
    auto result = CreateCall(function, args, "result");
    auto a = MakeInt(GETARG_A(instr_));
    auto cmp = builder_.CreateICmpNE(result, a, "cmp");
    builder_.CreateCondBr(cmp, blocks_[curr_ + 2], blocks_[curr_ + 1]);
}

void Compiler::CompileTest() {
    auto args = {MakeInt(GETARG_C(instr_)), GetValueR(GETARG_A(instr_), "ra")};
    auto result = ToBool(CreateCall("lll_test", args, "result"));
    builder_.CreateCondBr(result, blocks_[curr_ + 2], blocks_[curr_ + 1]);
}

void Compiler::CompileTestset() {
    auto rb = GetValueR(GETARG_B(instr_), "rb");
    auto args = {MakeInt(GETARG_C(instr_)), rb};
    auto result = ToBool(CreateCall("lll_test", args, "result"));
    auto setblock = CreateSubBlock("set");
    builder_.CreateCondBr(result, blocks_[curr_ + 2], setblock);
    builder_.SetInsertPoint(setblock);
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    SetRegister(ra, rb);
    builder_.CreateBr(blocks_[curr_ + 1]);
}

void Compiler::CompileCall() {
    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    if (b != 0)
        SetTop(a + b);
    auto args = {
        values_.state,
        GetValueR(a, "ra"),
        MakeInt(GETARG_C(instr_) - 1)
    };
    CreateCall("luaD_callnoyield", args);
}

void Compiler::CompileReturn() {
    if (proto_->sizep > 0)
        CreateCall("luaF_close", {values_.state, values_.base});
    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    llvm::Value* nresults = nullptr;
    if (b == 0) {
        nresults = TopDiff(GETARG_A(instr_));
    } else if (b == 1) { 
        nresults = MakeInt(0);
    } else {
        nresults = MakeInt(b - 1);
        SetTop(a + b - 1);
    }
    builder_.CreateRet(nresults);
}

void Compiler::CompileForloop() {
    auto ra = GetValueR(GETARG_A(instr_), "ra");
    auto jump = ToBool(CreateCall("lll_forloop", {ra}, "jump"));
    auto jumpblock = blocks_[curr_ + 1 + GETARG_sBx(instr_)];
    builder_.CreateCondBr(jump, jumpblock, blocks_[curr_ + 1]);
}

void Compiler::CompileForprep() {
    auto args = {values_.state, GetValueR(GETARG_A(instr_), "ra")};
    CreateCall("lll_forprep", args);
    builder_.CreateBr(blocks_[curr_ + 1 + GETARG_sBx(instr_)]);
}

void Compiler::CompileTforcall() {
    int a = GETARG_A(instr_);
    int cb = a + 3;
    SetRegister(GetValueR(cb + 2, "cb2"), GetValueR(a + 2, "ra2"));
    SetRegister(GetValueR(cb + 1, "cb1"), GetValueR(a + 1, "ra1"));
    SetRegister(GetValueR(cb, "cb"), GetValueR(a, "ra"));
    SetTop(cb + 3);
    auto args = {
        values_.state,
        GetValueR(cb, "cb"),
        MakeInt(GETARG_C(instr_))
    };
    CreateCall("luaD_callnoyield", args);
    ReloadTop();
}

void Compiler::CompileTforloop() {
    int a = GETARG_A(instr_);
    auto ra1 = GetValueR(a + 1, "ra1");
    auto tag = LoadField(ra1, MakeIntT(sizeof(int)), offsetof(TValue, tt_),
            "tag");
    auto notnil = builder_.CreateICmpNE(tag, MakeInt(LUA_TNIL), "notnil");
    auto continueblock = CreateSubBlock("continue");
    builder_.CreateCondBr(notnil, continueblock, blocks_[curr_ + 1]);

    builder_.SetInsertPoint(continueblock);
    auto ra = GetValueR(a, "ra");
    SetRegister(ra, ra1);
    builder_.CreateBr(blocks_[curr_ + 1 + GETARG_sBx(instr_)]);
}

void Compiler::CompileSetlist() {

    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);
    int c = GETARG_C(instr_);
    if (c == 0)
        c = GETARG_Ax(proto_->code[curr_ + 1]);

    auto n = (b != 0 ? MakeInt(b) : TopDiff(a + 1));
    auto fields = MakeInt((c - 1) * LFIELDS_PER_FLUSH);

    auto args = {values_.state, GetValueR(a, "ra"), fields, n};
    CreateCall("lll_setlist", args);
    ReloadTop();
}

void Compiler::CompileClosure() {
    auto args = {
        values_.state,
        values_.closure,
        values_.base,
        GetValueR(GETARG_A(instr_), "ra"),
        MakeInt(GETARG_Bx(instr_))
    };
    CreateCall("lll_closure", args);
    CompileCheckcg(GetValueR(GETARG_A(instr_) + 1, "ra1"));
}

void Compiler::CompileVararg() {
    // TODO: Create a separate class/module
    int a = GETARG_A(instr_);
    int b = GETARG_B(instr_);

    // Create blocks
    auto entryblock = blocks_[curr_];
    auto movecheckblock = CreateSubBlock("movecheck");
    auto moveblock = CreateSubBlock("move", movecheckblock);
    auto fillcheckblock = CreateSubBlock("fillcheck", moveblock);
    auto fillblock = CreateSubBlock("fill", fillcheckblock);
    auto endblock = blocks_[curr_ + 1];

    // Compute 'n' (available arguments)
    auto func = LoadField(values_.ci, rt_->GetType("TValue"),
            offsetof(CallInfo, func), "func");
    auto vadiff = builder_.CreatePtrDiff(values_.base, func, "vadiff");
    auto vasize = builder_.CreateIntCast(vadiff, MakeIntT(sizeof(int)), false,
            "vasize");
    auto n = builder_.CreateSub(vasize, MakeInt(proto_->numparams + 1),
            "n");

    // if n < 0 then n = 0 end
    auto nge0 = builder_.CreateICmpSGE(n, MakeInt(0), "n.ge.0");
    auto nge0int = builder_.CreateIntCast(nge0, MakeIntT(sizeof(int)), false,
            "n.ge.0_int");
    auto available = builder_.CreateMul(nge0int, n, "available");

    // Compute 'b' (required arguments)
    auto required = MakeInt(b - 1);
    if (b == 0) {
        required = available;
        CreateCall("lll_checkstack", {values_.state, available});
        UpdateStack();
        auto topidx = builder_.CreateAdd(MakeInt(a), n, "topidx");
        auto top = builder_.CreateGEP(values_.base, topidx, "top");
        SetField(values_.state, top, offsetof(lua_State, top), "top");
    }
    builder_.CreateBr(movecheckblock);

    // movecheckblock
    builder_.SetInsertPoint(movecheckblock);
    auto i = builder_.CreatePHI(MakeIntT(sizeof(int)), 2, "i");
    i->addIncoming(MakeInt(0), entryblock);
    i->addIncoming(builder_.CreateAdd(i, MakeInt(1)), moveblock);
    // TODO: Compare with the smallest value only
    auto iltrequired = builder_.CreateICmpSLT(i, required, "i.lt.required");
    auto iltavailable = builder_.CreateICmpSLT(i, available, "i.lt.available");
    auto cond = builder_.CreateAnd(iltrequired, iltavailable);
    builder_.CreateCondBr(cond, moveblock, fillcheckblock);

    // moveblock
    {
    builder_.SetInsertPoint(moveblock);
    auto validx = builder_.CreateSub(i, available, "validx");
    auto val = builder_.CreateGEP(values_.base, validx, "val");
    auto regidx = builder_.CreateAdd(MakeInt(a), i, "regidx");
    auto reg = builder_.CreateGEP(values_.base, regidx, "reg");
    SetRegister(reg, val);
    builder_.CreateBr(movecheckblock);
    }

    // fillcheckblock
    builder_.SetInsertPoint(fillcheckblock);
    auto j = builder_.CreatePHI(MakeIntT(sizeof(int)), 2, "j");
    j->addIncoming(i, movecheckblock);
    j->addIncoming(builder_.CreateAdd(j, MakeInt(1)), fillblock);
    auto jltrequired = builder_.CreateICmpSLT(j, required, "j.lt.required");
    builder_.CreateCondBr(jltrequired, fillblock, endblock);

    // fillblock
    {
    builder_.SetInsertPoint(fillblock);
    auto regidx = builder_.CreateAdd(MakeInt(a), j, "regidx");
    auto reg = builder_.CreateGEP(values_.base, regidx, "reg");
    SetField(reg, MakeInt(LUA_TNIL), offsetof(TValue, tt_), "tag");
    builder_.CreateBr(fillcheckblock);
    }
}

void Compiler::CompileCheckcg(llvm::Value* reg) {
    auto args = {values_.state, values_.ci, reg};
    CreateCall("lll_checkcg", args);
}

llvm::Type* Compiler::MakeIntT(int nbytes) {
    return llvm::IntegerType::get(context_, 8 * nbytes);
}

llvm::Value* Compiler::MakeInt(int value) {
    return llvm::ConstantInt::get(MakeIntT(sizeof(int)), value);
}

llvm::Value* Compiler::ToBool(llvm::Value* value) {
    return builder_.CreateICmpNE(value, MakeInt(0), value->getName());
}

llvm::Value* Compiler::GetFieldPtr(llvm::Value* strukt, llvm::Type* fieldtype,
        size_t offset, const std::string& name) {
    auto memt = llvm::PointerType::get(MakeIntT(1), 0);
    auto mem = builder_.CreateBitCast(strukt, memt, strukt->getName() + "_mem");
    auto element = builder_.CreateGEP(mem, MakeInt(offset), name + "_mem");
    auto ptrtype = llvm::PointerType::get(fieldtype, 0);
    return builder_.CreateBitCast(element, ptrtype, name + "_ptr");
}

llvm::Value* Compiler::LoadField(llvm::Value* strukt, llvm::Type* fieldtype,
        size_t offset, const std::string& name) {
    auto ptr = GetFieldPtr(strukt, fieldtype, offset, name);
    return builder_.CreateLoad(ptr, name);
}

void Compiler::SetField(llvm::Value* strukt, llvm::Value* fieldvalue,
        size_t offset, const std::string& fieldname) {
    auto ptr = GetFieldPtr(strukt, fieldvalue->getType(), offset,fieldname);
    builder_.CreateStore(fieldvalue, ptr);
}

llvm::Value* Compiler::GetValueR(int arg, const std::string& name) {
    return builder_.CreateGEP(values_.base, MakeInt(arg), name);
}

llvm::Value* Compiler::GetValueK(int arg, const std::string& name) {
    auto k = proto_->k + arg;
    auto intptr = llvm::ConstantInt::get(MakeIntT(sizeof(void*)), (uintptr_t)k);
    return builder_.CreateIntToPtr(intptr, rt_->GetType("TValue"), name);
}

llvm::Value* Compiler::GetValueRK(int arg, const std::string& name) {
    return ISK(arg) ? GetValueK(INDEXK(arg), name) : GetValueR(arg, name);
}

llvm::Value* Compiler::GetValueRIF(int arg,
        const std::string& name) {
    if (ISK(arg)) {
        int karg = INDEXK(arg);
        auto k = proto_->k + karg;
        if (ttisinteger(k)) {
            auto type = rt_->GetType("lua_Integer");
            return llvm::ConstantInt::get(type, ivalue(k));
        } else if (ttisfloat(k)) {
            auto type = rt_->GetType("lua_Number");
            return llvm::ConstantFP::get(type, fltvalue(k));
        } else {
            return GetValueK(karg, name);
        }
    } else {
         return GetValueR(arg, name);
    }
}

const char* Compiler::GetTypeRIF(int arg) {
    if (ISK(arg)) {
        auto k = proto_->k + INDEXK(arg);
        if (ttisinteger(k))
            return "I";
        else if (ttisfloat(k))
            return "F";
        else
            return "R";
    } else {
        return "R";
    }
}

llvm::Value* Compiler::GetUpval(int n) {
    auto upvals = GetFieldPtr(values_.closure, rt_->GetType("UpVal"),
            offsetof(LClosure, upvals), "upvals");
    auto upvalptr = builder_.CreateGEP(upvals, MakeInt(n), "upval");
    auto upval = builder_.CreateLoad(upvalptr, "upval");
    return LoadField(upval, rt_->GetType("TValue"), offsetof(UpVal, v), "val");
}

llvm::Value* Compiler::CreateCall(const std::string& name,
        std::initializer_list<llvm::Value*> args, const std::string& retname) {
    auto f = rt_->GetFunction(module_.get(), name);
    return builder_.CreateCall(f, args, retname);
}

void Compiler::SetRegister(llvm::Value* reg, llvm::Value* value) {
    auto indices = {MakeInt(0), MakeInt(0)};
    auto value_memptr = builder_.CreateGEP(value, indices, "value_memptr");
    auto value_mem = builder_.CreateLoad(value_memptr, "value_mem"); 
    auto reg_memptr = builder_.CreateGEP(reg, indices, "reg_memptr");
    builder_.CreateStore(value_mem, reg_memptr);
}

void Compiler::UpdateStack() {
    values_.base = LoadField(values_.ci, rt_->GetType("TValue"),
            ((uintptr_t)&(((CallInfo*)0)->u.l.base)), "base");
    // test offsetof(CallInfo, u.l.base)
}

void Compiler::ReloadTop() {
    auto top = LoadField(values_.ci, rt_->GetType("TValue"),
            offsetof(CallInfo, top), "top");
    SetField(values_.state, top, offsetof(lua_State, top), "top");
}

void Compiler::SetTop(int reg) {
    auto top = GetValueR(reg, "top");
    SetField(values_.state, top, offsetof(lua_State, top), "top");
}

llvm::Value* Compiler::TopDiff(int n) {
    auto top = LoadField(values_.state, rt_->GetType("TValue"),
            offsetof(lua_State, top), "top");
    auto r = GetValueR(n, "r");
    auto diff = builder_.CreatePtrDiff(top, r, "diff");
    return builder_.CreateIntCast(diff, MakeIntT(sizeof(int)), false, "idiff");
}

llvm::BasicBlock* Compiler::CreateSubBlock(const std::string& suffix,
            llvm::BasicBlock* preview) {
    if (!preview)
        preview = blocks_[curr_];
    auto name = blocks_[curr_]->getName() + suffix;
    return llvm::BasicBlock::Create(context_, name, function_, preview);
}

}

