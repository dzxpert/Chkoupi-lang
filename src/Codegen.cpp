#include "Codegen.h"
#include "AST.h"
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <stdexcept>

Codegen::Codegen()
    : builder(ctx),
      module(std::make_unique<llvm::Module>("chkoupi_module", ctx))
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();

    declarePrintf();
    declareScanf();
}

// ── Helpers ───────────────────────────────────────────────────────────────────
llvm::Type* Codegen::getLLVMType(const std::string& t) {
    if (t == "int"    || t == "") return llvm::Type::getInt64Ty(ctx);
    if (t == "float")             return llvm::Type::getDoubleTy(ctx);
    if (t == "bool")              return llvm::Type::getInt1Ty(ctx);
    if (t == "char")              return llvm::Type::getInt8Ty(ctx);
    if (t == "string")            return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx));
    if (t == "void")              return llvm::Type::getVoidTy(ctx);
    // Optional: optional:T -> { i1, T }
    if (t.rfind("optional:", 0) == 0) {
        std::string inner = t.substr(9);
        llvm::Type* innerTy = getLLVMType(inner);
        return llvm::StructType::get(ctx, {llvm::Type::getInt1Ty(ctx), innerTy});
    }
    // User-defined struct
    auto it = structTypes.find(t);
    if (it != structTypes.end()) return it->second;
    throw std::runtime_error("Unknown type: " + t);
}

llvm::AllocaInst* Codegen::createEntryAlloca(llvm::Function* fn,
                                               const std::string& name,
                                               llvm::Type* ty) {
    llvm::IRBuilder<> tmp(&fn->getEntryBlock(), fn->getEntryBlock().begin());
    return tmp.CreateAlloca(ty, nullptr, name);
}

void Codegen::declarePrintf() {
    llvm::FunctionType* ft = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(ctx),
        {llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx))},
        true); // varargs
    llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "printf", module.get());
}

void Codegen::declareScanf() {
    llvm::FunctionType* ft = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(ctx),
        {llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx))},
        true);
    llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "scanf", module.get());
}

// ── Top-level ─────────────────────────────────────────────────────────────────
void Codegen::generate(const Program& prog) {
    // Create implicit main() if there is no explicit fun main
    bool hasMain = false;
    for (auto& s : prog.stmts)
        if (auto* f = dynamic_cast<const FuncDecl*>(s.get()))
            if (f->name == "main") { hasMain = true; break; }

    llvm::Function* mainFn = nullptr;
    if (!hasMain) {
        auto* ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), false);
        mainFn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "main", module.get());
        auto* bb = llvm::BasicBlock::Create(ctx, "entry", mainFn);
        builder.SetInsertPoint(bb);
        currentFunction = mainFn;
    }

    for (auto& s : prog.stmts)
        genStmt(*s);

    if (!hasMain) {
        builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0));
    }

    llvm::verifyModule(*module, &llvm::errs());
}

void Codegen::dumpIR() const {
    module->print(llvm::outs(), nullptr);
}

// ── JIT execution ─────────────────────────────────────────────────────────────
void Codegen::runJIT() {
    // MCJIT takes ownership of the module — move it out
    std::string errStr;
    llvm::ExecutionEngine* ee =
        llvm::EngineBuilder(std::move(module))
            .setErrorStr(&errStr)
            .setEngineKind(llvm::EngineKind::JIT)
            .create();

    if (!ee)
        throw std::runtime_error("JIT setup failed: " + errStr);

    ee->finalizeObject();

    // Look up main() and call it
    auto mainFn = (int(*)())ee->getFunctionAddress("main");
    if (!mainFn)
        throw std::runtime_error("Couldn't find 'main' function to execute");

    int exitCode = mainFn();
    delete ee;
    std::exit(exitCode);
}

void Codegen::writeObjectFile(const std::string& path) const {
    std::string err;
    auto targetTriple = llvm::sys::getDefaultTargetTriple();
    auto* target = llvm::TargetRegistry::lookupTarget(targetTriple, err);
    if (!target) throw std::runtime_error("Target lookup failed: " + err);

    llvm::TargetOptions opt;
    auto* tm = target->createTargetMachine(targetTriple, "generic", "", opt, llvm::Reloc::PIC_);
    module->setDataLayout(tm->createDataLayout());
    module->setTargetTriple(targetTriple);

    std::error_code ec;
    llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);
    if (ec) throw std::runtime_error("Could not open file: " + ec.message());

    llvm::legacy::PassManager pm;
    if (tm->addPassesToEmitFile(pm, dest, nullptr, llvm::CodeGenFileType::ObjectFile))
        throw std::runtime_error("Target cannot emit object file");
    pm.run(*module);
    dest.flush();
}

