#include "Codegen.h"
#include "AST.h"
#include <llvm/Config/llvm-config.h>
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
    declareMalloc();
    declareFree();
    declareStrcmp();
    declareMemcpy();

    // Declare standard library math functions
    llvm::FunctionType* sqrtFt = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(ctx), {llvm::Type::getDoubleTy(ctx)}, false);
    llvm::Function::Create(sqrtFt, llvm::Function::ExternalLinkage, "sqrt", module.get());

    llvm::FunctionType* powFt = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(ctx), {llvm::Type::getDoubleTy(ctx), llvm::Type::getDoubleTy(ctx)}, false);
    llvm::Function::Create(powFt, llvm::Function::ExternalLinkage, "pow", module.get());
}

// ── Helpers ───────────────────────────────────────────────────────────────────
llvm::Type* Codegen::getLLVMType(const std::string& t) {
    if (t.rfind("jadwl<", 0) == 0) {
        std::string elem = t.substr(6, t.size() - 7);
        llvm::Type* et = getLLVMType(elem);
        return llvm::PointerType::getUnqual(et);
    }
    if (t == "int"    || t == "") return llvm::Type::getInt64Ty(ctx);
    if (t == "float")             return llvm::Type::getDoubleTy(ctx);
    if (t == "bool")              return llvm::Type::getInt1Ty(ctx);
    if (t == "char")              return llvm::Type::getInt8Ty(ctx);
    if (t == "string")            return llvm::StructType::get(ctx, {
                                      llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx)),
                                      llvm::Type::getInt64Ty(ctx)
                                  });
    if (t == "void")              return llvm::Type::getVoidTy(ctx);
    auto it = structTypes.find(t);
    if (it != structTypes.end()) {
        return llvm::PointerType::getUnqual(it->second);
    }
    throw std::runtime_error("Unknown type: " + t);
}

void Codegen::declareMalloc() {
    llvm::FunctionType* ft = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx)),
        {llvm::Type::getInt64Ty(ctx)},
        false);
    llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "malloc", module.get());
}

void Codegen::declareFree() {
    llvm::FunctionType* ft = llvm::FunctionType::get(
        llvm::Type::getVoidTy(ctx),
        {llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx))},
        false);
    llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "free", module.get());
}

std::string Codegen::inferType(const Expr& expr) {
    if (dynamic_cast<const IntLitExpr*>(&expr))    return "int";
    if (dynamic_cast<const FloatLitExpr*>(&expr))  return "float";
    if (dynamic_cast<const BoolLitExpr*>(&expr))   return "bool";
    if (dynamic_cast<const CharLitExpr*>(&expr))   return "char";
    if (dynamic_cast<const StringLitExpr*>(&expr)) return "string";
    if (auto* e = dynamic_cast<const VarExpr*>(&expr)) {
        auto it = variableTypes.find(e->name);
        if (it != variableTypes.end()) return it->second;
        return "int";
    }
    if (auto* e = dynamic_cast<const CallExpr*>(&expr)) {
        auto it = functionReturnTypes.find(e->callee);
        if (it != functionReturnTypes.end()) return it->second;
        return "int";
    }
    if (auto* e = dynamic_cast<const IndexExpr*>(&expr)) {
        std::string targetType = inferType(*e->target);
        if (targetType.rfind("jadwl<", 0) == 0) {
            return targetType.substr(6, targetType.size() - 7);
        }
        return "int";
    }
    if (auto* e = dynamic_cast<const ArrayLitExpr*>(&expr)) {
        if (e->elements.empty()) return "jadwl<int>";
        return "jadwl<" + inferType(*e->elements[0]) + ">";
    }
    if (auto* e = dynamic_cast<const BinaryExpr*>(&expr)) {
        return inferType(*e->lhs);
    }
    if (auto* e = dynamic_cast<const UnaryExpr*>(&expr)) {
        return inferType(*e->operand);
    }
    if (auto* e = dynamic_cast<const StructLitExpr*>(&expr)) {
        return e->structName;
    }
    if (auto* e = dynamic_cast<const MemberExpr*>(&expr)) {
        std::string targetType = inferType(*e->target);
        auto it = structFieldTypes.find(targetType);
        if (it != structFieldTypes.end()) {
            auto fIt = it->second.find(e->fieldName);
            if (fIt != it->second.end()) return fIt->second;
        }
        return "int";
    }
    if (auto* e = dynamic_cast<const MethodCallExpr*>(&expr)) {
        std::string targetType = inferType(*e->target);
        std::string mangledName = targetType + "." + e->methodName;
        auto it = functionReturnTypes.find(mangledName);
        if (it != functionReturnTypes.end()) return it->second;
        return "int";
    }
    return "int";
}

