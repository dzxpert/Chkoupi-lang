#pragma once
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Token kinds for Chkoupi-lang
//  Darija keywords are used directly; English names are comments.
// ─────────────────────────────────────────────────────────────────────────────
enum class TokenKind {
    // Literals
    Integer,        // 42
    Float,          // 3.14
    String,         // "..."
    Identifier,     // foo

    // Keywords
    Achfa,          // let / variable declaration
    Ab9a_dayr,      // const / immutable declaration
    Idha,           // if
    Wla,            // else
    Ki_tkoon,       // while
    Madam,          // for
    S7i7,           // true
    Ghalt,          // false
    Fun,            // function definition
    Raje3,          // return
    Void,           // void type

    // Types
    TypeInt,        // int
    TypeFloat,      // float
    TypeBool,       // bool
    TypeString,     // string

    // Built-ins
    Ektb,           // printf / print
    A9ra,           // scanf / read input

    // Operators
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Percent,        // %
    EqEq,           // ==
    BangEq,         // !=
    Lt,             // <
    Gt,             // >
    LtEq,           // <=
    GtEq,           // >=
    And,            // w  (and)
    Or,             // wla_had  (or)
    Not,            // machi  (not)
    Eq,             // =

    // Punctuation
    LParen,         // (
    RParen,         // )
    LBrace,         // {
    RBrace,         // }
    Semicolon,      // ;
    Colon,          // :
    Comma,          // ,
    Arrow,          // ->

    // Meta
    Eof,
    Unknown
};

struct Token {
    TokenKind kind;
    std::string lexeme;   // raw text
    int         line;
    int         col;
};