// ── Statement codegen ─────────────────────────────────────────────────────────
void Codegen::genStmt(const Stmt& stmt) {
    if (auto* s = dynamic_cast<const VarDeclStmt*>(&stmt))    { genVarDecl(*s); return; }
    if (auto* s = dynamic_cast<const PrintStmt*>(&stmt))      { genPrint(*s);   return; }
    if (auto* s = dynamic_cast<const ReadStmt*>(&stmt))       { genRead(*s);    return; }
    if (auto* s = dynamic_cast<const IfStmt*>(&stmt))         { genIf(*s);      return; }
    if (auto* s = dynamic_cast<const WhileStmt*>(&stmt))      { genWhile(*s);   return; }
    if (auto* s = dynamic_cast<const ForStmt*>(&stmt))        { genFor(*s);     return; }
    if (auto* s = dynamic_cast<const SwitchStmt*>(&stmt))     { genSwitch(*s);  return; }
    if (auto* s = dynamic_cast<const TryCatchStmt*>(&stmt))   { genTryCatch(*s); return; }
    if (dynamic_cast<const ImportStmt*>(&stmt))               { /* resolved at link time */ return; }
    if (auto* s = dynamic_cast<const ReturnStmt*>(&stmt))     { genReturn(*s);  return; }
    if (auto* s = dynamic_cast<const FuncDecl*>(&stmt))       { genFunc(*s);    return; }
    if (auto* s = dynamic_cast<const StructDecl*>(&stmt))     { genStructDecl(*s); return; }
    if (auto* s = dynamic_cast<const EnumDecl*>(&stmt))       { genEnumDecl(*s); return; }
    if (auto* s = dynamic_cast<const ExprStmt*>(&stmt))       { genExpr(*s->expr); return; }
    throw std::runtime_error("Unknown statement type");
}

void Codegen::genBlock(const std::vector<StmtPtr>& block) {
    for (auto& s : block) genStmt(*s);
}

void Codegen::genVarDecl(const VarDeclStmt& s) {
    // ── Array literal init: dir arr = [1, 2, 3]; ─────────────────────────────
    if (auto* arrLit = dynamic_cast<const ArrayLitExpr*>(s.init.get())) {
        if (arrLit->elements.empty())
            throw std::runtime_error("Empty array literal");
        llvm::Value* firstVal = genExpr(*arrLit->elements[0]);
        llvm::Type*  elemTy   = firstVal->getType();
        size_t count = arrLit->elements.size();
        auto* arrTy  = llvm::ArrayType::get(elemTy, count);
        auto* alloca = createEntryAlloca(currentFunction, s.name, arrTy);
        auto* zero   = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
        for (size_t i = 0; i < count; ++i) {
            auto* idx = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), i);
            auto* gep = builder.CreateGEP(arrTy, alloca, {zero, idx});
            llvm::Value* val = (i == 0) ? firstVal : genExpr(*arrLit->elements[i]);
            builder.CreateStore(val, gep);
        }
        namedValues[s.name] = alloca;
        constFlags[s.name]  = s.isConst;
        varTypes[s.name]    = s.type;
        return;
    }

    // ── Optional init: dir x : ymkn[tabi3i] = walo; ─────────────────────────
    if (s.type.rfind("optional:", 0) == 0) {
        llvm::Type* optTy = getLLVMType(s.type);
        auto* alloca = createEntryAlloca(currentFunction, s.name, optTy);
        if (dynamic_cast<const NullLitExpr*>(s.init.get())) {
            // walo: {false, undef}
            auto* hasValPtr = builder.CreateStructGEP(optTy, alloca, 0);
            builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx), 0), hasValPtr);
        } else {
            // value present: {true, val}
            llvm::Value* initVal = genExpr(*s.init);
            auto* hasValPtr = builder.CreateStructGEP(optTy, alloca, 0);
            builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx), 1), hasValPtr);
            auto* valPtr = builder.CreateStructGEP(optTy, alloca, 1);
            builder.CreateStore(initVal, valPtr);
        }
        namedValues[s.name] = alloca;
        constFlags[s.name]  = s.isConst;
        varTypes[s.name]    = s.type;
        return;
    }

    // ── Struct literal init ──────────────────────────────────────────────────
    if (auto* slit = dynamic_cast<const StructLitExpr*>(s.init.get())) {
        auto stIt = structTypes.find(slit->structName);
        if (stIt == structTypes.end())
            throw std::runtime_error("Unknown struct type: " + slit->structName);
        llvm::StructType* st = stIt->second;
        auto* alloca = createEntryAlloca(currentFunction, s.name, st);
        for (auto& [fieldName, fieldExpr] : slit->fieldInits) {
            int idx = getFieldIndex(slit->structName, fieldName);
            auto* gep = builder.CreateStructGEP(st, alloca, idx);
            builder.CreateStore(genExpr(*fieldExpr), gep);
        }
        namedValues[s.name] = alloca;
        constFlags[s.name]  = s.isConst;
        varTypes[s.name]    = slit->structName;
        return;
    }

    // ── Normal scalar init ───────────────────────────────────────────────────
    llvm::Value* initVal = genExpr(*s.init);
    llvm::Type*  ty      = initVal->getType();
    auto* alloca = createEntryAlloca(currentFunction, s.name, ty);
    builder.CreateStore(initVal, alloca);
    namedValues[s.name] = alloca;
    constFlags[s.name]  = s.isConst;
    varTypes[s.name]    = s.type;
}