const ReturnStmt* Codegen::findReturnStmt(const std::vector<StmtPtr>& stmts) {
    for (auto& s : stmts) {
        if (auto* r = dynamic_cast<const ReturnStmt*>(s.get())) {
            return r;
        }
        if (auto* ifs = dynamic_cast<const IfStmt*>(s.get())) {
            if (auto* r = findReturnStmt(ifs->thenBlock)) return r;
            if (auto* r = findReturnStmt(ifs->elseBlock)) return r;
        }
        if (auto* wh = dynamic_cast<const WhileStmt*>(s.get())) {
            if (auto* r = findReturnStmt(wh->body)) return r;
        }
        if (auto* fo = dynamic_cast<const ForStmt*>(s.get())) {
            if (auto* r = findReturnStmt(fo->body)) return r;
        }
        if (auto* sw = dynamic_cast<const SwitchStmt*>(s.get())) {
            for (auto& c : sw->cases) {
                if (auto* r = findReturnStmt(c.body)) return r;
            }
        }
        if (auto* tc = dynamic_cast<const TryCatchStmt*>(s.get())) {
            if (auto* r = findReturnStmt(tc->tryBlock)) return r;
            if (auto* r = findReturnStmt(tc->catchBlock)) return r;
        }
    }
    return nullptr;
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
    // Register standard library function types
    functionReturnTypes["sqrt"] = "float";
    functionReturnTypes["pow"]  = "float";

    // Pre-pass 1: Create opaque named StructTypes
    for (auto& s : prog.stmts) {
        if (auto* sd = dynamic_cast<const StructDeclStmt*>(s.get())) {
            llvm::StructType* structTy = llvm::StructType::create(ctx, sd->name);
            structTypes[sd->name] = structTy;

            std::vector<std::string> orderedFields;
            std::map<std::string, std::string> fieldTys;
            std::map<std::string, const Expr*> defaultVals;
            for (const auto& field : sd->fields) {
                orderedFields.push_back(field.name);
                fieldTys[field.name] = field.type;
                if (field.defaultVal) {
                    defaultVals[field.name] = field.defaultVal.get();
                }
            }
            structFields[sd->name] = orderedFields;
            structFieldTypes[sd->name] = fieldTys;
            structDefaultVals[sd->name] = defaultVals;
        }
    }

    // Pre-pass 2: Populate struct bodies (field types)
    for (auto& s : prog.stmts) {
        if (auto* sd = dynamic_cast<const StructDeclStmt*>(s.get())) {
            llvm::StructType* structTy = structTypes[sd->name];
            std::vector<llvm::Type*> fieldTypes;
            for (const auto& field : sd->fields) {
                fieldTypes.push_back(getLLVMType(field.type));
            }
            structTy->setBody(fieldTypes);
        }
    }

    // Pre-pass 3: register function and method return types
    for (auto& s : prog.stmts) {
        if (auto* f = dynamic_cast<const FuncDecl*>(s.get())) {
            std::string retType = f->returnType;
            if (retType.empty()) {
                const ReturnStmt* r = findReturnStmt(f->body);
                retType = (r && r->value) ? inferType(*r->value) : "void";
            }
            functionReturnTypes[f->name] = retType;
        } else if (auto* sd = dynamic_cast<const StructDeclStmt*>(s.get())) {
            for (auto& mStmt : sd->methods) {
                if (auto* f = dynamic_cast<const FuncDecl*>(mStmt.get())) {
                    std::string retType = f->returnType;
                    if (retType.empty()) {
                        const ReturnStmt* r = findReturnStmt(f->body);
                        retType = (r && r->value) ? inferType(*r->value) : "void";
                    }
                    std::string mangledName = sd->name + "." + f->name;
                    functionReturnTypes[mangledName] = retType;
                }
            }
        }
    }

    // Pre-pass 4: Compile struct methods
    for (auto& s : prog.stmts) {
        if (auto* sd = dynamic_cast<const StructDeclStmt*>(s.get())) {
            for (auto& mStmt : sd->methods) {
                if (auto* fd = dynamic_cast<const FuncDecl*>(mStmt.get())) {
                    genMethod(sd->name, *fd);
                }
            }
        }
    }

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
#if LLVM_VERSION_MAJOR >= 18
    if (tm->addPassesToEmitFile(pm, dest, nullptr, llvm::CodeGenFileType::ObjectFile))
#else
    if (tm->addPassesToEmitFile(pm, dest, nullptr, llvm::CGFT_ObjectFile))
#endif
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
    if (dynamic_cast<const BreakStmt*>(&stmt))               { genBreak();     return; }
    if (dynamic_cast<const ContinueStmt*>(&stmt))            { genContinue();  return; }
    if (auto* s = dynamic_cast<const FuncDecl*>(&stmt))       { genFunc(*s);    return; }
    if (auto* s = dynamic_cast<const StructDeclStmt*>(&stmt)) { genStructDecl(*s); return; }
    if (auto* s = dynamic_cast<const FreeStmt*>(&stmt))       { genFree(*s);    return; }
    if (auto* s = dynamic_cast<const ExprStmt*>(&stmt))       { genExpr(*s->expr); return; }
    throw std::runtime_error("Unknown statement type");
}

