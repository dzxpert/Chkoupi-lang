#include "Parser.h"
#include <stdexcept>

Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

Token& Parser::peek(int offset) {
    size_t idx = pos + offset;
    if (idx >= tokens.size()) return tokens.back();
    return tokens[idx];
}

Token Parser::advance() {
    if (pos < tokens.size()) return tokens[pos++];
    return tokens.back();
}

bool Parser::check(TokenKind kind) {
    return peek().kind == kind;
}

bool Parser::match(TokenKind kind) {
    if (check(kind)) { advance(); return true; }
    return false;
}

Token Parser::expect(TokenKind kind, const char* msg) {
    if (!check(kind))
        throw std::runtime_error(std::string(msg) + " at line " + std::to_string(peek().line));
    return advance();
}

// ── Program ───────────────────────────────────────────────────────────────────
Program Parser::parse() {
    Program prog;
    while (!check(TokenKind::Eof)) {
        prog.stmts.push_back(parseStmt());
    }
    return prog;
}

// ── Type helper ───────────────────────────────────────────────────────────────
static std::string tokenToType(TokenKind k) {
    switch (k) {
        case TokenKind::TypeTabi3i:  return "int";
        case TokenKind::Type3ouchri: return "float";
        case TokenKind::Type5iyar:   return "bool";
        case TokenKind::TypeFargh:   return "void";
        case TokenKind::Type7arf:    return "char";
        case TokenKind::TypeNass:    return "string";
        default:                     return "";
    }
}

static bool isTypeToken(TokenKind k) {
    return k == TokenKind::TypeTabi3i  || k == TokenKind::Type3ouchri ||
           k == TokenKind::Type5iyar   || k == TokenKind::TypeFargh   ||
           k == TokenKind::Type7arf    || k == TokenKind::TypeNass;
}

// ── Statements ────────────────────────────────────────────────────────────────
StmtPtr Parser::parseStmt() {
    if (check(TokenKind::Dir))        return parseVarDecl(false);
    if (check(TokenKind::Dima))       return parseVarDecl(true);
    if (check(TokenKind::Ektb))       return parsePrint();
    if (check(TokenKind::A9ra))       return parseRead();
    if (check(TokenKind::Idha))       return parseIf();
    if (check(TokenKind::Ab9a_dor))   return parseWhile();
    if (check(TokenKind::Dor))        return parseFor();
    if (check(TokenKind::Raja3))      return parseReturn();
    if (check(TokenKind::Dalla))      return parseFuncDecl();
    if (check(TokenKind::Bdl))        return parseSwitch();
    if (check(TokenKind::Jarb))       return parseTryCatch();
    if (check(TokenKind::Jibli))      return parseImport();

    // Expression statement
    auto expr = std::make_unique<ExprStmt>();
    expr->expr = parseExpr();
    expect(TokenKind::Semicolon, "Expected ';' after expression");
    return expr;
}

// dir x : tabi3i = <expr>;   or   dir x = <expr>;
StmtPtr Parser::parseVarDecl(bool isConst) {
    advance(); // consume dir / dima
    auto s = std::make_unique<VarDeclStmt>();
    s->isConst = isConst;
    s->name = expect(TokenKind::Identifier, "Expected variable name").lexeme;

    // Optional type annotation  : tabi3i
    if (match(TokenKind::Colon)) {
        if (!isTypeToken(peek().kind))
            throw std::runtime_error("Expected type name at line " + std::to_string(peek().line));
        s->type = tokenToType(advance().kind);
    }

    expect(TokenKind::Eq, "Expected '=' in variable declaration");
    s->init = parseExpr();
    expect(TokenKind::Semicolon, "Expected ';' after declaration");
    return s;
}

// ektb("format", args...);
StmtPtr Parser::parsePrint() {
    advance(); // consume ektb
    expect(TokenKind::LParen, "Expected '(' after ektb");
    auto s = std::make_unique<PrintStmt>();
    if (!check(TokenKind::RParen)) {
        s->args.push_back(parseExpr());
        while (match(TokenKind::Comma))
            s->args.push_back(parseExpr());
    }
    expect(TokenKind::RParen, "Expected ')' after ektb args");
    expect(TokenKind::Semicolon, "Expected ';'");
    return s;
}

// a9ra(x);
StmtPtr Parser::parseRead() {
    advance(); // consume a9ra
    expect(TokenKind::LParen, "Expected '(' after a9ra");
    auto s = std::make_unique<ReadStmt>();
    s->varName = expect(TokenKind::Identifier, "Expected variable name").lexeme;
    expect(TokenKind::RParen, "Expected ')'");
    expect(TokenKind::Semicolon, "Expected ';'");
    return s;
}

// idha (cond) { ... } idha_mknch { ... }
StmtPtr Parser::parseIf() {
    advance(); // consume idha
    expect(TokenKind::LParen, "Expected '(' after idha");
    auto s = std::make_unique<IfStmt>();
    s->condition = parseExpr();
    expect(TokenKind::RParen, "Expected ')'");
    s->thenBlock = parseBlock();
    if (match(TokenKind::Idha_mknch))
        s->elseBlock = parseBlock();
    return s;
}