// ektb("fmt", args...)  →  printf
void Codegen::genPrint(const PrintStmt& s) {
    llvm::Function* printfFn = module->getFunction("printf");
    std::vector<llvm::Value*> args;
    for (auto& a : s.args) args.push_back(genExpr(*a));
    builder.CreateCall(printfFn, args);
}

// a9ra(x)  →  scanf("%d", &x)
void Codegen::genRead(const ReadStmt& s) {
    llvm::Function* scanfFn = module->getFunction("scanf");
    auto it = namedValues.find(s.varName);
    if (it == namedValues.end())
        throw std::runtime_error("Undefined variable: " + s.varName);

    // Infer format from type
    llvm::Type* ty = it->second->getAllocatedType();
    std::string fmt = ty->isDoubleTy() ? "%lf" : (ty->isIntegerTy(1) ? "%d" : "%lld");
    llvm::Value* fmtStr = builder.CreateGlobalStringPtr(fmt);
    builder.CreateCall(scanfFn, {fmtStr, it->second});
}

// idha (cond) { ... } wla { ... }
void Codegen::genIf(const IfStmt& s) {
    llvm::Value* cond = genExpr(*s.condition);
    if (!cond->getType()->isIntegerTy(1))
        cond = builder.CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0));

    auto* thenBB  = llvm::BasicBlock::Create(ctx, "idha.then",     currentFunction);
    auto* elseBB  = llvm::BasicBlock::Create(ctx, "idha_mknch",    currentFunction);
    auto* mergeBB = llvm::BasicBlock::Create(ctx, "idha.end",      currentFunction);

    builder.CreateCondBr(cond, thenBB, elseBB);

    builder.SetInsertPoint(thenBB);
    genBlock(s.thenBlock);
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(mergeBB);

    builder.SetInsertPoint(elseBB);
    genBlock(s.elseBlock);
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(mergeBB);

    builder.SetInsertPoint(mergeBB);
}

// ki_tkoon (cond) { ... }
void Codegen::genWhile(const WhileStmt& s) {
    auto* condBB = llvm::BasicBlock::Create(ctx, "ab9a_dor.cond", currentFunction);
    auto* bodyBB = llvm::BasicBlock::Create(ctx, "ab9a_dor.body", currentFunction);
    auto* endBB  = llvm::BasicBlock::Create(ctx, "ab9a_dor.end",  currentFunction);

    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);
    llvm::Value* cond = genExpr(*s.condition);
    if (!cond->getType()->isIntegerTy(1))
        cond = builder.CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0));
    builder.CreateCondBr(cond, bodyBB, endBB);

    builder.SetInsertPoint(bodyBB);
    genBlock(s.body);
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(condBB);

    builder.SetInsertPoint(endBB);
}