void Codegen::genBlock(const std::vector<StmtPtr>& block) {
    auto savedNamedValues = namedValues;
    auto savedConstFlags  = constFlags;
    for (auto& s : block) genStmt(*s);
    namedValues = savedNamedValues;
    constFlags  = savedConstFlags;
}

void Codegen::genVarDecl(const VarDeclStmt& s) {
    llvm::Value* initVal = genExpr(*s.init);
    llvm::Type*  ty;
    if (!s.type.empty()) {
        ty = getLLVMType(s.type);
        if (ty->isDoubleTy() && initVal->getType()->isIntegerTy()) {
            initVal = builder.CreateSIToFP(initVal, ty);
        } else if (ty->isIntegerTy() && initVal->getType()->isDoubleTy()) {
            initVal = builder.CreateFPToSI(initVal, ty);
        }
    } else {
        ty = initVal->getType();
    }
    auto* alloca = createEntryAlloca(currentFunction, s.name, ty);
    builder.CreateStore(initVal, alloca);
    namedValues[s.name] = alloca;
    constFlags[s.name]  = s.isConst;

    std::string typeStr = s.type;
    if (typeStr.empty()) {
        typeStr = inferType(*s.init);
    }
    variableTypes[s.name] = typeStr;
}

// ektb("fmt", args...)  →  printf
void Codegen::genPrint(const PrintStmt& s) {
    llvm::Function* printfFn = module->getFunction("printf");
    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < s.args.size(); ++i) {
        llvm::Value* val = genExpr(*s.args[i]);
        std::string argType = inferType(*s.args[i]);
        if (argType == "string") {
            val = builder.CreateExtractValue(val, 0, "print.strptr");
        } else if (i > 0) { // Do not promote the format string itself
            llvm::Type* ty = val->getType();
            if (ty->isIntegerTy(1) || ty->isIntegerTy(8)) {
                val = builder.CreateZExt(val, llvm::Type::getInt32Ty(ctx));
            }
        }
        args.push_back(val);
    }
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

// ab9a_dor (cond) { ... }
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

    loopStack.push_back({endBB, condBB});
    builder.SetInsertPoint(bodyBB);
    genBlock(s.body);
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(condBB);
    loopStack.pop_back();

    builder.SetInsertPoint(endBB);
}

// dor (init; cond; update) { ... }
void Codegen::genFor(const ForStmt& s) {
    genStmt(*s.init);

    auto* condBB   = llvm::BasicBlock::Create(ctx, "dor.cond",   currentFunction);
    auto* bodyBB   = llvm::BasicBlock::Create(ctx, "dor.body",   currentFunction);
    auto* updateBB = llvm::BasicBlock::Create(ctx, "dor.update", currentFunction);
    auto* endBB    = llvm::BasicBlock::Create(ctx, "dor.end",    currentFunction);

    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);
    llvm::Value* cond = genExpr(*s.condition);
    if (!cond->getType()->isIntegerTy(1))
        cond = builder.CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0));
    builder.CreateCondBr(cond, bodyBB, endBB);

    loopStack.push_back({endBB, updateBB});
    builder.SetInsertPoint(bodyBB);
    genBlock(s.body);
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(updateBB);
    loopStack.pop_back();

    builder.SetInsertPoint(updateBB);
    genExpr(*s.update);
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

// jarb { ... } ila_ghalt { ... }
// Real exception handling requires LLVM invoke/landingpad + personality function.
// For now, emit the try block inline and ignore the catch block.
void Codegen::genTryCatch(const TryCatchStmt& s) {
    llvm::errs() << "[chkoupi warning] jarb/ila_ghalt: exception handling not yet "
                    "implemented, catch block will be ignored\n";
    genBlock(s.tryBlock);
}

void Codegen::genReturn(const ReturnStmt& s) {
    if (s.value)
        builder.CreateRet(genExpr(*s.value));
    else
        builder.CreateRetVoid();
}

void Codegen::genBreak() {
    if (loopStack.empty())
        throw std::runtime_error("a7bss used outside of a loop");
    builder.CreateBr(loopStack.back().breakBB);
    // Create unreachable block for any code after break
    auto* deadBB = llvm::BasicBlock::Create(ctx, "a7bss.after", currentFunction);
    builder.SetInsertPoint(deadBB);
}

