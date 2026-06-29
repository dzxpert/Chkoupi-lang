#include "Sema.h"
#include <stdexcept>

static void error(const std::string& msg) {
    throw std::runtime_error("[sema] " + msg);
}

static bool isCompatibleType(const std::string& expected, const std::string& actual) {
    if (expected == actual) return true;
    if (expected == "float" && actual == "int") return true;
    if (expected == "string" && actual == "string_literal") return true;
    return false;
}

void Sema::pushScope() {
    scopes.emplace_back();
}

void Sema::popScope() {
    scopes.pop_back();
}

void Sema::declareVar(const std::string& name, const std::string& type, bool isConst) {
    if (!scopes.empty() && scopes.back().count(name))
        error("Variable '" + name + "' already declared in this scope");
    scopes.back()[name] = {type, isConst};
}

Sema::VarInfo* Sema::lookupVar(const std::string& name) {
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
        auto it = scopes[i].find(name);
        if (it != scopes[i].end()) return &it->second;
    }
    return nullptr;
}

// ── Top-level ────────────────────────────────────────────────────────────────
void Sema::analyze(const Program& prog) {
    // Register built-in functions
    functions["printf"] = {-1, "int"};   // varargs
    functions["scanf"]  = {-1, "int"};
    functions["tool"]   = {1, "int"};
    functions["sqrt"]   = {1, "float"};
    functions["pow"]    = {2, "float"};

    pushScope(); // global scope

    // First pass: register all struct types and fields
    for (auto& s : prog.stmts) {
        if (auto* sd = dynamic_cast<const StructDeclStmt*>(s.get())) {
            if (structs.count(sd->name)) {
                error("Struct '" + sd->name + "' already declared");
            }
            StructInfo info;
            for (const auto& field : sd->fields) {
                if (info.fields.count(field.name)) {
                    error("Field '" + field.name + "' already declared in struct '" + sd->name + "'");
                }
                info.fields[field.name] = field.type;
                if (field.defaultVal) {
                    info.fieldsWithDefaults.insert(field.name);
                }
            }
            structs[sd->name] = info;
        }
    }

    // Second pass: register all function and method declarations
    for (auto& s : prog.stmts) {
        if (auto* f = dynamic_cast<const FuncDecl*>(s.get())) {
            functions[f->name] = {(int)f->params.size(), f->returnType};
        } else if (auto* sd = dynamic_cast<const StructDeclStmt*>(s.get())) {
            for (auto& mStmt : sd->methods) {
                if (auto* fd = dynamic_cast<const FuncDecl*>(mStmt.get())) {
                    std::string mangledName = sd->name + "." + fd->name;
                    if (functions.count(mangledName)) {
                        error("Method '" + fd->name + "' already declared in struct '" + sd->name + "'");
                    }
                    functions[mangledName] = {(int)fd->params.size() + 1, fd->returnType};
                }
            }
        }
    }

    // Third pass: check everything
    for (auto& s : prog.stmts)
        checkStmt(*s);

    popScope();
}

// ── Statements ───────────────────────────────────────────────────────────────
void Sema::checkStmt(const Stmt& stmt) {
    if (auto* s = dynamic_cast<const VarDeclStmt*>(&stmt))    { checkVarDecl(*s); return; }
    if (auto* s = dynamic_cast<const PrintStmt*>(&stmt))      { checkPrint(*s);   return; }
    if (auto* s = dynamic_cast<const ReadStmt*>(&stmt))       { checkRead(*s);    return; }
    if (auto* s = dynamic_cast<const IfStmt*>(&stmt))         { checkIf(*s);      return; }
    if (auto* s = dynamic_cast<const WhileStmt*>(&stmt))      { checkWhile(*s);   return; }
    if (auto* s = dynamic_cast<const ForStmt*>(&stmt))        { checkFor(*s);     return; }
    if (auto* s = dynamic_cast<const SwitchStmt*>(&stmt))     { checkSwitch(*s);  return; }
    if (auto* s = dynamic_cast<const TryCatchStmt*>(&stmt))   { checkTryCatch(*s); return; }
    if (dynamic_cast<const ImportStmt*>(&stmt))               { return; }
    if (auto* s = dynamic_cast<const ReturnStmt*>(&stmt))     { checkReturn(*s);  return; }
    if (auto* s = dynamic_cast<const FuncDecl*>(&stmt))       { checkFunc(*s);    return; }
    if (auto* s = dynamic_cast<const StructDeclStmt*>(&stmt)) { checkStructDecl(*s); return; }
    if (auto* s = dynamic_cast<const FreeStmt*>(&stmt))       { checkFree(*s);    return; }
    if (auto* s = dynamic_cast<const ExprStmt*>(&stmt))       { checkExpr(*s->expr); return; }
    if (dynamic_cast<const BreakStmt*>(&stmt)) {
        if (loopDepth == 0) error("a7bss used outside of a loop");
        return;
    }
    if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        if (loopDepth == 0) error("kml used outside of a loop");
        return;
    }
}