// madam (init; cond; update) { ... }
void Codegen::genFor(const ForStmt& s) {
    genStmt(*s.init);

    auto* condBB = llvm::BasicBlock::Create(ctx, "madam.cond", currentFunction);
    auto* bodyBB = llvm::BasicBlock::Create(ctx, "madam.body", currentFunction);
    auto* endBB  = llvm::BasicBlock::Create(ctx, "madam.end",  currentFunction);

    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);
    llvm::Value* cond = genExpr(*s.condition);
    if (!cond->getType()->isIntegerTy(1))
        cond = builder.CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0));
    builder.CreateCondBr(cond, bodyBB, endBB);

    builder.SetInsertPoint(bodyBB);
    genBlock(s.body);
    genExpr(*s.update); // update expression (e.g. i = i + 1)
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(condBB);

    builder.SetInsertPoint(endBB);
}

// bdl (expr) { khyr val: ... }  →  LLVM switch instruction
void Codegen::genSwitch(const SwitchStmt& s) {
    llvm::Value* switchVal = genExpr(*s.expr);

    auto* endBB = llvm::BasicBlock::Create(ctx, "bdl.end", currentFunction);
    auto* defaultBB = endBB; // fall-through to end by default

    auto* sw = builder.CreateSwitch(switchVal, defaultBB, (unsigned)s.cases.size());

    for (auto& c : s.cases) {
        llvm::Value* caseVal = genExpr(*c.value);
        auto* constInt = llvm::dyn_cast<llvm::ConstantInt>(caseVal);
        if (!constInt)
            throw std::runtime_error("khyr value must be an integer constant");

        auto* caseBB = llvm::BasicBlock::Create(ctx, "bdl.khyr", currentFunction);
        sw->addCase(constInt, caseBB);

        builder.SetInsertPoint(caseBB);
        genBlock(c.body);
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(endBB);
    }

    builder.SetInsertPoint(endBB);
}

// jarb { ... } ila_ghalt { ... }  →  emits both blocks sequentially
// (full exception support requires personality functions; this is a simplification)
void Codegen::genTryCatch(const TryCatchStmt& s) {
    auto* tryBB   = llvm::BasicBlock::Create(ctx, "jarb.try",    currentFunction);
    auto* catchBB = llvm::BasicBlock::Create(ctx, "ila_ghalt",   currentFunction);
    auto* endBB   = llvm::BasicBlock::Create(ctx, "jarb.end",    currentFunction);

    builder.CreateBr(tryBB);

    builder.SetInsertPoint(tryBB);
    genBlock(s.tryBlock);
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(endBB);

    builder.SetInsertPoint(catchBB);
    genBlock(s.catchBlock);
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(endBB);

    builder.SetInsertPoint(endBB);
}

void Codegen::genReturn(const ReturnStmt& s) {
    if (s.value)
        builder.CreateRet(genExpr(*s.value));
    else
        builder.CreateRetVoid();
}

void Codegen::genFunc(const FuncDecl& s) {
    // ── Save caller context ───────────────────────────────────────────────────
    llvm::Function*  savedFn = currentFunction;
    llvm::BasicBlock* savedBB = builder.GetInsertBlock();
    auto savedNamedValues = namedValues; // snapshot outer scope

    // ── Build inner function ──────────────────────────────────────────────────
    std::vector<llvm::Type*> paramTypes;
    for (auto& p : s.params) paramTypes.push_back(getLLVMType(p.type));
    auto* retTy = getLLVMType(s.returnType);
    auto* ft    = llvm::FunctionType::get(retTy, paramTypes, false);
    auto* fn    = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, s.name, module.get());

    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);
    currentFunction = fn;

    size_t idx = 0;
    for (auto& arg : fn->args()) {
        const auto& p = s.params[idx++];
        arg.setName(p.name);
        auto* alloca = createEntryAlloca(fn, p.name, arg.getType());
        builder.CreateStore(&arg, alloca);
        namedValues[p.name] = alloca;
    }

    genBlock(s.body);

    // Implicit void return
    if (retTy->isVoidTy() && !builder.GetInsertBlock()->getTerminator())
        builder.CreateRetVoid();

    llvm::verifyFunction(*fn, &llvm::errs());

    // ── Restore caller context ────────────────────────────────────────────────
    namedValues     = savedNamedValues;
    currentFunction = savedFn;
    if (savedBB)
        builder.SetInsertPoint(savedBB);
}

