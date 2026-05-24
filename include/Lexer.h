#pragma once
#include "Token.h"
#include <string>
#include <vector>

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> tokenize();

private:
    std::string src;
    size_t      pos  = 0;
    int         line = 1;
    int         col  = 1;

    char peek(int offset = 0) const;
    char advance();
    void skipWhitespaceAndComments();
    Token readString();
    Token readChar();
    Token readNumber();
    Token readIdentifierOrKeyword();
    Token makeToken(TokenKind kind, std::string lexeme);
};
