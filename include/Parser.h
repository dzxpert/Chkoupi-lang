#pragma once
#include "Token.h"
#include "AST.h"
#include <vector>

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parse();
    std::string currentDir = ".";

private:
    std::vector<StmtPtr> resolveImport(const std::string& path);
    std::vector<Token> tokens;
    size_t             pos = 0;

    Token&  peek(int offset = 0);
    Token   advance();
    bool    check(TokenKind kind);
    bool    match(TokenKind kind);
    Token   expect(TokenKind kind, const char* msg);

    // Statements
    StmtPtr parseStmt();
    StmtPtr parseVarDecl(bool isConst);   // dir / dima
    StmtPtr parsePrint();                  // ektb
    StmtPtr parseRead();                   // a9ra
    StmtPtr parseIf();                     // idha / idha_mknch
    StmtPtr parseWhile();                  // ab9a_dor
    StmtPtr parseFor();                    // dor
    StmtPtr parseSwitch();                 // bdl / khyr
    StmtPtr parseTryCatch();               // jarb / ila_ghalt
    StmtPtr parseImport();                 // jibli
    StmtPtr parseReturn();                 // raja3
    StmtPtr parseBreak();                  // a7bss
    StmtPtr parseContinue();               // kml
    StmtPtr parseFuncDecl();               // dalla
    std::vector<StmtPtr> parseBlock();
    std::string          parseType();

    // Expressions (Pratt-style precedence)
    ExprPtr parseExpr();
    ExprPtr parseAssign();
    ExprPtr parseOr();          // wla
    ExprPtr parseAnd();         // w
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseTerm();
    ExprPtr parseFactor();
    ExprPtr parseUnary();       // machi / -
    ExprPtr parseCall();
    ExprPtr parsePrimary();
};