// ── Expression codegen ────────────────────────────────────────────────────────
llvm::Value* Codegen::genExpr(const Expr& expr) {
    if (auto* e = dynamic_cast<const IntLitExpr*>(&expr))
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), e->value);
    if (auto* e = dynamic_cast<const FloatLitExpr*>(&expr))
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx), e->value);
    if (auto* e = dynamic_cast<const StringLitExpr*>(&expr))
        return builder.CreateGlobalStringPtr(e->value);
    if (auto* e = dynamic_cast<const BoolLitExpr*>(&expr))
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx), e->value ? 1 : 0);
    if (auto* e = dynamic_cast<const VarExpr*>(&expr)) {
        auto it = namedValues.find(e->name);
        if (it == namedValues.end())
            throw std::runtime_error("Undefined variable: " + e->name);
        return builder.CreateLoad(it->second->getAllocatedType(), it->second, e->name);
    }
    if (auto* e = dynamic_cast<const BinaryExpr*>(&expr))       return genBinary(*e);
    if (auto* e = dynamic_cast<const UnaryExpr*>(&expr))        return genUnary(*e);
    if (auto* e = dynamic_cast<const CallExpr*>(&expr))         return genCall(*e);
    if (auto* e = dynamic_cast<const AssignExpr*>(&expr))       return genAssign(*e);
    if (auto* e = dynamic_cast<const ArrayLitExpr*>(&expr))     return genArrayLit(*e);
    if (auto* e = dynamic_cast<const IndexExpr*>(&expr))        return genIndex(*e);
    if (auto* e = dynamic_cast<const IndexAssignExpr*>(&expr))  return genIndexAssign(*e);
    if (auto* e = dynamic_cast<const StructLitExpr*>(&expr))    return genStructLit(*e);
    if (auto* e = dynamic_cast<const FieldAccessExpr*>(&expr))  return genFieldAccess(*e);
    if (auto* e = dynamic_cast<const FieldAssignExpr*>(&expr))  return genFieldAssign(*e);
    if (dynamic_cast<const NullLitExpr*>(&expr))
        throw std::runtime_error("'walo' can only be used in optional variable declarations");
    throw std::runtime_error("Unknown expression type");
}

llvm::Value* Codegen::genBinary(const BinaryExpr& e) {
    llvm::Value* L = genExpr(*e.lhs);
    llvm::Value* R = genExpr(*e.rhs);
    bool isFloat = L->getType()->isDoubleTy();

    if (e.op == "+")   return isFloat ? builder.CreateFAdd(L, R) : builder.CreateAdd(L, R);
    if (e.op == "-")   return isFloat ? builder.CreateFSub(L, R) : builder.CreateSub(L, R);
    if (e.op == "*")   return isFloat ? builder.CreateFMul(L, R) : builder.CreateMul(L, R);
    if (e.op == "/")   return isFloat ? builder.CreateFDiv(L, R) : builder.CreateSDiv(L, R);
    if (e.op == "%")   return builder.CreateSRem(L, R);
    if (e.op == "==")  return isFloat ? builder.CreateFCmpOEQ(L, R) : builder.CreateICmpEQ(L, R);
    if (e.op == "!=")  return isFloat ? builder.CreateFCmpONE(L, R) : builder.CreateICmpNE(L, R);
    if (e.op == "<")   return isFloat ? builder.CreateFCmpOLT(L, R) : builder.CreateICmpSLT(L, R);
    if (e.op == ">")   return isFloat ? builder.CreateFCmpOGT(L, R) : builder.CreateICmpSGT(L, R);
    if (e.op == "<=")  return isFloat ? builder.CreateFCmpOLE(L, R) : builder.CreateICmpSLE(L, R);
    if (e.op == ">=")  return isFloat ? builder.CreateFCmpOGE(L, R) : builder.CreateICmpSGE(L, R);
    if (e.op == "and") return builder.CreateAnd(L, R);
    if (e.op == "or")  return builder.CreateOr(L, R);
    throw std::runtime_error("Unknown binary operator: " + e.op);
}