void Codegen::genContinue() {
    if (loopStack.empty())
        throw std::runtime_error("kml used outside of a loop");
    builder.CreateBr(loopStack.back().continueBB);
    auto* deadBB = llvm::BasicBlock::Create(ctx, "kml.after", currentFunction);
    builder.SetInsertPoint(deadBB);
}

void Codegen::genFunc(const FuncDecl& s) {
    // ── Save caller context ───────────────────────────────────────────────────
    llvm::Function*  savedFn = currentFunction;
    llvm::BasicBlock* savedBB = builder.GetInsertBlock();
    auto savedNamedValues = namedValues; // snapshot outer scope
    auto savedVariableTypes = variableTypes;

    // ── Build inner function ──────────────────────────────────────────────────
    std::vector<llvm::Type*> paramTypes;
    for (auto& p : s.params) paramTypes.push_back(getLLVMType(p.type));
    std::string retTypeStr = functionReturnTypes[s.name];
    auto* retTy = getLLVMType(retTypeStr);
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
        variableTypes[p.name] = p.type;
    }

    genBlock(s.body);

    // Implicit void return
    if (retTy->isVoidTy() && !builder.GetInsertBlock()->getTerminator())
        builder.CreateRetVoid();

    llvm::verifyFunction(*fn, &llvm::errs());

    // ── Restore caller context ────────────────────────────────────────────────
    namedValues     = savedNamedValues;
    variableTypes   = savedVariableTypes;
    currentFunction = savedFn;
    if (savedBB)
        builder.SetInsertPoint(savedBB);
}

void Codegen::genStructDecl(const StructDeclStmt& s) {
    // Handled in pre-passes
}

void Codegen::genFree(const FreeStmt& s) {
    llvm::Value* val = genExpr(*s.value);
    std::string type = inferType(*s.value);

    llvm::Function* freeFn = module->getFunction("free");
    if (!freeFn) throw std::runtime_error("free is not declared");

    if (type == "string") {
        llvm::Value* ptr = builder.CreateExtractValue(val, 0, "free.strptr");
        builder.CreateCall(freeFn, {ptr});
    } else if (type.rfind("jadwl<", 0) == 0) {
        llvm::Value* offset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), -8);
        llvm::Value* rawPtr = builder.CreateGEP(llvm::Type::getInt8Ty(ctx), val, offset);
        builder.CreateCall(freeFn, {rawPtr});
    } else {
        std::string destructorName = type + ".kssr";
        llvm::Function* destructorFn = module->getFunction(destructorName);
        if (destructorFn) {
            builder.CreateCall(destructorFn, {val});
        }
        llvm::Value* rawPtr = builder.CreateBitCast(val, llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx)));
        builder.CreateCall(freeFn, {rawPtr});
    }
}

