#include "Sema.h"
#include <stdexcept>

static void error(const std::string& msg) {
    throw std::runtime_error("[sema] " + msg);
}

void Sema::pushScope() {
    scopes.emplace_back();
}

void Sema::popScope() {
    scopes.pop_back();
}

void Sema::declareVar(const std::string& name, const std::string& type, bool isConst) {
    if (!scopes.empty() && scopes.back().count(name))
        error("Variable '" + name + "' already declared in this scope");
    scopes.back()[name] = {type, isConst};
}

Sema::VarInfo* Sema::lookupVar(const std::string& name) {
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
        auto it = scopes[i].find(name);
        if (it != scopes[i].end()) return &it->second;
    }
    return nullptr;
}

// ── Top-level ────────────────────────────────────────────────────────────────
void Sema::analyze(const Program& prog) {
    // Register built-in functions
    functions["printf"] = {-1, "int"};   // varargs
    functions["scanf"]  = {-1, "int"};

    pushScope(); // global scope

    // First pass: register all function declarations
    for (auto& s : prog.stmts) {
        if (auto* f = dynamic_cast<const FuncDecl*>(s.get()))
            functions[f->name] = {(int)f->params.size(), f->returnType};
    }

    // Second pass: check everything
    for (auto& s : prog.stmts)
        checkStmt(*s);

    popScope();
}

// ── Statements ───────────────────────────────────────────────────────────────
void Sema::checkStmt(const Stmt& stmt) {
    if (auto* s = dynamic_cast<const VarDeclStmt*>(&stmt))    { checkVarDecl(*s); return; }
    if (auto* s = dynamic_cast<const PrintStmt*>(&stmt))      { checkPrint(*s);   return; }
    if (auto* s = dynamic_cast<const ReadStmt*>(&stmt))       { checkRead(*s);    return; }
    if (auto* s = dynamic_cast<const IfStmt*>(&stmt))         { checkIf(*s);      return; }
    if (auto* s = dynamic_cast<const WhileStmt*>(&stmt))      { checkWhile(*s);   return; }
    if (auto* s = dynamic_cast<const ForStmt*>(&stmt))        { checkFor(*s);     return; }
    if (auto* s = dynamic_cast<const SwitchStmt*>(&stmt))     { checkSwitch(*s);  return; }
    if (auto* s = dynamic_cast<const TryCatchStmt*>(&stmt))   { checkTryCatch(*s); return; }
    if (dynamic_cast<const ImportStmt*>(&stmt))               { return; }
    if (auto* s = dynamic_cast<const ReturnStmt*>(&stmt))     { checkReturn(*s);  return; }
    if (auto* s = dynamic_cast<const FuncDecl*>(&stmt))       { checkFunc(*s);    return; }
    if (auto* s = dynamic_cast<const ExprStmt*>(&stmt))       { checkExpr(*s->expr); return; }
    if (dynamic_cast<const BreakStmt*>(&stmt)) {
        if (loopDepth == 0) error("a7bss used outside of a loop");
        return;
    }
    if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        if (loopDepth == 0) error("kml used outside of a loop");
        return;
    }
}

void Sema::checkBlock(const std::vector<StmtPtr>& block) {
    pushScope();
    for (auto& s : block) checkStmt(*s);
    popScope();
}

void Sema::checkVarDecl(const VarDeclStmt& s) {
    checkExpr(*s.init);
    std::string type = s.type.empty() ? "inferred" : s.type;
    declareVar(s.name, type, s.isConst);
}

void Sema::checkPrint(const PrintStmt& s) {
    for (auto& a : s.args) checkExpr(*a);
}

void Sema::checkRead(const ReadStmt& s) {
    if (!lookupVar(s.varName))
        error("Undefined variable '" + s.varName + "' in a9ra");
}

void Sema::checkIf(const IfStmt& s) {
    checkExpr(*s.condition);
    checkBlock(s.thenBlock);
    if (!s.elseBlock.empty())
        checkBlock(s.elseBlock);
}

void Sema::checkWhile(const WhileStmt& s) {
    checkExpr(*s.condition);
    ++loopDepth;
    checkBlock(s.body);
    --loopDepth;
}

void Sema::checkFor(const ForStmt& s) {
    pushScope(); // for-init scope
    checkStmt(*s.init);
    checkExpr(*s.condition);
    checkExpr(*s.update);
    ++loopDepth;
    checkBlock(s.body);
    --loopDepth;
    popScope();
}

void Sema::checkSwitch(const SwitchStmt& s) {
    checkExpr(*s.expr);
    for (auto& c : s.cases) {
        checkExpr(*c.value);
        checkBlock(c.body);
    }
}

void Sema::checkTryCatch(const TryCatchStmt& s) {
    checkBlock(s.tryBlock);
    checkBlock(s.catchBlock);
}

void Sema::checkReturn(const ReturnStmt& s) {
    if (functionDepth == 0)
        error("raja3 used outside of a function");
    if (s.value) checkExpr(*s.value);
}

void Sema::checkFunc(const FuncDecl& s) {
    ++functionDepth;
    pushScope();
    for (auto& p : s.params)
        declareVar(p.name, p.type, false);
    for (auto& st : s.body)
        checkStmt(*st);
    popScope();
    --functionDepth;
}

// ── Expressions ──────────────────────────────────────────────────────────────
void Sema::checkExpr(const Expr& expr) {
    if (dynamic_cast<const IntLitExpr*>(&expr))    return;
    if (dynamic_cast<const FloatLitExpr*>(&expr))  return;
    if (dynamic_cast<const StringLitExpr*>(&expr)) return;
    if (dynamic_cast<const BoolLitExpr*>(&expr))   return;
    if (dynamic_cast<const CharLitExpr*>(&expr))   return;

    if (auto* e = dynamic_cast<const VarExpr*>(&expr)) {
        if (!lookupVar(e->name))
            error("Undefined variable '" + e->name + "'");
        return;
    }
    if (auto* e = dynamic_cast<const BinaryExpr*>(&expr)) {
        checkExpr(*e->lhs);
        checkExpr(*e->rhs);
        return;
    }
    if (auto* e = dynamic_cast<const UnaryExpr*>(&expr)) {
        checkExpr(*e->operand);
        return;
    }
    if (auto* e = dynamic_cast<const CallExpr*>(&expr)) {
        auto it = functions.find(e->callee);
        if (it == functions.end())
            error("Undefined function '" + e->callee + "'");
        if (it->second.paramCount >= 0 && (int)e->args.size() != it->second.paramCount)
            error("Function '" + e->callee + "' expects " +
                  std::to_string(it->second.paramCount) + " arguments, got " +
                  std::to_string(e->args.size()));
        for (auto& a : e->args) checkExpr(*a);
        return;
    }
    if (auto* e = dynamic_cast<const AssignExpr*>(&expr)) {
        auto* info = lookupVar(e->name);
        if (!info)
            error("Undefined variable '" + e->name + "'");
        if (info->isConst)
            error("Cannot assign to constant '" + e->name + "'");
        checkExpr(*e->value);
        return;
    }
}