llvm::Value* Codegen::genUnary(const UnaryExpr& e) {
    llvm::Value* V = genExpr(*e.operand);
    if (e.op == "machi" || e.op == "not") return builder.CreateNot(V);
    if (e.op == "-") return V->getType()->isDoubleTy() ? builder.CreateFNeg(V) : builder.CreateNeg(V);
    throw std::runtime_error("Unknown unary operator: " + e.op);
}

llvm::Value* Codegen::genCall(const CallExpr& e) {
    llvm::Function* fn = module->getFunction(e.callee);
    if (!fn) throw std::runtime_error("Unknown function: " + e.callee);
    std::vector<llvm::Value*> args;
    for (auto& a : e.args) args.push_back(genExpr(*a));
    return builder.CreateCall(fn, args);
}

llvm::Value* Codegen::genAssign(const AssignExpr& e) {
    auto it = namedValues.find(e.name);
    if (it == namedValues.end())
        throw std::runtime_error("Undefined variable: " + e.name);
    if (constFlags.count(e.name) && constFlags[e.name])
        throw std::runtime_error("Cannot assign to constant: " + e.name);
    llvm::Value* val = genExpr(*e.value);
    builder.CreateStore(val, it->second);
    return val;
}

// ── Struct / Enum declaration codegen ─────────────────────────────────────────

void Codegen::genStructDecl(const StructDecl& s) {
    std::vector<llvm::Type*> fieldTypes;
    std::vector<std::pair<std::string, std::string>> fieldInfo;
    for (auto& f : s.fields) {
        fieldTypes.push_back(getLLVMType(f.type));
        fieldInfo.push_back({f.name, f.type});
    }
    auto* st = llvm::StructType::create(ctx, fieldTypes, s.name);
    structTypes[s.name] = st;
    structFieldInfo[s.name] = std::move(fieldInfo);
}

void Codegen::genEnumDecl(const EnumDecl& s) {
    int idx = 0;
    for (auto& v : s.variants) {
        enumVariants[s.name][v] = idx++;
    }
}

// ── Array codegen ─────────────────────────────────────────────────────────────

llvm::Value* Codegen::genArrayLit(const ArrayLitExpr& e) {
    // Standalone array literal (outside var decl): alloca + populate
    if (e.elements.empty())
        throw std::runtime_error("Empty array literal");
    llvm::Value* firstVal = genExpr(*e.elements[0]);
    llvm::Type*  elemTy   = firstVal->getType();
    size_t count = e.elements.size();
    auto* arrTy  = llvm::ArrayType::get(elemTy, count);
    auto* alloca = createEntryAlloca(currentFunction, "jadwal_tmp", arrTy);
    auto* zero   = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
    for (size_t i = 0; i < count; ++i) {
        auto* idx = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), i);
        auto* gep = builder.CreateGEP(arrTy, alloca, {zero, idx});
        llvm::Value* val = (i == 0) ? firstVal : genExpr(*e.elements[i]);
        builder.CreateStore(val, gep);
    }
    return builder.CreateLoad(arrTy, alloca);
}

llvm::Value* Codegen::genIndex(const IndexExpr& e) {
    auto* varExpr = dynamic_cast<const VarExpr*>(e.object.get());
    if (!varExpr) throw std::runtime_error("Can only index variables");
    auto it = namedValues.find(varExpr->name);
    if (it == namedValues.end())
        throw std::runtime_error("Undefined variable: " + varExpr->name);
    auto* alloca = it->second;
    auto* arrTy  = alloca->getAllocatedType();
    if (!arrTy->isArrayTy())
        throw std::runtime_error("Variable '" + varExpr->name + "' is not an array");
    llvm::Value* idx  = genExpr(*e.index);
    auto* zero   = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
    auto* gep    = builder.CreateGEP(arrTy, alloca, {zero, idx});
    auto* elemTy = arrTy->getArrayElementType();
    return builder.CreateLoad(elemTy, gep);
}