void Sema::checkBlock(const std::vector<StmtPtr>& block) {
    pushScope();
    for (auto& s : block) checkStmt(*s);
    popScope();
}

void Sema::checkVarDecl(const VarDeclStmt& s) {
    checkExpr(*s.init);
    std::string type = s.type.empty() ? inferType(*s.init) : s.type;
    declareVar(s.name, type, s.isConst);
}

void Sema::checkPrint(const PrintStmt& s) {
    for (auto& a : s.args) checkExpr(*a);
}

void Sema::checkRead(const ReadStmt& s) {
    if (!lookupVar(s.varName))
        error("Undefined variable '" + s.varName + "' in a9ra");
}

void Sema::checkIf(const IfStmt& s) {
    checkExpr(*s.condition);
    checkBlock(s.thenBlock);
    if (!s.elseBlock.empty())
        checkBlock(s.elseBlock);
}

void Sema::checkWhile(const WhileStmt& s) {
    checkExpr(*s.condition);
    ++loopDepth;
    checkBlock(s.body);
    --loopDepth;
}

void Sema::checkFor(const ForStmt& s) {
    pushScope(); // for-init scope
    checkStmt(*s.init);
    checkExpr(*s.condition);
    checkExpr(*s.update);
    ++loopDepth;
    checkBlock(s.body);
    --loopDepth;
    popScope();
}

void Sema::checkSwitch(const SwitchStmt& s) {
    checkExpr(*s.expr);
    for (auto& c : s.cases) {
        checkExpr(*c.value);
        checkBlock(c.body);
    }
}

void Sema::checkTryCatch(const TryCatchStmt& s) {
    checkBlock(s.tryBlock);
    checkBlock(s.catchBlock);
}

void Sema::checkReturn(const ReturnStmt& s) {
    if (functionDepth == 0)
        error("raja3 used outside of a function");
    if (s.value) checkExpr(*s.value);
}

void Sema::checkFunc(const FuncDecl& s) {
    ++functionDepth;
    pushScope();
    for (auto& p : s.params)
        declareVar(p.name, p.type, false);
    for (auto& st : s.body)
        checkStmt(*st);
    popScope();
    --functionDepth;
}

void Sema::checkStructDecl(const StructDeclStmt& s) {
    for (const auto& field : s.fields) {
        std::string t = field.type;
        if (t != "int" && t != "float" && t != "bool" && t != "char" && t != "string" && t.rfind("jadwl<", 0) != 0) {
            if (!structs.count(t)) {
                error("Unknown type '" + t + "' for field '" + field.name + "' in struct '" + s.name + "'");
            }
        }
        if (field.defaultVal) {
            checkExpr(*field.defaultVal);
            std::string initType = inferType(*field.defaultVal);
            if (!isCompatibleType(t, initType)) {
                error("Default value type '" + initType + "' does not match field type '" + t + "' in struct '" + s.name + "'");
            }
        }
    }

    for (auto& mStmt : s.methods) {
        if (auto* fd = dynamic_cast<const FuncDecl*>(mStmt.get())) {
            ++functionDepth;
            pushScope();
            declareVar("had", s.name, false);
            for (auto& p : fd->params)
                declareVar(p.name, p.type, false);
            for (auto& st : fd->body)
                checkStmt(*st);
            popScope();
            --functionDepth;
        }
    }
}

void Sema::checkFree(const FreeStmt& s) {
    checkExpr(*s.value);
    std::string type = inferType(*s.value);
    if (type == "string_literal") {
        error("Cannot kssr a non-owned string (literal or non-owned string variable)");
    }
    if (type != "string" && type.rfind("jadwl<", 0) != 0 && !structs.count(type)) {
        error("Cannot kssr value of non-heap type '" + type + "'");
    }
    if (structs.count(type)) {
        std::string destructorName = type + ".kssr";
        auto it = functions.find(destructorName);
        if (it != functions.end()) {
            if (it->second.paramCount != 1) {
                error("Destructor 'kssr' of struct '" + type + "' must not have any parameters");
            }
            if (it->second.returnType != "void" && it->second.returnType != "") {
                error("Destructor 'kssr' of struct '" + type + "' must not return a value");
            }
        }
    }
}

