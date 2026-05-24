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

int main(int argc, char** argv) {
    if (argc < 2) { printUsage(argv[0]); return 1; }

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
