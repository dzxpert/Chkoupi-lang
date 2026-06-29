#include "Lexer.h"
#include "Parser.h"
#include "Sema.h"
#include "Codegen.h"

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "[chkoupi] ma9drtech n7ell fichier: " << path << "\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <source.dz> [--emit-ir] [--emit-obj <out.o>]\n"
              << "  No flags  : compile + run (JIT)\n"
              << "  --emit-ir : print LLVM IR to stdout\n"
              << "  --emit-obj: compile to object file\n";
}

static std::string escapeStringLiteral(const std::string& s) {
    std::string res = "\"";
    for (char c : s) {
        if (c == '"') res += "\\\"";
        else if (c == '\\') res += "\\\\";
        else if (c == '\n') res += "\\n";
        else if (c == '\t') res += "\\t";
        else res += c;
    }
    res += "\"";
    return res;
}

static std::string escapeCharLiteral(const std::string& s) {
    if (s.empty()) return "''";
    char c = s[0];
    std::string res = "'";
    if (c == '\'') res += "\\'";
    else if (c == '\\') res += "\\\\";
    else if (c == '\n') res += "\\n";
    else if (c == '\t') res += "\\t";
    else if (c == '\0') res += "\\0";
    else res += c;
    res += "'";
    return res;
}

static std::string serializeTokens(const std::vector<Token>& tokens, size_t start, size_t end) {
    std::string res = "";
    for (size_t i = start; i <= end; ++i) {
        if (tokens[i].kind == TokenKind::String) {
            res += escapeStringLiteral(tokens[i].lexeme);
        } else if (tokens[i].kind == TokenKind::Char) {
            res += escapeCharLiteral(tokens[i].lexeme);
        } else {
            res += tokens[i].lexeme;
        }
        if (i < end) {
            res += " ";
        }
    }
    return res + "\n";
}

static void runREPL() {
    std::cout << "Chkoupi-lang Interactive REPL\n";
    std::cout << "Type 'khroj;' to exit.\n";
    std::cout << "=========================================\n";

    std::string accumulatedCode = "";
    std::string multiLineBuffer = "";
    int openBraces = 0;
    bool inString = false;
    bool inChar = false;
    bool inBlockComment = false;

    while (true) {
        if (openBraces > 0 || inString || inChar || inBlockComment) {
            std::cout << "...      ";
        } else {
            std::cout << "chkoupi > ";
        }
        std::flush(std::cout);

        std::string line;
        if (!std::getline(std::cin, line)) {
            break;
        }

        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
        trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

        if (openBraces == 0 && !inString && !inChar && !inBlockComment && (trimmed == "khroj;" || trimmed == "exit;")) {
            break;
        }

        multiLineBuffer += line + "\n";

        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (inBlockComment) {
                if (c == '*' && i + 1 < line.size() && line[i+1] == '/') {
                    inBlockComment = false;
                    i++; // consume '/'
                }
            } else if (inString) {
                if (c == '\\' && i + 1 < line.size()) {
                    i++; // skip escaped char
                } else if (c == '"') {
                    inString = false;
                }
            } else if (inChar) {
                if (c == '\\' && i + 1 < line.size()) {
                    i++; // skip escaped char
                } else if (c == '\'') {
                    inChar = false;
                }
            } else {
                if (c == '/' && i + 1 < line.size() && line[i+1] == '/') {
                    // Line comment: ignores the rest of the line
                    break;
                } else if (c == '/' && i + 1 < line.size() && line[i+1] == '*') {
                    inBlockComment = true;
                    i++; // consume '*'
                } else if (c == '"') {
                    inString = true;
                } else if (c == '\'') {
                    inChar = true;
                } else if (c == '{') {
                    openBraces++;
                } else if (c == '}') {
                    openBraces--;
                }
            }
        }
        if (openBraces < 0) openBraces = 0;

        if (openBraces > 0 || inString || inChar || inBlockComment) {
            continue;
        }

        std::string candidateCode = accumulatedCode + multiLineBuffer;

        try {
            Lexer lexer(candidateCode);
            auto tokens = lexer.tokenize();

            Parser parser(std::move(tokens));
            parser.currentDir = std::filesystem::current_path().string();
            auto program = parser.parse();

            Sema sema;
            sema.analyze(program);

            Codegen codegen;
            codegen.generate(program);

            codegen.runJIT(false);

            // Extract only the new definitions (dalla / 9aleb / dir / dima) from multiLineBuffer
            Lexer inputLexer(multiLineBuffer);
            auto inputTokens = inputLexer.tokenize();
            std::string extractedDefs = "";
            int outerBraceDepth = 0;
            for (size_t i = 0; i < inputTokens.size(); ++i) {
                if (inputTokens[i].kind == TokenKind::LBrace) {
                    outerBraceDepth++;
                } else if (inputTokens[i].kind == TokenKind::RBrace) {
                    outerBraceDepth--;
                }

                if (outerBraceDepth == 0) {
                    if (inputTokens[i].kind == TokenKind::Dalla || inputTokens[i].kind == TokenKind::Qaleb) {
                        size_t start = i;
                        int braceDepth = 0;
                        size_t end = i;
                        for (size_t j = i; j < inputTokens.size(); ++j) {
                            if (inputTokens[j].kind == TokenKind::LBrace) {
                                braceDepth++;
                            } else if (inputTokens[j].kind == TokenKind::RBrace) {
                                braceDepth--;
                                if (braceDepth == 0) {
                                    end = j;
                                    i = j; // skip forward in outer loop
                                    break;
                                }
                            }
                        }
                        extractedDefs += serializeTokens(inputTokens, start, end);
                    } else if (inputTokens[i].kind == TokenKind::Dir || inputTokens[i].kind == TokenKind::Dima) {
                        size_t start = i;
                        size_t end = i;
                        for (size_t j = i; j < inputTokens.size(); ++j) {
                            if (inputTokens[j].kind == TokenKind::Semicolon) {
                                end = j;
                                i = j; // skip forward in outer loop
                                break;
                            }
                        }
                        extractedDefs += serializeTokens(inputTokens, start, end);
                    }
                }
            }
            accumulatedCode += extractedDefs;
            multiLineBuffer = "";
        } catch (const std::exception& e) {
            std::cerr << "[chkoupi khta9] " << e.what() << "\n";
            multiLineBuffer = "";
            openBraces = 0;
            inString = false;
            inChar = false;
            inBlockComment = false;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        runREPL();
        return 0;
    }

    std::string srcPath = argv[1];
    bool emitIR  = false;
    std::string objPath;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--emit-ir")  emitIR = true;
        if (arg == "--emit-obj" && i + 1 < argc) objPath = argv[++i];
    }

    try {
        std::string src = readFile(srcPath);

        // 1. Lex
        Lexer lexer(src);
        auto tokens = lexer.tokenize();

        // 2. Parse
        Parser parser(std::move(tokens));
        parser.currentDir = std::filesystem::path(srcPath).parent_path().string();
        auto program = parser.parse();

        // 3. Semantic analysis
        Sema sema;
        sema.analyze(program);

        // 4. Codegen
        Codegen codegen;
        codegen.generate(program);

        // 5. Emit modes
        if (emitIR) {
            codegen.dumpIR();
            return 0;
        }

        if (!objPath.empty()) {
            codegen.writeObjectFile(objPath);
            std::cout << "[chkoupi] ktbt object file: " << objPath << "\n";
            return 0;
        }

        // 6. Default mode: JIT execute
        codegen.runJIT();

    } catch (const std::exception& e) {
        std::cerr << "[chkoupi khta9] " << e.what() << "\n";
        return 1;
    }

    return 0;
}
