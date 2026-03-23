#include "Lexer.h"
#include "Parser.h"
#include "Codegen.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "Cannot open file: " << path << "\n"; std::exit(1); }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: chkoupi <source.chk> [--emit-ir] [--emit-obj <out.o>]\n";
        return 1;
    }

    std::string srcPath = argv[1];
    bool emitIR = false;
    std::string objPath;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--emit-ir")   emitIR = true;
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

        // 3. Codegen
        Codegen codegen;
        codegen.generate(program);

        // 4. Output
        if (emitIR)          codegen.dumpIR();
        if (!objPath.empty()) codegen.writeObjectFile(objPath);

    } catch (const std::exception& e) {
        std::cerr << "[Chkoupi Error] " << e.what() << "\n";
        return 1;
    }

    return 0;
}
