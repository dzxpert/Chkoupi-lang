#pragma once
#include "AST.h"
#include <map>
#include <set>
#include <string>
#include <vector>

class Sema {
public:
    // Analyze the program. Throws on first error with line-context message.
    void analyze(const Program& prog);

private:
    struct VarInfo {
        std::string type;  // "int", "float", etc.
        bool isConst;
    };

    struct FuncInfo {
        int paramCount;
        std::string returnType;
    };

    struct StructInfo {
        std::map<std::string, std::string> fields;
        std::set<std::string> fieldsWithDefaults;
    };

    // Scope stack: each scope is a map of variable names
    std::vector<std::map<std::string, VarInfo>> scopes;

    // Known functions
    std::map<std::string, FuncInfo> functions;

    // Known structs
    std::map<std::string, StructInfo> structs;

    int loopDepth    = 0;
    int functionDepth = 0;

    void pushScope();
    void popScope();
    void declareVar(const std::string& name, const std::string& type, bool isConst);
    VarInfo* lookupVar(const std::string& name);

    void checkStmt(const Stmt& stmt);
    void checkVarDecl(const VarDeclStmt& s);
    void checkPrint(const PrintStmt& s);
    void checkRead(const ReadStmt& s);
    void checkIf(const IfStmt& s);
    void checkWhile(const WhileStmt& s);
    void checkFor(const ForStmt& s);
    void checkSwitch(const SwitchStmt& s);
    void checkTryCatch(const TryCatchStmt& s);
    void checkReturn(const ReturnStmt& s);
    void checkFunc(const FuncDecl& s);
    void checkStructDecl(const StructDeclStmt& s);
    void checkFree(const FreeStmt& s);
    void checkBlock(const std::vector<StmtPtr>& block);

    void checkExpr(const Expr& expr);
    std::string inferType(const Expr& expr);
};