llvm::Value* Codegen::genIndexAssign(const IndexAssignExpr& e) {
    auto* varExpr = dynamic_cast<const VarExpr*>(e.object.get());
    if (!varExpr) throw std::runtime_error("Can only index-assign variables");
    auto it = namedValues.find(varExpr->name);
    if (it == namedValues.end())
        throw std::runtime_error("Undefined variable: " + varExpr->name);
    if (constFlags.count(varExpr->name) && constFlags[varExpr->name])
        throw std::runtime_error("Cannot assign to constant: " + varExpr->name);
    auto* alloca = it->second;
    auto* arrTy  = alloca->getAllocatedType();
    if (!arrTy->isArrayTy())
        throw std::runtime_error("Variable '" + varExpr->name + "' is not an array");
    llvm::Value* idx = genExpr(*e.index);
    llvm::Value* val = genExpr(*e.value);
    auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
    auto* gep  = builder.CreateGEP(arrTy, alloca, {zero, idx});
    builder.CreateStore(val, gep);
    return val;
}

// ── Struct codegen ────────────────────────────────────────────────────────────

llvm::Value* Codegen::genStructLit(const StructLitExpr& e) {
    auto stIt = structTypes.find(e.structName);
    if (stIt == structTypes.end())
        throw std::runtime_error("Unknown struct type: " + e.structName);
    llvm::StructType* st = stIt->second;
    auto* alloca = createEntryAlloca(currentFunction, "9aleb_tmp", st);
    for (auto& [fieldName, fieldExpr] : e.fieldInits) {
        int idx = getFieldIndex(e.structName, fieldName);
        auto* gep = builder.CreateStructGEP(st, alloca, idx);
        builder.CreateStore(genExpr(*fieldExpr), gep);
    }
    return builder.CreateLoad(st, alloca);
}

llvm::Value* Codegen::genFieldAccess(const FieldAccessExpr& e) {
    // 1. Enum variant access: EnumName.variant → integer constant
    if (auto* var = dynamic_cast<const VarExpr*>(e.object.get())) {
        auto enumIt = enumVariants.find(var->name);
        if (enumIt != enumVariants.end()) {
            auto variantIt = enumIt->second.find(e.field);
            if (variantIt == enumIt->second.end())
                throw std::runtime_error("Unknown enum variant: " + var->name + "." + e.field);
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), variantIt->second);
        }
    }

    // 2. Struct field access: obj.field
    auto* var = dynamic_cast<const VarExpr*>(e.object.get());
    if (!var) throw std::runtime_error("Field access requires a variable");
    auto it = namedValues.find(var->name);
    if (it == namedValues.end())
        throw std::runtime_error("Undefined variable: " + var->name);
    std::string sName = getStructNameForVar(var->name);
    auto* st = structTypes[sName];
    int idx = getFieldIndex(sName, e.field);
    auto* gep = builder.CreateStructGEP(st, it->second, idx);
    auto* fieldTy = st->getElementType(idx);
    return builder.CreateLoad(fieldTy, gep);
}

llvm::Value* Codegen::genFieldAssign(const FieldAssignExpr& e) {
    auto* var = dynamic_cast<const VarExpr*>(e.object.get());
    if (!var) throw std::runtime_error("Field assign requires a variable");
    auto it = namedValues.find(var->name);
    if (it == namedValues.end())
        throw std::runtime_error("Undefined variable: " + var->name);
    if (constFlags.count(var->name) && constFlags[var->name])
        throw std::runtime_error("Cannot assign to constant: " + var->name);
    std::string sName = getStructNameForVar(var->name);
    auto* st = structTypes[sName];
    int idx = getFieldIndex(sName, e.field);
    auto* gep = builder.CreateStructGEP(st, it->second, idx);
    llvm::Value* val = genExpr(*e.value);
    builder.CreateStore(val, gep);
    return val;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

int Codegen::getFieldIndex(const std::string& structName, const std::string& fieldName) {
    auto it = structFieldInfo.find(structName);
    if (it == structFieldInfo.end())
        throw std::runtime_error("Unknown struct: " + structName);
    for (int i = 0; i < (int)it->second.size(); ++i) {
        if (it->second[i].first == fieldName) return i;
    }
    throw std::runtime_error("Unknown field '" + fieldName + "' in struct '" + structName + "'");
}

std::string Codegen::getStructNameForVar(const std::string& varName) {
    auto it = varTypes.find(varName);
    if (it == varTypes.end() || it->second.empty())
        throw std::runtime_error("Variable '" + varName + "' has no known struct type");
    // Check if it's a known struct
    if (structTypes.count(it->second)) return it->second;
    throw std::runtime_error("Variable '" + varName + "' is not a struct (type: " + it->second + ")");
}