// ── Expressions ──────────────────────────────────────────────────────────────
void Sema::checkExpr(const Expr& expr) {
    if (dynamic_cast<const IntLitExpr*>(&expr))    return;
    if (dynamic_cast<const FloatLitExpr*>(&expr))  return;
    if (dynamic_cast<const StringLitExpr*>(&expr)) return;
    if (dynamic_cast<const BoolLitExpr*>(&expr))   return;
    if (dynamic_cast<const CharLitExpr*>(&expr))   return;

    if (auto* e = dynamic_cast<const ArrayLitExpr*>(&expr)) {
        for (auto& elem : e->elements) checkExpr(*elem);
        return;
    }
    if (auto* e = dynamic_cast<const IndexExpr*>(&expr)) {
        checkExpr(*e->target);
        checkExpr(*e->index);
        return;
    }
    if (auto* e = dynamic_cast<const IndexAssignExpr*>(&expr)) {
        checkExpr(*e->target);
        checkExpr(*e->index);
        checkExpr(*e->value);
        return;
    }

    if (auto* e = dynamic_cast<const VarExpr*>(&expr)) {
        if (!lookupVar(e->name))
            error("Undefined variable '" + e->name + "'");
        return;
    }
    if (auto* e = dynamic_cast<const BinaryExpr*>(&expr)) {
        checkExpr(*e->lhs);
        checkExpr(*e->rhs);
        return;
    }
    if (auto* e = dynamic_cast<const UnaryExpr*>(&expr)) {
        checkExpr(*e->operand);
        return;
    }
    if (auto* e = dynamic_cast<const CallExpr*>(&expr)) {
        auto it = functions.find(e->callee);
        if (it == functions.end())
            error("Undefined function '" + e->callee + "'");
        if (it->second.paramCount >= 0 && (int)e->args.size() != it->second.paramCount)
            error("Function '" + e->callee + "' expects " +
                  std::to_string(it->second.paramCount) + " arguments, got " +
                  std::to_string(e->args.size()));
        for (auto& a : e->args) checkExpr(*a);
        return;
    }
    if (auto* e = dynamic_cast<const AssignExpr*>(&expr)) {
        auto* info = lookupVar(e->name);
        if (!info)
            error("Undefined variable '" + e->name + "'");
        if (info->isConst)
            error("Cannot assign to constant '" + e->name + "'");
        checkExpr(*e->value);
        return;
    }
    if (auto* e = dynamic_cast<const StructLitExpr*>(&expr)) {
        if (!structs.count(e->structName)) {
            error("Unknown struct type '" + e->structName + "'");
        }
        const auto& info = structs[e->structName];
        std::set<std::string> initializedFields;
        for (const auto& init : e->initializers) {
            if (!info.fields.count(init.first)) {
                error("Struct '" + e->structName + "' has no field named '" + init.first + "'");
            }
            checkExpr(*init.second);
            if (initializedFields.count(init.first)) {
                error("Duplicate initializer for field '" + init.first + "' in struct '" + e->structName + "'");
            }
            initializedFields.insert(init.first);
            
            std::string initType = inferType(*init.second);
            std::string fieldType = info.fields.at(init.first);
            if (!isCompatibleType(fieldType, initType)) {
                error("Initializer type '" + initType + "' does not match field type '" + fieldType + "' for field '" + init.first + "' in struct '" + e->structName + "'");
            }
        }
        
        for (const auto& field : info.fields) {
            if (!initializedFields.count(field.first) && !info.fieldsWithDefaults.count(field.first)) {
                error("Field '" + field.first + "' of struct '" + e->structName + "' is not initialized and has no default value");
            }
        }
        return;
    }
    if (auto* e = dynamic_cast<const MemberExpr*>(&expr)) {
        checkExpr(*e->target);
        std::string targetType = inferType(*e->target);
        if (!structs.count(targetType)) {
            error("Cannot access field '" + e->fieldName + "' on non-struct type '" + targetType + "'");
        }
        const auto& info = structs[targetType];
        if (!info.fields.count(e->fieldName)) {
            error("Struct '" + targetType + "' has no field named '" + e->fieldName + "'");
        }
        return;
    }
    if (auto* e = dynamic_cast<const MemberAssignExpr*>(&expr)) {
        checkExpr(*e->target);
        std::string targetType = inferType(*e->target);
        if (!structs.count(targetType)) {
            error("Cannot assign field '" + e->fieldName + "' on non-struct type '" + targetType + "'");
        }
        const auto& info = structs[targetType];
        if (!info.fields.count(e->fieldName)) {
            error("Struct '" + targetType + "' has no field named '" + e->fieldName + "'");
        }
        checkExpr(*e->value);
        
        std::string valType = inferType(*e->value);
        std::string fieldType = info.fields.at(e->fieldName);
        if (valType != fieldType && !(fieldType == "float" && valType == "int")) {
            error("Cannot assign value of type '" + valType + "' to field '" + e->fieldName + "' of type '" + fieldType + "'");
        }
        return;
    }
    if (auto* e = dynamic_cast<const MethodCallExpr*>(&expr)) {
        checkExpr(*e->target);
        std::string targetType = inferType(*e->target);
        if (!structs.count(targetType)) {
            error("Cannot call method '" + e->methodName + "' on non-struct type '" + targetType + "'");
        }
        std::string mangledName = targetType + "." + e->methodName;
        auto it = functions.find(mangledName);
        if (it == functions.end()) {
            error("Struct '" + targetType + "' has no method named '" + e->methodName + "'");
        }
        int expectedParams = it->second.paramCount - 1;
        if ((int)e->args.size() != expectedParams) {
            error("Method '" + e->methodName + "' of struct '" + targetType + "' expects " +
                  std::to_string(expectedParams) + " arguments, got " + std::to_string(e->args.size()));
        }
        for (auto& a : e->args) checkExpr(*a);
        return;
    }
}

