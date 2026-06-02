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

struct Expr { virtual ~Expr() = default; };

struct IntLitExpr    : Expr { long long  value; };
struct FloatLitExpr  : Expr { double     value; };
struct StringLitExpr : Expr { std::string value; };
struct BoolLitExpr   : Expr { bool       value; };  // sa7 / ghalt
struct CharLitExpr   : Expr { char       value; };  // 'a'

struct VarExpr : Expr { std::string name; };

struct BinaryExpr : Expr {
    std::string op;   // "+", "-", "==", "and", "or", etc.
    ExprPtr lhs, rhs;
};

struct UnaryExpr : Expr {
    std::string op;   // "machi", "-"
    ExprPtr operand;
};

struct CallExpr : Expr {
    std::string          callee;
    std::vector<ExprPtr> args;
};

struct AssignExpr : Expr {
    std::string name;
    ExprPtr     value;
};

struct ArrayLitExpr : Expr {
    std::vector<ExprPtr> elements;
};

struct IndexExpr : Expr {
    ExprPtr target;
    ExprPtr index;
};

struct IndexAssignExpr : Expr {
    ExprPtr target;
    ExprPtr index;
    ExprPtr value;
};

// ── Statements ───────────────────────────────────────────────────────────────

struct Stmt { virtual ~Stmt() = default; };

// dir x = <expr>  /  dima x = <expr>
struct VarDeclStmt : Stmt {
    std::string name;
    std::string type;     // "int", "float", "bool", "string", "char", "void", ""=infer
    bool        isConst;  // dima
    ExprPtr     init;
};

// ektb(...)   →  printf
struct PrintStmt : Stmt {
    std::vector<ExprPtr> args;
};

// a9ra(x)   →  scanf
struct ReadStmt : Stmt {
    std::string varName;
};

// idha (...) { ... } idha_mknch { ... }
struct IfStmt : Stmt {
    ExprPtr              condition;
    std::vector<StmtPtr> thenBlock;
    std::vector<StmtPtr> elseBlock;
};

// ab9a_dor (...) { ... }
struct WhileStmt : Stmt {
    ExprPtr              condition;
    std::vector<StmtPtr> body;
};

// dor (init; cond; update) { ... }
struct ForStmt : Stmt {
    StmtPtr              init;
    ExprPtr              condition;
    ExprPtr              update;
    std::vector<StmtPtr> body;
};

// bdl (expr) { khyr val: ... }
struct SwitchStmt : Stmt {
    struct Case {
        ExprPtr              value;
        std::vector<StmtPtr> body;
    };
    ExprPtr          expr;
    std::vector<Case> cases;
};

// jarb { ... } ila_ghalt { ... }
struct TryCatchStmt : Stmt {
    std::vector<StmtPtr> tryBlock;
    std::vector<StmtPtr> catchBlock;
};

// jibli "path";
struct ImportStmt : Stmt {
    std::string path;
};

// raja3 <expr>
struct ReturnStmt : Stmt {
    ExprPtr value;
};

// a7bss (break)
struct BreakStmt : Stmt {};

// kml (continue)
struct ContinueStmt : Stmt {};

// Bare expression statement
struct ExprStmt : Stmt {
    ExprPtr expr;
};

// dalla name(params) -> type { body }
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
