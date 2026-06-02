#pragma once
#include "AST.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Codegen {
public:
    Codegen();
    void generate(const Program& prog);
    void dumpIR() const;
    void writeObjectFile(const std::string& path) const;
    void runJIT();   // JIT compile + execute (default mode)

private:
    llvm::LLVMContext                        ctx;
    llvm::IRBuilder<>                        builder;
    std::unique_ptr<llvm::Module>            module;

    // Symbol table: name -> alloca instruction
    std::map<std::string, llvm::AllocaInst*> namedValues;
    // Const flag: name -> true if dima
    std::map<std::string, bool>              constFlags;
    // Chkoupi types for variables and functions (used for AST-based type inference)
    std::map<std::string, std::string>       variableTypes;
    std::map<std::string, std::string>       functionReturnTypes;

    // Loop context stack for break/continue
    struct LoopContext {
        llvm::BasicBlock* breakBB;     // where a7bss jumps to
        llvm::BasicBlock* continueBB;  // where kml jumps to
    };
    std::vector<LoopContext>                 loopStack;

    // Helpers
    llvm::Type*      getLLVMType(const std::string& typeName);
    llvm::Function*  currentFunction = nullptr;
    llvm::AllocaInst* createEntryAlloca(llvm::Function* fn,
                                        const std::string& name,
                                        llvm::Type* ty);
    void declarePrintf();
    void declareScanf();
    void declareMalloc();
    std::string inferType(const Expr& expr);

    // Code generation visitors
    void     genStmt(const Stmt& stmt);
    void     genVarDecl(const VarDeclStmt& s);
    void     genPrint(const PrintStmt& s);
    void     genRead(const ReadStmt& s);
    void     genIf(const IfStmt& s);
    void     genWhile(const WhileStmt& s);
    void     genFor(const ForStmt& s);
    void     genSwitch(const SwitchStmt& s);
    void     genTryCatch(const TryCatchStmt& s);
    void     genReturn(const ReturnStmt& s);
    void     genBreak();
    void     genContinue();
    void     genFunc(const FuncDecl& s);
    void     genBlock(const std::vector<StmtPtr>& block);

    llvm::Value* genExpr(const Expr& expr);
    llvm::Value* genBinary(const BinaryExpr& e);
    llvm::Value* genLogicalAnd(const BinaryExpr& e);
    llvm::Value* genLogicalOr(const BinaryExpr& e);
    llvm::Value* genUnary(const UnaryExpr& e);
    llvm::Value* genCall(const CallExpr& e);
    llvm::Value* genAssign(const AssignExpr& e);
    llvm::Value* genArrayLit(const ArrayLitExpr& e);
    llvm::Value* genIndex(const IndexExpr& e);
    llvm::Value* genIndexAssign(const IndexAssignExpr& e);
};