void Codegen::genMethod(const std::string& structName, const FuncDecl& fd) {
    llvm::Function*  savedFn = currentFunction;
    llvm::BasicBlock* savedBB = builder.GetInsertBlock();
    auto savedNamedValues = namedValues;
    auto savedVariableTypes = variableTypes;

    std::vector<llvm::Type*> paramTypes;
    paramTypes.push_back(getLLVMType(structName));
    for (auto& p : fd.params) paramTypes.push_back(getLLVMType(p.type));

    std::string mangledName = structName + "." + fd.name;
    std::string retTypeStr = functionReturnTypes[mangledName];
    auto* retTy = getLLVMType(retTypeStr);
    auto* ft    = llvm::FunctionType::get(retTy, paramTypes, false);
    auto* fn    = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, mangledName, module.get());

    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(bb);
    currentFunction = fn;

    auto argIt = fn->arg_begin();
    llvm::Argument& hadArg = *argIt++;
    hadArg.setName("had");
    auto* hadAlloca = createEntryAlloca(fn, "had", hadArg.getType());
    builder.CreateStore(&hadArg, hadAlloca);
    namedValues["had"] = hadAlloca;
    variableTypes["had"] = structName;

    size_t idx = 0;
    while (argIt != fn->arg_end()) {
        llvm::Argument& arg = *argIt++;
        const auto& p = fd.params[idx++];
        arg.setName(p.name);
        auto* alloca = createEntryAlloca(fn, p.name, arg.getType());
        builder.CreateStore(&arg, alloca);
        namedValues[p.name] = alloca;
        variableTypes[p.name] = p.type;
    }

    genBlock(fd.body);

    if (retTy->isVoidTy() && !builder.GetInsertBlock()->getTerminator())
        builder.CreateRetVoid();

    llvm::verifyFunction(*fn, &llvm::errs());

    namedValues     = savedNamedValues;
    variableTypes   = savedVariableTypes;
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
    if (auto* e = dynamic_cast<const StringLitExpr*>(&expr)) {
        llvm::Value* strVal = builder.CreateGlobalStringPtr(e->value);
        llvm::Value* lenVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), e->value.size());
        llvm::Type* stringStructTy = getLLVMType("string");
        llvm::Value* emptyStruct = llvm::UndefValue::get(stringStructTy);
        llvm::Value* structWithPtr = builder.CreateInsertValue(emptyStruct, strVal, 0);
        llvm::Value* finalStruct = builder.CreateInsertValue(structWithPtr, lenVal, 1);
        return finalStruct;
    }
    if (auto* e = dynamic_cast<const BoolLitExpr*>(&expr))
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx), e->value ? 1 : 0);
    if (auto* e = dynamic_cast<const CharLitExpr*>(&expr))
        return llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx), (uint8_t)e->value);
    if (auto* e = dynamic_cast<const VarExpr*>(&expr)) {
        auto it = namedValues.find(e->name);
        if (it == namedValues.end())
            throw std::runtime_error("Undefined variable: " + e->name);
        return builder.CreateLoad(it->second->getAllocatedType(), it->second, e->name);
    }
    if (auto* e = dynamic_cast<const BinaryExpr*>(&expr))   return genBinary(*e);
    if (auto* e = dynamic_cast<const UnaryExpr*>(&expr))    return genUnary(*e);
    if (auto* e = dynamic_cast<const CallExpr*>(&expr))     return genCall(*e);
    if (auto* e = dynamic_cast<const AssignExpr*>(&expr))   return genAssign(*e);
    if (auto* e = dynamic_cast<const ArrayLitExpr*>(&expr))  return genArrayLit(*e);
    if (auto* e = dynamic_cast<const IndexExpr*>(&expr))     return genIndex(*e);
    if (auto* e = dynamic_cast<const IndexAssignExpr*>(&expr)) return genIndexAssign(*e);
    if (auto* e = dynamic_cast<const StructLitExpr*>(&expr))  return genStructLit(*e);
    if (auto* e = dynamic_cast<const MemberExpr*>(&expr))     return genMemberExpr(*e);
    if (auto* e = dynamic_cast<const MemberAssignExpr*>(&expr)) return genMemberAssign(*e);
    if (auto* e = dynamic_cast<const MethodCallExpr*>(&expr)) return genMethodCall(*e);
    throw std::runtime_error("Unknown expression type");
}

llvm::Value* Codegen::genBinary(const BinaryExpr& e) {
    // Short-circuit: don't evaluate RHS eagerly
    if (e.op == "and") return genLogicalAnd(e);
    if (e.op == "or")  return genLogicalOr(e);

    llvm::Value* L = genExpr(*e.lhs);
    llvm::Value* R = genExpr(*e.rhs);

    std::string lhsType = inferType(*e.lhs);
    std::string rhsType = inferType(*e.rhs);

    if (lhsType == "string" && rhsType == "string") {
        if (e.op == "+") {
            llvm::Value* lPtr = builder.CreateExtractValue(L, 0);
            llvm::Value* lLen = builder.CreateExtractValue(L, 1);
            llvm::Value* rPtr = builder.CreateExtractValue(R, 0);
            llvm::Value* rLen = builder.CreateExtractValue(R, 1);

            llvm::Value* newLen = builder.CreateAdd(lLen, rLen, "strcat.len");
            llvm::Value* one64 = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 1);
            llvm::Value* mallocSize = builder.CreateAdd(newLen, one64);

            llvm::Function* mallocFn = module->getFunction("malloc");
            llvm::Value* destPtr = builder.CreateCall(mallocFn, {mallocSize});

            llvm::Function* memcpyFn = module->getFunction("memcpy");
            builder.CreateCall(memcpyFn, {destPtr, lPtr, lLen});

            llvm::Value* destPtrOffset = builder.CreateGEP(llvm::Type::getInt8Ty(ctx), destPtr, lLen);
            builder.CreateCall(memcpyFn, {destPtrOffset, rPtr, rLen});

            llvm::Value* destPtrNull = builder.CreateGEP(llvm::Type::getInt8Ty(ctx), destPtr, newLen);
            builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx), 0), destPtrNull);

            llvm::Type* stringStructTy = getLLVMType("string");
            llvm::Value* emptyStruct = llvm::UndefValue::get(stringStructTy);
            llvm::Value* structWithPtr = builder.CreateInsertValue(emptyStruct, destPtr, 0);
            llvm::Value* finalStruct = builder.CreateInsertValue(structWithPtr, newLen, 1);
            return finalStruct;
        }

        llvm::Value* lPtr = builder.CreateExtractValue(L, 0);
        llvm::Value* rPtr = builder.CreateExtractValue(R, 0);
        llvm::Function* strcmpFn = module->getFunction("strcmp");
        llvm::Value* cmpResult = builder.CreateCall(strcmpFn, {lPtr, rPtr});
        llvm::Value* zero32 = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0);
        if (e.op == "==") return builder.CreateICmpEQ(cmpResult, zero32);
        if (e.op == "!=") return builder.CreateICmpNE(cmpResult, zero32);
        if (e.op == "<")  return builder.CreateICmpSLT(cmpResult, zero32);
        if (e.op == ">")  return builder.CreateICmpSGT(cmpResult, zero32);
        if (e.op == "<=") return builder.CreateICmpSLE(cmpResult, zero32);
        if (e.op == ">=") return builder.CreateICmpSGE(cmpResult, zero32);
        throw std::runtime_error("Unsupported binary operator for string: " + e.op);
    }

    // Promote mixed float/int operands to float (except modulo)
    if (e.op != "%") {
        if (L->getType()->isDoubleTy() && R->getType()->isIntegerTy()) {
            R = builder.CreateSIToFP(R, llvm::Type::getDoubleTy(ctx));
        } else if (L->getType()->isIntegerTy() && R->getType()->isDoubleTy()) {
            L = builder.CreateSIToFP(L, llvm::Type::getDoubleTy(ctx));
        }
    }

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
    throw std::runtime_error("Unknown binary operator: " + e.op);
}

