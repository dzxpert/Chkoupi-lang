#pragma once
#include "Token.h"
#include "AST.h"
#include <vector>

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parse();

private:
    std::vector<Token> tokens;
    size_t             pos = 0;

    Token&  peek(int offset = 0);
    Token   advance();
    bool    check(TokenKind kind);
    bool    match(TokenKind kind);
    Token   expect(TokenKind kind, const char* msg);

    // Statements
    StmtPtr parseStmt();
    StmtPtr parseVarDecl(bool isConst);
    StmtPtr parsePrint();
    StmtPtr parseRead();
    StmtPtr parseIf();
    StmtPtr parseWhile();
    StmtPtr parseFor();
    StmtPtr parseReturn();
    StmtPtr parseFuncDecl();
    std::vector<StmtPtr> parseBlock();

    // Expressions (Pratt-style precedence)
    ExprPtr parseExpr();
    ExprPtr parseAssign();
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseTerm();
    ExprPtr parseFactor();
    ExprPtr parseUnary();
    ExprPtr parseCall();
    ExprPtr parsePrimary();
};