std::string Sema::inferType(const Expr& expr) {
    if (dynamic_cast<const IntLitExpr*>(&expr))    return "int";
    if (dynamic_cast<const FloatLitExpr*>(&expr))  return "float";
    if (dynamic_cast<const BoolLitExpr*>(&expr))   return "bool";
    if (dynamic_cast<const CharLitExpr*>(&expr))   return "char";
    if (dynamic_cast<const StringLitExpr*>(&expr)) return "string_literal";
    if (auto* e = dynamic_cast<const VarExpr*>(&expr)) {
        auto* info = lookupVar(e->name);
        if (info) return info->type;
        return "int";
    }
    if (auto* e = dynamic_cast<const CallExpr*>(&expr)) {
        auto it = functions.find(e->callee);
        if (it != functions.end()) return it->second.returnType;
        return "int";
    }
    if (auto* e = dynamic_cast<const IndexExpr*>(&expr)) {
        std::string targetType = inferType(*e->target);
        if (targetType.rfind("jadwl<", 0) == 0) {
            return targetType.substr(6, targetType.size() - 7);
        }
        return "int";
    }
    if (auto* e = dynamic_cast<const ArrayLitExpr*>(&expr)) {
        if (e->elements.empty()) return "jadwl<int>";
        return "jadwl<" + inferType(*e->elements[0]) + ">";
    }
    if (auto* e = dynamic_cast<const BinaryExpr*>(&expr)) {
        std::string lhsType = inferType(*e->lhs);
        std::string rhsType = inferType(*e->rhs);
        if ((lhsType == "string" || lhsType == "string_literal") &&
            (rhsType == "string" || rhsType == "string_literal") &&
            e->op == "+") {
            return "string";
        }
        return lhsType;
    }
    if (auto* e = dynamic_cast<const UnaryExpr*>(&expr)) {
        return inferType(*e->operand);
    }
    if (auto* e = dynamic_cast<const StructLitExpr*>(&expr)) {
        return e->structName;
    }
    if (auto* e = dynamic_cast<const MemberExpr*>(&expr)) {
        std::string targetType = inferType(*e->target);
        if (structs.count(targetType)) {
            const auto& info = structs[targetType];
            auto it = info.fields.find(e->fieldName);
            if (it != info.fields.end()) return it->second;
        }
        return "int";
    }
    if (auto* e = dynamic_cast<const MethodCallExpr*>(&expr)) {
        std::string targetType = inferType(*e->target);
        std::string mangledName = targetType + "." + e->methodName;
        auto it = functions.find(mangledName);
        if (it != functions.end()) return it->second.returnType;
        return "int";
    }
    return "int";
}