// w (and): short-circuit — if LHS is false, skip RHS
llvm::Value* Codegen::genLogicalAnd(const BinaryExpr& e) {
    llvm::Value* L = genExpr(*e.lhs);
    if (!L->getType()->isIntegerTy(1))
        L = builder.CreateICmpNE(L, llvm::ConstantInt::get(L->getType(), 0));

    auto* lhsBB  = builder.GetInsertBlock();
    auto* rhsBB  = llvm::BasicBlock::Create(ctx, "w.rhs",  currentFunction);
    auto* endBB  = llvm::BasicBlock::Create(ctx, "w.end",  currentFunction);

    builder.CreateCondBr(L, rhsBB, endBB);

    builder.SetInsertPoint(rhsBB);
    llvm::Value* R = genExpr(*e.rhs);
    if (!R->getType()->isIntegerTy(1))
        R = builder.CreateICmpNE(R, llvm::ConstantInt::get(R->getType(), 0));
    auto* rhsEndBB = builder.GetInsertBlock();
    builder.CreateBr(endBB);

    builder.SetInsertPoint(endBB);
    auto* phi = builder.CreatePHI(llvm::Type::getInt1Ty(ctx), 2, "w.result");
    phi->addIncoming(llvm::ConstantInt::getFalse(ctx), lhsBB);
    phi->addIncoming(R, rhsEndBB);
    return phi;
}

// wla (or): short-circuit — if LHS is true, skip RHS
llvm::Value* Codegen::genLogicalOr(const BinaryExpr& e) {
    llvm::Value* L = genExpr(*e.lhs);
    if (!L->getType()->isIntegerTy(1))
        L = builder.CreateICmpNE(L, llvm::ConstantInt::get(L->getType(), 0));

    auto* lhsBB  = builder.GetInsertBlock();
    auto* rhsBB  = llvm::BasicBlock::Create(ctx, "wla.rhs", currentFunction);
    auto* endBB  = llvm::BasicBlock::Create(ctx, "wla.end", currentFunction);

    builder.CreateCondBr(L, endBB, rhsBB);

    builder.SetInsertPoint(rhsBB);
    llvm::Value* R = genExpr(*e.rhs);
    if (!R->getType()->isIntegerTy(1))
        R = builder.CreateICmpNE(R, llvm::ConstantInt::get(R->getType(), 0));
    auto* rhsEndBB = builder.GetInsertBlock();
    builder.CreateBr(endBB);

    builder.SetInsertPoint(endBB);
    auto* phi = builder.CreatePHI(llvm::Type::getInt1Ty(ctx), 2, "wla.result");
    phi->addIncoming(llvm::ConstantInt::getTrue(ctx), lhsBB);
    phi->addIncoming(R, rhsEndBB);
    return phi;
}

llvm::Value* Codegen::genUnary(const UnaryExpr& e) {
    llvm::Value* V = genExpr(*e.operand);
    if (e.op == "machi" || e.op == "not") return builder.CreateNot(V);
    if (e.op == "-") return V->getType()->isDoubleTy() ? builder.CreateFNeg(V) : builder.CreateNeg(V);
    throw std::runtime_error("Unknown unary operator: " + e.op);
}