// ab9a_dor (cond) { ... }
StmtPtr Parser::parseWhile() {
    advance(); // consume ab9a_dor
    expect(TokenKind::LParen, "Expected '(' after ab9a_dor");
    auto s = std::make_unique<WhileStmt>();
    s->condition = parseExpr();
    expect(TokenKind::RParen, "Expected ')'");
    s->body = parseBlock();
    return s;
}

// dor (init; cond; update) { ... }
StmtPtr Parser::parseFor() {
    advance(); // consume dor
    expect(TokenKind::LParen, "Expected '(' after dor");
    auto s = std::make_unique<ForStmt>();

    if (check(TokenKind::Dir))
        s->init = parseVarDecl(false);
    else {
        auto es = std::make_unique<ExprStmt>();
        es->expr = parseExpr();
        expect(TokenKind::Semicolon, "Expected ';' in for-init");
        s->init = std::move(es);
    }

    s->condition = parseExpr();
    expect(TokenKind::Semicolon, "Expected ';' in for-condition");
    s->update = parseExpr();
    expect(TokenKind::RParen, "Expected ')' after for-update");
    s->body = parseBlock();
    return s;
}

// raja3 <expr>;
StmtPtr Parser::parseReturn() {
    advance(); // consume raja3
    auto s = std::make_unique<ReturnStmt>();
    if (!check(TokenKind::Semicolon))
        s->value = parseExpr();
    expect(TokenKind::Semicolon, "Expected ';' after raja3");
    return s;
}

// dalla name(p: tabi3i, ...) -> tabi3i { ... }
StmtPtr Parser::parseFuncDecl() {
    advance(); // consume dalla
    auto s = std::make_unique<FuncDecl>();
    s->name = expect(TokenKind::Identifier, "Expected function name").lexeme;
    expect(TokenKind::LParen, "Expected '('");
    while (!check(TokenKind::RParen) && !check(TokenKind::Eof)) {
        FuncDecl::Param p;
        p.name = expect(TokenKind::Identifier, "Expected param name").lexeme;
        expect(TokenKind::Colon, "Expected ':' after param name");
        if (!isTypeToken(peek().kind))
            throw std::runtime_error("Expected type name at line " + std::to_string(peek().line));
        p.type = tokenToType(advance().kind);
        s->params.push_back(std::move(p));
        match(TokenKind::Comma);
    }
    expect(TokenKind::RParen, "Expected ')'");
    s->returnType = "void";
    if (match(TokenKind::Arrow)) {
        if (!isTypeToken(peek().kind))
            throw std::runtime_error("Expected return type at line " + std::to_string(peek().line));
        s->returnType = tokenToType(advance().kind);
    }
    s->body = parseBlock();
    return s;
}

// bdl (expr) { khyr val: ... }
StmtPtr Parser::parseSwitch() {
    advance(); // consume bdl
    expect(TokenKind::LParen, "Expected '(' after bdl");
    auto s = std::make_unique<SwitchStmt>();
    s->expr = parseExpr();
    expect(TokenKind::RParen, "Expected ')'");
    expect(TokenKind::LBrace, "Expected '{'");
    while (!check(TokenKind::RBrace) && !check(TokenKind::Eof)) {
        expect(TokenKind::Khyr, "Expected 'khyr' in bdl");
        SwitchStmt::Case c;
        c.value = parseExpr();
        expect(TokenKind::Colon, "Expected ':' after khyr value");
        while (!check(TokenKind::Khyr) && !check(TokenKind::RBrace) && !check(TokenKind::Eof))
            c.body.push_back(parseStmt());
        s->cases.push_back(std::move(c));
    }
    expect(TokenKind::RBrace, "Expected '}'");
    return s;
}

// jarb { ... } ila_ghalt { ... }
StmtPtr Parser::parseTryCatch() {
    advance(); // consume jarb
    auto s = std::make_unique<TryCatchStmt>();
    s->tryBlock = parseBlock();
    expect(TokenKind::Ila_ghalt, "Expected 'ila_ghalt' after jarb block");
    s->catchBlock = parseBlock();
    return s;
}

// jibli "module";
StmtPtr Parser::parseImport() {
    advance(); // consume jibli
    auto s = std::make_unique<ImportStmt>();
    s->path = expect(TokenKind::String, "Expected module path string after jibli").lexeme;
    expect(TokenKind::Semicolon, "Expected ';'");
    return s;
}

std::vector<StmtPtr> Parser::parseBlock() {
    expect(TokenKind::LBrace, "Expected '{'");
    std::vector<StmtPtr> stmts;
    while (!check(TokenKind::RBrace) && !check(TokenKind::Eof))
        stmts.push_back(parseStmt());
    expect(TokenKind::RBrace, "Expected '}'");
    return stmts;
}

// ── Expressions ───────────────────────────────────────────────────────────────
ExprPtr Parser::parseExpr()   { return parseAssign(); }

