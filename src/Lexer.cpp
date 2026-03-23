#include "Lexer.h"
#include <stdexcept>
#include <unordered_map>

// ─── Keyword table ────────────────────────────────────────────────────────────
static const std::unordered_map<std::string, TokenKind> KEYWORDS = {
    {"achfa",       TokenKind::Achfa},
    {"ab9a_dayr",   TokenKind::Ab9a_dayr},
    {"idha",        TokenKind::Idha},
    {"wla",         TokenKind::Wla},
    {"ki_tkoon",    TokenKind::Ki_tkoon},
    {"madam",       TokenKind::Madam},
    {"s7i7",        TokenKind::S7i7},
    {"ghalt",       TokenKind::Ghalt},
    {"fun",         TokenKind::Fun},
    {"raje3",       TokenKind::Raje3},
    {"void",        TokenKind::Void},
    {"int",         TokenKind::TypeInt},
    {"float",       TokenKind::TypeFloat},
    {"bool",        TokenKind::TypeBool},
    {"string",      TokenKind::TypeString},
    {"ektb",        TokenKind::Ektb},
    {"a9ra",        TokenKind::A9ra},
    {"w",           TokenKind::And},       // logical and
    {"wla_had",     TokenKind::Or},        // logical or
    {"machi",       TokenKind::Not},       // logical not
};

Lexer::Lexer(std::string source) : src(std::move(source)) {}

char Lexer::peek(int offset) const {
    size_t idx = pos + offset;
    return (idx < src.size()) ? src[idx] : '\0';
}

char Lexer::advance() {
    char c = src[pos++];
    if (c == '\n') { ++line; col = 1; }
    else           { ++col; }
    return c;
}

Token Lexer::makeToken(TokenKind kind, std::string lexeme) {
    return Token{kind, std::move(lexeme), line, col};
}

void Lexer::skipWhitespaceAndComments() {
    while (pos < src.size()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            // Single-line comment
            while (pos < src.size() && peek() != '\n') advance();
        } else if (c == '/' && peek(1) == '*') {
            // Multi-line comment
            advance(); advance();
            while (pos < src.size()) {
                if (peek() == '*' && peek(1) == '/') { advance(); advance(); break; }
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::readString() {
    advance(); // consume opening "
    std::string val;
    while (pos < src.size() && peek() != '"') {
        char c = advance();
        if (c == '\\') {
            char esc = advance();
            switch (esc) {
                case 'n':  val += '\n'; break;
                case 't':  val += '\t'; break;
                case '"':  val += '"';  break;
                case '\\': val += '\\'; break;
                default:   val += esc;  break;
            }
        } else {
            val += c;
        }
    }
    if (pos >= src.size())
        throw std::runtime_error("Unterminated string literal");
    advance(); // consume closing "
    return makeToken(TokenKind::String, val);
}

Token Lexer::readNumber() {
    std::string num;
    bool isFloat = false;
    while (pos < src.size() && (std::isdigit(peek()) || peek() == '.')) {
        if (peek() == '.') isFloat = true;
        num += advance();
    }
    return makeToken(isFloat ? TokenKind::Float : TokenKind::Integer, num);
}

Token Lexer::readIdentifierOrKeyword() {
    std::string ident;
    // Allow letters, digits, underscore, and arabic-style numerals in identifiers (9, 7, etc.)
    while (pos < src.size() && (std::isalnum(peek()) || peek() == '_')) {
        ident += advance();
    }
    auto it = KEYWORDS.find(ident);
    TokenKind kind = (it != KEYWORDS.end()) ? it->second : TokenKind::Identifier;
    return makeToken(kind, ident);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespaceAndComments();
        if (pos >= src.size()) {
            tokens.push_back(makeToken(TokenKind::Eof, ""));
            break;
        }

        char c = peek();

        if (c == '"') { tokens.push_back(readString()); continue; }
        if (std::isdigit(c)) { tokens.push_back(readNumber()); continue; }
        if (std::isalpha(c) || c == '_') { tokens.push_back(readIdentifierOrKeyword()); continue; }

        // Operators and punctuation
        advance();
        switch (c) {
            case '+': tokens.push_back(makeToken(TokenKind::Plus,      "+")); break;
            case '-':
                if (peek() == '>') { advance(); tokens.push_back(makeToken(TokenKind::Arrow, "->")); }
                else               tokens.push_back(makeToken(TokenKind::Minus, "-"));
                break;
            case '*': tokens.push_back(makeToken(TokenKind::Star,      "*")); break;
            case '/': tokens.push_back(makeToken(TokenKind::Slash,     "/")); break;
            case '%': tokens.push_back(makeToken(TokenKind::Percent,   "%")); break;
            case '=':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenKind::EqEq,   "==")); }
                else               tokens.push_back(makeToken(TokenKind::Eq, "="));
                break;
            case '!':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenKind::BangEq, "!=")); }
                break;
            case '<':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenKind::LtEq,   "<=")); }
                else               tokens.push_back(makeToken(TokenKind::Lt, "<"));
                break;
            case '>':
                if (peek() == '=') { advance(); tokens.push_back(makeToken(TokenKind::GtEq,   ">=")); }
                else               tokens.push_back(makeToken(TokenKind::Gt, ">"));
                break;
            case '(': tokens.push_back(makeToken(TokenKind::LParen,    "(")); break;
            case ')': tokens.push_back(makeToken(TokenKind::RParen,    ")")); break;
            case '{': tokens.push_back(makeToken(TokenKind::LBrace,    "{")); break;
            case '}': tokens.push_back(makeToken(TokenKind::RBrace,    "}")); break;
            case ';': tokens.push_back(makeToken(TokenKind::Semicolon, ";")); break;
            case ':': tokens.push_back(makeToken(TokenKind::Colon,     ":")); break;
            case ',': tokens.push_back(makeToken(TokenKind::Comma,     ",")); break;
            default:  tokens.push_back(makeToken(TokenKind::Unknown,   std::string(1, c))); break;
        }
    }

    return tokens;
}
