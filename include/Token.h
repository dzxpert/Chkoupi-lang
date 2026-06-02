#pragma once
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Token kinds for Chkoupi-lang
//  All keywords are in Algerian Darija.
// ─────────────────────────────────────────────────────────────────────────────
enum class TokenKind {
    // Literals
    Integer,        // 42
    Float,          // 3.14
    String,         // "..."
    Char,           // 'a'
    Identifier,     // foo

    // Keywords — declarations
    Dir,            // dir x = ...   (let)
    Dima,           // dima x = ...  (const)

    // Keywords — control flow
    Idha,           // idha           (if)
    Idha_mknch,     // idha_mknch     (else)
    Ab9a_dor,       // ab9a_dor       (while)
    Dor,            // dor            (for)
    A7bss,          // a7bss          (break)
    Kml,            // kml            (continue)

    // Keywords — switch/case
    Bdl,            // bdl            (switch)
    Khyr,           // khyr           (case)

    // Keywords — exceptions
    Jarb,           // jarb           (try)
    Ila_ghalt,      // ila_ghalt      (except/catch)

    // Keywords — functions
    Dalla,          // dalla          (function definition)
    Raja3,          // raja3          (return)

    // Keywords — import
    Jibli,          // jibli          (import)

    // Boolean literals
    Sa7,            // sa7            (true)
    Ghalt,          // ghalt          (false)

    // Types (Darija)
    TypeTabi3i,     // tabi3i         (int)
    Type3ouchri,    // 3ouchri        (float)
    Type5iyar,      // 5iyar          (bool)
    TypeFargh,      // fargh          (void)
    Type7arf,       // 7arf           (char)
    TypeNass,       // nass           (string)
    TypeJadwl,      // jadwl          (array)

    // Built-ins
    Ektb,           // ektb(...)      (printf)
    A9ra,           // a9ra(x)        (scanf)

    // Logical operators (keywords)
    W,              // w              (and)
    Wla,            // wla            (or)
    Machi,          // machi          (not)

    // Arithmetic operators
    Plus,           // +
    Minus,          // -
    PlusPlus,       // ++
    MinusMinus,     // --
    Star,           // *
    Slash,          // /
    Percent,        // %

    // Comparison operators
    EqEq,           // ==
    BangEq,         // !=
    Lt,             // <
    Gt,             // >
    LtEq,           // <=
    GtEq,           // >=

    // Assignment
    Eq,             // =
    PlusEq,         // +=
    MinusEq,        // -=
    StarEq,         // *=
    SlashEq,        // /=
    PercentEq,      // %=

    // Punctuation
    LParen,         // (
    RParen,         // )
    LBrace,         // {
    RBrace,         // }
    LBracket,       // [
    RBracket,       // ]
    Semicolon,      // ;
    Colon,          // :
    Comma,          // ,
    Arrow,          // ->
    Dot,            // .

    // Meta
    Eof,
    Unknown
};

struct Token {
    TokenKind   kind;
    std::string lexeme;   // raw text from source
    int         line;
    int         col;
};