llvm::Value* Codegen::genCall(const CallExpr& e) {
    if (e.callee == "tool") {
        if (e.args.size() != 1)
            throw std::runtime_error("Built-in function 'tool' expects exactly 1 argument");
        llvm::Value* arr = genExpr(*e.args[0]);
        std::string targetType = inferType(*e.args[0]);
        if (targetType == "string") {
            return builder.CreateExtractValue(arr, 1, "string.len");
        }
        if (targetType.rfind("jadwl<", 0) != 0) {
            throw std::runtime_error("Built-in function 'tool' expects a string (nass) or an array (jadwl), got " + targetType);
        }
        llvm::Value* offset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), -1);
        llvm::Value* lenPtr = builder.CreateGEP(llvm::Type::getInt64Ty(ctx), arr, offset);
        return builder.CreateLoad(llvm::Type::getInt64Ty(ctx), lenPtr, "array.len");
    }

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

    llvm::Type* varTy = it->second->getAllocatedType();
    if (varTy->isDoubleTy() && val->getType()->isIntegerTy()) {
        val = builder.CreateSIToFP(val, varTy);
    } else if (varTy->isIntegerTy() && val->getType()->isDoubleTy()) {
        val = builder.CreateFPToSI(val, varTy);
    }

    builder.CreateStore(val, it->second);
    return val;
}

llvm::Value* Codegen::genArrayLit(const ArrayLitExpr& e) {
    llvm::Function* mallocFn = module->getFunction("malloc");
    if (!mallocFn) throw std::runtime_error("malloc is not declared");

    if (e.elements.empty()) {
        llvm::Value* allocSize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 8);
        llvm::Value* rawPtr = builder.CreateCall(mallocFn, {allocSize});
        llvm::Value* lenVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
        builder.CreateStore(lenVal, rawPtr);
        llvm::Value* byteOffset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 8);
        return builder.CreateGEP(llvm::Type::getInt8Ty(ctx), rawPtr, byteOffset);
    }

    std::vector<llvm::Value*> values;
    for (auto& elem : e.elements) {
        values.push_back(genExpr(*elem));
    }

    llvm::Type* elemType = values[0]->getType();
    uint64_t elemSize = 8;
    if (elemType->isIntegerTy(1) || elemType->isIntegerTy(8)) {
        elemSize = 1;
    }

    llvm::Value* allocSize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 8 + values.size() * elemSize);
    llvm::Value* rawPtr = builder.CreateCall(mallocFn, {allocSize});

    llvm::Value* lenVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), values.size());
    builder.CreateStore(lenVal, rawPtr);

    llvm::Value* byteOffset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 8);
    llvm::Value* elemZeroPtr = builder.CreateGEP(llvm::Type::getInt8Ty(ctx), rawPtr, byteOffset);

    for (size_t i = 0; i < values.size(); ++i) {
        llvm::Value* idxVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), i);
        llvm::Value* elemPtr = builder.CreateGEP(elemType, elemZeroPtr, idxVal);
        builder.CreateStore(values[i], elemPtr);
    }

    return elemZeroPtr;
}

llvm::Value* Codegen::genIndex(const IndexExpr& e) {
    llvm::Value* arr = genExpr(*e.target);
    llvm::Value* idx = genExpr(*e.index);

    std::string targetType = inferType(*e.target);
    std::string elemTypeStr = "int";
    if (targetType.rfind("jadwl<", 0) == 0) {
        elemTypeStr = targetType.substr(6, targetType.size() - 7);
    }
    llvm::Type* elemType = getLLVMType(elemTypeStr);

    llvm::Value* elemPtr = builder.CreateGEP(elemType, arr, idx);
    return builder.CreateLoad(elemType, elemPtr, "array.index");
}

llvm::Value* Codegen::genIndexAssign(const IndexAssignExpr& e) {
    llvm::Value* arr = genExpr(*e.target);
    llvm::Value* idx = genExpr(*e.index);
    llvm::Value* val = genExpr(*e.value);

    std::string targetType = inferType(*e.target);
    std::string elemTypeStr = "int";
    if (targetType.rfind("jadwl<", 0) == 0) {
        elemTypeStr = targetType.substr(6, targetType.size() - 7);
    }
    llvm::Type* elemType = getLLVMType(elemTypeStr);

    llvm::Value* elemPtr = builder.CreateGEP(elemType, arr, idx);
    builder.CreateStore(val, elemPtr);
    return val;
}

void Codegen::declareStrcmp() {
    llvm::FunctionType* ft = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(ctx),
        {
            llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx)),
            llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx))
        },
        false);
    llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "strcmp", module.get());
}

void Codegen::declareMemcpy() {
    llvm::FunctionType* ft = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx)),
        {
            llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx)),
            llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx)),
            llvm::Type::getInt64Ty(ctx)
        },
        false);
    llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "memcpy", module.get());
}

