#pragma once
#include <memory>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  AST Node hierarchy for Chkoupi-lang
// ─────────────────────────────────────────────────────────────────────────────

struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// ── Expressions ──────────────────────────────────────────────────────────────

struct Expr {
    virtual ~Expr() = default;
};

struct IntLitExpr : Expr {
    long long value;
};

struct FloatLitExpr : Expr {
    double value;
};

struct StringLitExpr : Expr {
    std::string value;
};

struct BoolLitExpr : Expr {
    bool value;  // s7i7 / ghalt
};

struct VarExpr : Expr {
    std::string name;
};

struct BinaryExpr : Expr {
    std::string op;   // "+", "-", "==", "and", "or", etc.
    ExprPtr     lhs;
    ExprPtr     rhs;
};

struct UnaryExpr : Expr {
    std::string op;   // "machi" (not), "-"
    ExprPtr     operand;
};

struct CallExpr : Expr {
    std::string            callee;
    std::vector<ExprPtr>   args;
};

struct AssignExpr : Expr {
    std::string name;
    ExprPtr     value;
};

// ── Statements ───────────────────────────────────────────────────────────────

struct Stmt {
    virtual ~Stmt() = default;
};

// achfa x = <expr>   (let)
struct VarDeclStmt : Stmt {
    std::string name;
    std::string type;   // "int", "float", "bool", "string", ""=infer
    bool        isConst;   // ab9a_dayr
    ExprPtr     init;
};

// ektb(<expr>, ...)   (printf)
struct PrintStmt : Stmt {
    std::vector<ExprPtr> args;
};

// a9ra(<var>)   (scanf)
struct ReadStmt : Stmt {
    std::string varName;
};

// idha (...) { ... } wla { ... }
struct IfStmt : Stmt {
    ExprPtr             condition;
    std::vector<StmtPtr> thenBlock;
    std::vector<StmtPtr> elseBlock;
};

// ki_tkoon (...) { ... }
struct WhileStmt : Stmt {
    ExprPtr              condition;
    std::vector<StmtPtr> body;
};

// madam (init; cond; update) { ... }
struct ForStmt : Stmt {
    StmtPtr              init;
    ExprPtr              condition;
    ExprPtr              update;
    std::vector<StmtPtr> body;
};

// raje3 <expr>
struct ReturnStmt : Stmt {
    ExprPtr value;
};

// Bare expression statement
struct ExprStmt : Stmt {
    ExprPtr expr;
};

// fun name(params) -> type { body }
struct FuncDecl : Stmt {
    struct Param { std::string name, type; };
    std::string          name;
    std::vector<Param>   params;
    std::string          returnType;
    std::vector<StmtPtr> body;
};

// Top-level program
struct Program {
    std::vector<StmtPtr> stmts;
};
