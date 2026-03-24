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

// ── Complex type expressions ─────────────────────────────────────────────────

// Array literal: [1, 2, 3]
struct ArrayLitExpr : Expr {
    std::vector<ExprPtr> elements;
};

// Array index read: arr[i]
struct IndexExpr : Expr {
    ExprPtr object;
    ExprPtr index;
};

// Array index write: arr[i] = val
struct IndexAssignExpr : Expr {
    ExprPtr object;
    ExprPtr index;
    ExprPtr value;
};

// Struct literal: Insan { ism: "brahim", 3omr: 25 }
struct StructLitExpr : Expr {
    std::string structName;
    std::vector<std::pair<std::string, ExprPtr>> fieldInits;
};

// Field access read: obj.field
struct FieldAccessExpr : Expr {
    ExprPtr     object;
    std::string field;
};

// Field access write: obj.field = val
struct FieldAssignExpr : Expr {
    ExprPtr     object;
    std::string field;
    ExprPtr     value;
};

// Null literal: walo
struct NullLitExpr : Expr {};

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

// 9aleb Name { field: type; ... }
struct StructDecl : Stmt {
    struct Field { std::string name, type; };
    std::string          name;
    std::vector<Field>   fields;
};

// anwa3 Name { variant; ... }
struct EnumDecl : Stmt {
    std::string              name;
    std::vector<std::string> variants;
};

// raja3 <expr>
struct ReturnStmt : Stmt {
    ExprPtr value;
};

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