llvm::Value* Codegen::genStructLit(const StructLitExpr& e) {
    llvm::Function* mallocFn = module->getFunction("malloc");
    if (!mallocFn) throw std::runtime_error("malloc is not declared");

    llvm::StructType* structTy = structTypes[e.structName];
    if (!structTy) throw std::runtime_error("Unknown struct type: " + e.structName);

    llvm::Value* sizeOfVal = llvm::ConstantExpr::getSizeOf(structTy);
    llvm::Value* rawPtr = builder.CreateCall(mallocFn, {sizeOfVal});
    llvm::Value* structPtr = builder.CreateBitCast(rawPtr, llvm::PointerType::getUnqual(structTy));

    const auto& fields = structFields[e.structName];
    const auto& defaultVals = structDefaultVals[e.structName];

    std::map<std::string, const Expr*> initMap;
    for (const auto& init : e.initializers) {
        initMap[init.first] = init.second.get();
    }

    for (size_t i = 0; i < fields.size(); ++i) {
        const std::string& fieldName = fields[i];
        llvm::Value* val = nullptr;

        auto it = initMap.find(fieldName);
        if (it != initMap.end()) {
            val = genExpr(*it->second);
        } else {
            auto dIt = defaultVals.find(fieldName);
            if (dIt != defaultVals.end()) {
                val = genExpr(*dIt->second);
            } else {
                llvm::Type* expectedFieldTy = structTy->getElementType(i);
                val = llvm::Constant::getNullValue(expectedFieldTy);
            }
        }

        llvm::Type* expectedFieldTy = structTy->getElementType(i);
        if (expectedFieldTy->isDoubleTy() && val->getType()->isIntegerTy()) {
            val = builder.CreateSIToFP(val, expectedFieldTy);
        } else if (expectedFieldTy->isIntegerTy() && val->getType()->isDoubleTy()) {
            val = builder.CreateFPToSI(val, expectedFieldTy);
        }

        llvm::Value* fieldPtr = builder.CreateStructGEP(structTy, structPtr, i);
        builder.CreateStore(val, fieldPtr);
    }

    return structPtr;
}

llvm::Value* Codegen::genMemberExpr(const MemberExpr& e) {
    llvm::Value* targetPtr = genExpr(*e.target);
    std::string targetType = inferType(*e.target);

    llvm::StructType* structTy = structTypes[targetType];
    if (!structTy) throw std::runtime_error("Unknown struct type: " + targetType);

    const auto& fields = structFields[targetType];
    auto it = std::find(fields.begin(), fields.end(), e.fieldName);
    if (it == fields.end()) throw std::runtime_error("Field not found: " + e.fieldName);
    size_t index = std::distance(fields.begin(), it);

    llvm::Value* fieldPtr = builder.CreateStructGEP(structTy, targetPtr, index);
    return builder.CreateLoad(structTy->getElementType(index), fieldPtr, "member." + e.fieldName);
}

llvm::Value* Codegen::genMemberAssign(const MemberAssignExpr& e) {
    llvm::Value* targetPtr = genExpr(*e.target);
    std::string targetType = inferType(*e.target);

    llvm::StructType* structTy = structTypes[targetType];
    if (!structTy) throw std::runtime_error("Unknown struct type: " + targetType);

    const auto& fields = structFields[targetType];
    auto it = std::find(fields.begin(), fields.end(), e.fieldName);
    if (it == fields.end()) throw std::runtime_error("Field not found: " + e.fieldName);
    size_t index = std::distance(fields.begin(), it);

    llvm::Value* val = genExpr(*e.value);

    llvm::Type* expectedFieldTy = structTy->getElementType(index);
    if (expectedFieldTy->isDoubleTy() && val->getType()->isIntegerTy()) {
        val = builder.CreateSIToFP(val, expectedFieldTy);
    } else if (expectedFieldTy->isIntegerTy() && val->getType()->isDoubleTy()) {
        val = builder.CreateFPToSI(val, expectedFieldTy);
    }

    llvm::Value* fieldPtr = builder.CreateStructGEP(structTy, targetPtr, index);
    builder.CreateStore(val, fieldPtr);
    return val;
}

llvm::Value* Codegen::genMethodCall(const MethodCallExpr& e) {
    std::string targetType = inferType(*e.target);
    std::string mangledName = targetType + "." + e.methodName;

    llvm::Function* fn = module->getFunction(mangledName);
    if (!fn) throw std::runtime_error("Unknown method: " + mangledName);

    std::vector<llvm::Value*> args;
    args.push_back(genExpr(*e.target));
    for (auto& a : e.args) args.push_back(genExpr(*a));

    return builder.CreateCall(fn, args);
}
