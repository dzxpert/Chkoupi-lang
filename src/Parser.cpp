#include "Parser.h"
#include "Lexer.h"
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <set>
#include <filesystem>

static std::set<std::filesystem::path> resolvedImports;

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
        auto stmt = parseStmt();
        if (auto* imp = dynamic_cast<ImportStmt*>(stmt.get())) {
            auto importedStmts = resolveImport(imp->path);
            for (auto& s : importedStmts) {
                prog.stmts.push_back(std::move(s));
            }
        } else {
            prog.stmts.push_back(std::move(stmt));
        }
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
           k == TokenKind::Type7arf    || k == TokenKind::TypeNass    ||
           k == TokenKind::TypeJadwl;
}

std::string Parser::parseType() {
    if (check(TokenKind::TypeJadwl)) {
        advance(); // consume jadwl
        expect(TokenKind::Lt, "Expected '<' after jadwl type");
        std::string elemType = parseType();
        expect(TokenKind::Gt, "Expected '>' after element type");
        return "jadwl<" + elemType + ">";
    }
    if (isTypeToken(peek().kind)) {
        return tokenToType(advance().kind);
    }
    if (check(TokenKind::Identifier)) {
        return advance().lexeme;
    }
    throw std::runtime_error("Expected type name at line " + std::to_string(peek().line));
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
    if (check(TokenKind::A7bss))      return parseBreak();
    if (check(TokenKind::Kml))        return parseContinue();
    if (check(TokenKind::Dalla))      return parseFuncDecl(false);
    if (check(TokenKind::Qaleb))      return parseStructDecl();
    if (check(TokenKind::Kssr))       return parseFree();
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
        s->type = parseType();
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
// idha (cond) { ... } idha_mknch idha (cond2) { ... } idha_mknch { ... }
StmtPtr Parser::parseIf() {
    advance(); // consume idha
    expect(TokenKind::LParen, "Expected '(' after idha");
    auto s = std::make_unique<IfStmt>();
    s->condition = parseExpr();
    expect(TokenKind::RParen, "Expected ')'");
    s->thenBlock = parseBlock();
    if (match(TokenKind::Idha_mknch)) {
        if (check(TokenKind::Idha)) {
            // else-if chain: wrap nested if as a single statement in elseBlock
            s->elseBlock.push_back(parseIf());
        } else {
            s->elseBlock = parseBlock();
        }
    }
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

// a7bss;
StmtPtr Parser::parseBreak() {
    advance(); // consume a7bss
    expect(TokenKind::Semicolon, "Expected ';' after a7bss");
    return std::make_unique<BreakStmt>();
}

// kml;
StmtPtr Parser::parseContinue() {
    advance(); // consume kml
    expect(TokenKind::Semicolon, "Expected ';' after kml");
    return std::make_unique<ContinueStmt>();
}

// dalla name(p: tabi3i, ...) -> tabi3i { ... }
StmtPtr Parser::parseFuncDecl(bool isMethod) {
    advance(); // consume dalla
    auto s = std::make_unique<FuncDecl>();
    std::string name;
    if (check(TokenKind::Identifier)) {
        name = advance().lexeme;
    } else if (isMethod && check(TokenKind::Kssr)) {
        name = advance().lexeme;
    } else {
        throw std::runtime_error("Expected function name at line " + std::to_string(peek().line));
    }
    s->name = name;
    expect(TokenKind::LParen, "Expected '('");
    while (!check(TokenKind::RParen) && !check(TokenKind::Eof)) {
        FuncDecl::Param p;
        p.name = expect(TokenKind::Identifier, "Expected param name").lexeme;
        expect(TokenKind::Colon, "Expected ':' after param name");
        p.type = parseType();
        s->params.push_back(std::move(p));
        match(TokenKind::Comma);
    }
    expect(TokenKind::RParen, "Expected ')'");
    s->returnType = "";
    if (match(TokenKind::Arrow)) {
        s->returnType = parseType();
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

StmtPtr Parser::parseStructDecl() {
    advance(); // consume 9aleb
    auto s = std::make_unique<StructDeclStmt>();
    s->name = expect(TokenKind::Identifier, "Expected struct name").lexeme;
    expect(TokenKind::LBrace, "Expected '{' after struct name");
    
    while (!check(TokenKind::RBrace) && !check(TokenKind::Eof)) {
        if (check(TokenKind::Dalla)) {
            s->methods.push_back(parseFuncDecl(true));
        } else {
            expect(TokenKind::Dir, "Struct members must be fields ('dir') or methods ('dalla')");
            std::string fieldName = expect(TokenKind::Identifier, "Expected field name").lexeme;
            expect(TokenKind::Colon, "Expected ':' for field type annotation");
            std::string fieldType = parseType();
            
            ExprPtr defaultVal = nullptr;
            if (match(TokenKind::Eq)) {
                defaultVal = parseExpr();
            }
            expect(TokenKind::Semicolon, "Expected ';' after field declaration");
            s->fields.push_back({fieldName, fieldType, std::move(defaultVal)});
        }
    }
    
    expect(TokenKind::RBrace, "Expected '}' at end of struct declaration");
    return s;
}

StmtPtr Parser::parseFree() {
    advance(); // consume kssr
    auto s = std::make_unique<FreeStmt>();
    s->value = parseExpr();
    expect(TokenKind::Semicolon, "Expected ';' after kssr");
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

static std::string compoundToOp(TokenKind k) {
    switch (k) {
        case TokenKind::PlusEq:    return "+";
        case TokenKind::MinusEq:   return "-";
        case TokenKind::StarEq:    return "*";
        case TokenKind::SlashEq:   return "/";
        case TokenKind::PercentEq: return "%";
        default:                   return "";
    }
}

static bool isCompoundAssign(TokenKind k) {
    return k == TokenKind::PlusEq  || k == TokenKind::MinusEq ||
           k == TokenKind::StarEq  || k == TokenKind::SlashEq ||
           k == TokenKind::PercentEq;
}

ExprPtr Parser::parseAssign() {
    if (check(TokenKind::Identifier)) {
        if (peek(1).kind == TokenKind::Eq) {
            auto name = advance().lexeme;
            advance(); // consume =
            auto val = parseAssign();
            auto e = std::make_unique<AssignExpr>();
            e->name = name;
            e->value = std::move(val);
            return e;
        }
        if (isCompoundAssign(peek(1).kind)) {
            auto name = advance().lexeme;
            auto op = compoundToOp(advance().kind);
            auto rhs = parseAssign();
            // Build: x = x <op> rhs
            auto varRef = std::make_unique<VarExpr>();
            varRef->name = name;
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = op;
            bin->lhs = std::move(varRef);
            bin->rhs = std::move(rhs);
            auto e = std::make_unique<AssignExpr>();
            e->name = name;
            e->value = std::move(bin);
            return e;
        }
    }

    auto lhs = parseOr();
    if (match(TokenKind::Eq)) {
        auto rhs = parseAssign();
        if (auto* v = dynamic_cast<VarExpr*>(lhs.get())) {
            auto e = std::make_unique<AssignExpr>();
            e->name = v->name;
            e->value = std::move(rhs);
            return e;
        } else if (auto* idx = dynamic_cast<IndexExpr*>(lhs.get())) {
            auto e = std::make_unique<IndexAssignExpr>();
            e->target = std::move(idx->target);
            e->index = std::move(idx->index);
            e->value = std::move(rhs);
            return e;
        } else if (auto* mem = dynamic_cast<MemberExpr*>(lhs.get())) {
            auto e = std::make_unique<MemberAssignExpr>();
            e->target = std::move(mem->target);
            e->fieldName = mem->fieldName;
            e->value = std::move(rhs);
            return e;
        }
        throw std::runtime_error("Invalid assignment target");
    }
    return lhs;
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
    auto expr = parsePrimary();
    while (true) {
        if (check(TokenKind::LParen)) {
            auto* v = dynamic_cast<VarExpr*>(expr.get());
            if (!v) break;
            advance(); // consume (
            auto e = std::make_unique<CallExpr>();
            e->callee = v->name;
            if (!check(TokenKind::RParen)) {
                e->args.push_back(parseExpr());
                while (match(TokenKind::Comma))
                    e->args.push_back(parseExpr());
            }
            expect(TokenKind::RParen, "Expected ')'");
            expr = std::move(e);
        } else if (match(TokenKind::LBracket)) {
            auto indexExpr = parseExpr();
            expect(TokenKind::RBracket, "Expected ']'");
            auto idx = std::make_unique<IndexExpr>();
            idx->target = std::move(expr);
            idx->index = std::move(indexExpr);
            expr = std::move(idx);
        } else if (match(TokenKind::Dot)) {
            std::string fieldName;
            bool isMethodCall = false;
            if (check(TokenKind::Identifier)) {
                fieldName = advance().lexeme;
                if (check(TokenKind::LParen)) {
                    isMethodCall = true;
                }
            } else if (check(TokenKind::Kssr)) {
                if (peek(1).kind == TokenKind::LParen) {
                    fieldName = advance().lexeme;
                    isMethodCall = true;
                } else {
                    throw std::runtime_error("Expected field or method name after '.'");
                }
            } else {
                throw std::runtime_error("Expected field or method name after '.'");
            }

            if (isMethodCall) {
                advance(); // consume (
                auto mc = std::make_unique<MethodCallExpr>();
                mc->target = std::move(expr);
                mc->methodName = fieldName;
                if (!check(TokenKind::RParen)) {
                    mc->args.push_back(parseExpr());
                    while (match(TokenKind::Comma))
                        mc->args.push_back(parseExpr());
                }
                expect(TokenKind::RParen, "Expected ')' after method arguments");
                expr = std::move(mc);
            } else {
                auto mem = std::make_unique<MemberExpr>();
                mem->target = std::move(expr);
                mem->fieldName = fieldName;
                expr = std::move(mem);
            }
        } else if (check(TokenKind::PlusPlus) || check(TokenKind::MinusMinus)) {
            auto* v = dynamic_cast<VarExpr*>(expr.get());
            if (!v) {
                throw std::runtime_error("Increment/decrement operand must be a variable");
            }
            std::string op = (advance().kind == TokenKind::PlusPlus) ? "+" : "-";
            auto varRef = std::make_unique<VarExpr>();
            varRef->name = v->name;
            auto one = std::make_unique<IntLitExpr>();
            one->value = 1;
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = op;
            bin->lhs = std::move(varRef);
            bin->rhs = std::move(one);
            auto e = std::make_unique<AssignExpr>();
            e->name = v->name;
            e->value = std::move(bin);
            expr = std::move(e);
            break; // Increment is assignment and is not chainable
        } else {
            break;
        }
    }
    return expr;
}

ExprPtr Parser::parsePrimary() {
    if (match(TokenKind::LBracket)) {
        auto e = std::make_unique<ArrayLitExpr>();
        if (!check(TokenKind::RBracket)) {
            e->elements.push_back(parseExpr());
            while (match(TokenKind::Comma)) {
                e->elements.push_back(parseExpr());
            }
        }
        expect(TokenKind::RBracket, "Expected ']'");
        return e;
    }
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
    if (check(TokenKind::Char)) {
        auto e = std::make_unique<CharLitExpr>();
        e->value = advance().lexeme[0];
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
    if (check(TokenKind::Had)) {
        advance();
        auto e = std::make_unique<VarExpr>();
        e->name = "had";
        return e;
    }
    if (check(TokenKind::Identifier)) {
        if (peek(1).kind == TokenKind::LBrace) {
            auto nameToken = advance(); // consume identifier
            advance(); // consume {
            auto e = std::make_unique<StructLitExpr>();
            e->structName = nameToken.lexeme;
            if (!check(TokenKind::RBrace)) {
                while (true) {
                    std::string fieldName = expect(TokenKind::Identifier, "Expected field name in struct literal").lexeme;
                    expect(TokenKind::Colon, "Expected ':' after field name in struct literal");
                    auto fieldValue = parseExpr();
                    e->initializers.push_back({fieldName, std::move(fieldValue)});
                    if (!match(TokenKind::Comma)) break;
                    if (check(TokenKind::RBrace)) break; // allow trailing comma
                }
            }
            expect(TokenKind::RBrace, "Expected '}' at the end of struct literal");
            return e;
        }
        
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

std::vector<StmtPtr> Parser::resolveImport(const std::string& path) {
    if (path == "math") {
        static bool mathImported = false;
        if (mathImported) return {}; // already imported once
        mathImported = true;

        std::string mathSource = 
            "dalla jdr(x: 3ouchri) -> 3ouchri {\n"
            "    raja3 sqrt(x);\n"
            "}\n"
            "dalla qwa(x: 3ouchri, y: 3ouchri) -> 3ouchri {\n"
            "    raja3 pow(x, y);\n"
            "}\n";
        
        Lexer lex(mathSource);
        Parser p(lex.tokenize());
        p.currentDir = currentDir;
        return p.parse().stmts;
    }

    // Custom filesystem import
    std::filesystem::path fullPath = std::filesystem::path(currentDir) / path;
    if (fullPath.extension() != ".dz") {
        fullPath.replace_extension(".dz");
    }

    // Try current directory as well if fullPath is not found
    if (!std::filesystem::exists(fullPath)) {
        std::filesystem::path localPath = std::filesystem::path(path);
        if (localPath.extension() != ".dz") {
            localPath.replace_extension(".dz");
        }
        if (std::filesystem::exists(localPath)) {
            fullPath = localPath;
        }
    }

    std::filesystem::path canonPath;
    try {
        canonPath = std::filesystem::canonical(fullPath);
    } catch (...) {
        throw std::runtime_error("jibli error: Cannot find imported file '" + path + "'");
    }

    if (resolvedImports.count(canonPath)) {
        return {}; // already imported, skip to prevent circular/duplicate definitions
    }
    resolvedImports.insert(canonPath);

    std::ifstream file(canonPath);
    if (!file.is_open()) {
        throw std::runtime_error("jibli error: Cannot open imported file '" + canonPath.string() + "'");
    }

    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Lexer lexer(source);
    Parser subParser(lexer.tokenize());
    subParser.currentDir = canonPath.parent_path().string();
    return subParser.parse().stmts;
}
