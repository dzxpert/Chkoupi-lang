#include "Lexer.h"
#include <stdexcept>
#include <unordered_map>

// ─── Keyword table (Darija) ───────────────────────────────────────────────────
static const std::unordered_map<std::string, TokenKind> KEYWORDS = {
    // Declarations
    {"dir",         TokenKind::Dir},
    {"dima",        TokenKind::Dima},
    // Control flow
    {"idha",        TokenKind::Idha},
    {"idha_mknch",  TokenKind::Idha_mknch},
    {"ab9a_dor",    TokenKind::Ab9a_dor},
    {"dor",         TokenKind::Dor},
    // Switch / case
    {"bdl",         TokenKind::Bdl},
    {"khyr",        TokenKind::Khyr},
    // Exceptions
    {"jarb",        TokenKind::Jarb},
    {"ila_ghalt",   TokenKind::Ila_ghalt},
    // Functions
    {"dalla",       TokenKind::Dalla},
    {"raja3",       TokenKind::Raja3},
    // Import
    {"jibli",       TokenKind::Jibli},
    // Booleans
    {"sa7",         TokenKind::Sa7},
    {"ghalt",       TokenKind::Ghalt},
    {"walo",        TokenKind::Walo},
    // Types
    {"tabi3i",      TokenKind::TypeTabi3i},
    {"3ouchri",     TokenKind::Type3ouchri},
    {"5iyar",       TokenKind::Type5iyar},
    {"fargh",       TokenKind::TypeFargh},
    {"7arf",        TokenKind::Type7arf},
    {"nass",        TokenKind::TypeNass},
    {"jadwal",      TokenKind::TypeJadwal},
    {"9aleb",       TokenKind::Type9aleb},
    {"anwa3",       TokenKind::TypeAnwa3},
    {"ymkn",        TokenKind::TypeYmkn},
    // Built-ins
    {"ektb",        TokenKind::Ektb},
    {"a9ra",        TokenKind::A9ra},
    // Logical operators
    {"w",           TokenKind::W},
    {"wla",         TokenKind::Wla},
    {"machi",       TokenKind::Machi},
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
            while (pos < src.size() && peek() != '\n') advance();
        } else if (c == '/' && peek(1) == '*') {
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

// Identifiers can contain: letters, digits, and _ (including Arabic digits 3,5,7,9)
static bool isIdentStart(char c) {
    return std::isalpha(c) || c == '_';
}
static bool isIdentCont(char c) {
    return std::isalnum(c) || c == '_';
}

Token Lexer::readIdentifierOrKeyword() {
    std::string ident;
    while (pos < src.size() && isIdentCont(peek())) {
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
        // Keywords starting with digits: 3ouchri, 5iyar, 7arf
        // If a digit is immediately followed by a letter → it's a keyword, not a number
        if (std::isdigit(c) && std::isalpha(peek(1))) {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }
        if (std::isdigit(c)) { tokens.push_back(readNumber()); continue; }
        if (isIdentStart(c)) { tokens.push_back(readIdentifierOrKeyword()); continue; }

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
            case '[': tokens.push_back(makeToken(TokenKind::LBracket,  "[")); break;
            case ']': tokens.push_back(makeToken(TokenKind::RBracket,  "]")); break;
            case ';': tokens.push_back(makeToken(TokenKind::Semicolon, ";")); break;
            case ':': tokens.push_back(makeToken(TokenKind::Colon,     ":")); break;
            case ',': tokens.push_back(makeToken(TokenKind::Comma,     ",")); break;
            case '.': tokens.push_back(makeToken(TokenKind::Dot,       ".")); break;
            default:  tokens.push_back(makeToken(TokenKind::Unknown,   std::string(1, c))); break;
        }
    }

    return tokens;
}