ExprPtr Parser::parseAssign() {
    if (check(TokenKind::Identifier) && peek(1).kind == TokenKind::Eq) {
        auto name = advance().lexeme;
        advance(); // consume =
        auto val = parseAssign();
        auto e = std::make_unique<AssignExpr>();
        e->name = name;
        e->value = std::move(val);
        return e;
    }
    return parseOr();
}

ExprPtr Parser::parseOr() {
    auto lhs = parseAnd();
    while (check(TokenKind::Wla)) {    // wla = or
        advance();
        auto rhs = parseAnd();
        auto e = std::make_unique<BinaryExpr>();
        e->op = "or"; e->lhs = std::move(lhs); e->rhs = std::move(rhs);
        lhs = std::move(e);
    }
    return lhs;
}

ExprPtr Parser::parseAnd() {
    auto lhs = parseEquality();
    while (check(TokenKind::W)) {      // w = and
        advance();
        auto rhs = parseEquality();
        auto e = std::make_unique<BinaryExpr>();
        e->op = "and"; e->lhs = std::move(lhs); e->rhs = std::move(rhs);
        lhs = std::move(e);
    }
    return lhs;
}

ExprPtr Parser::parseEquality() {
    auto lhs = parseComparison();
    while (check(TokenKind::EqEq) || check(TokenKind::BangEq)) {
        std::string op = advance().lexeme;
        auto rhs = parseComparison();
        auto e = std::make_unique<BinaryExpr>();
        e->op = op; e->lhs = std::move(lhs); e->rhs = std::move(rhs);
        lhs = std::move(e);
    }
    return lhs;
}

ExprPtr Parser::parseComparison() {
    auto lhs = parseTerm();
    while (check(TokenKind::Lt) || check(TokenKind::Gt) ||
           check(TokenKind::LtEq) || check(TokenKind::GtEq)) {
        std::string op = advance().lexeme;
        auto rhs = parseTerm();
        auto e = std::make_unique<BinaryExpr>();
        e->op = op; e->lhs = std::move(lhs); e->rhs = std::move(rhs);
        lhs = std::move(e);
    }
    return lhs;
}

ExprPtr Parser::parseTerm() {
    auto lhs = parseFactor();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        std::string op = advance().lexeme;
        auto rhs = parseFactor();
        auto e = std::make_unique<BinaryExpr>();
        e->op = op; e->lhs = std::move(lhs); e->rhs = std::move(rhs);
        lhs = std::move(e);
    }
    return lhs;
}

ExprPtr Parser::parseFactor() {
    auto lhs = parseUnary();
    while (check(TokenKind::Star) || check(TokenKind::Slash) || check(TokenKind::Percent)) {
        std::string op = advance().lexeme;
        auto rhs = parseUnary();
        auto e = std::make_unique<BinaryExpr>();
        e->op = op; e->lhs = std::move(lhs); e->rhs = std::move(rhs);
        lhs = std::move(e);
    }
    return lhs;
}

ExprPtr Parser::parseUnary() {
    if (check(TokenKind::Machi) || check(TokenKind::Minus)) {
        std::string op = advance().lexeme;
        auto e = std::make_unique<UnaryExpr>();
        e->op = op;
        e->operand = parseUnary();
        return e;
    }
    return parseCall();
}

ExprPtr Parser::parseCall() {
    auto callee = parsePrimary();
    if (auto* v = dynamic_cast<VarExpr*>(callee.get())) {
        if (check(TokenKind::LParen)) {
            advance();
            auto e = std::make_unique<CallExpr>();
            e->callee = v->name;
            if (!check(TokenKind::RParen)) {
                e->args.push_back(parseExpr());
                while (match(TokenKind::Comma))
                    e->args.push_back(parseExpr());
            }
            expect(TokenKind::RParen, "Expected ')'");
            return e;
        }
    }
    return callee;
}

ExprPtr Parser::parsePrimary() {
    if (check(TokenKind::Integer)) {
        auto e = std::make_unique<IntLitExpr>();
        e->value = std::stoll(advance().lexeme);
        return e;
    }
    if (check(TokenKind::Float)) {
        auto e = std::make_unique<FloatLitExpr>();
        e->value = std::stod(advance().lexeme);
        return e;
    }
    if (check(TokenKind::String)) {
        auto e = std::make_unique<StringLitExpr>();
        e->value = advance().lexeme;
        return e;
    }
    if (check(TokenKind::Sa7)) {
        advance();
        auto e = std::make_unique<BoolLitExpr>();
        e->value = true;
        return e;
    }
    if (check(TokenKind::Ghalt)) {
        advance();
        auto e = std::make_unique<BoolLitExpr>();
        e->value = false;
        return e;
    }
    if (check(TokenKind::Identifier)) {
        auto e = std::make_unique<VarExpr>();
        e->name = advance().lexeme;
        return e;
    }
    if (match(TokenKind::LParen)) {
        auto e = parseExpr();
        expect(TokenKind::RParen, "Expected ')'");
        return e;
    }
    throw std::runtime_error("Unexpected token '" + peek().lexeme + "' at line " + std::to_string(peek().line));
}
